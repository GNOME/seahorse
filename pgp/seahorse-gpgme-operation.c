/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "seahorse-util.h"

#include "pgp/seahorse-gpgme.h"
#include "pgp/seahorse-gpgme-operation.h"
#include "pgp/seahorse-gpgme-source.h"

#define DEBUG_FLAG SEAHORSE_DEBUG_OPERATION
#include "seahorse-debug.h"

/* -----------------------------------------------------------------------------
 * DEFINITIONS
 */
 
typedef struct _WatchData {
    SeahorseGpgmeOperation *op;   /* The operation we're working with */
    gint stag;                  /* IO watch source tag */
    gboolean registered;        /* Whether this watch is currently registered */

    /* GPGME watch info */
    int fd;
    int dir;
    gpgme_io_cb_t fnc;
    void *fnc_data;
    
} WatchData;

/* Macros "borrowed" from GDK */
#define READ_CONDITION (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define WRITE_CONDITION (G_IO_OUT | G_IO_ERR)

typedef struct _SeahorseGpgmeOperationPrivate {
    gpgme_ctx_t gctx;               /* The context we watch for the async op to complete */
    gchar *message;                 /* A progress message to display (or NULL for GPGME messages) */
    struct gpgme_io_cbs io_cbs;     /* The GPGME IO callback vtable */
    GList *watches;                 /* Watches GPGME asked us to track */
    gboolean busy;                  /* If the context is currently executing something */
    guint def_total;                /* Default total */
} SeahorseGpgmeOperationPrivate;

enum {
    PROP_0,
    PROP_GCTX,
    PROP_MESSAGE,
    PROP_DEF_TOTAL
};

enum {
    RESULTS,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

#define SEAHORSE_GPGME_OPERATION_GET_PRIVATE(obj)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SEAHORSE_TYPE_GPGME_OPERATION, SeahorseGpgmeOperationPrivate))

/* TODO: This is just nasty. Gotta get rid of these weird macros */
IMPLEMENT_OPERATION_PROPS(Gpgme, gpgme)

    g_object_class_install_property (gobject_class, PROP_GCTX,
        g_param_spec_pointer ("gctx", "GPGME Context", "GPGME Context that this operation is watching.", 
                              G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_MESSAGE,
        g_param_spec_string ("message", "Progress Message", "Progress message that overrides whatever GPGME gives us", 
                             NULL, G_PARAM_READABLE | G_PARAM_WRITABLE));   

    g_object_class_install_property (gobject_class, PROP_DEF_TOTAL,
        g_param_spec_uint ("default-total", "Default Total", "Default total to use instead of GPGME's progress total.",
                           0, G_MAXUINT, 0, G_PARAM_READABLE | G_PARAM_WRITABLE));

    signals[RESULTS] = g_signal_new ("results", SEAHORSE_TYPE_GPGME_OPERATION, 
                G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SeahorseGpgmeOperationClass, results),
                NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    g_type_class_add_private (gobject_class, sizeof (SeahorseGpgmeOperationPrivate));

END_IMPLEMENT_OPERATION_PROPS


/* -----------------------------------------------------------------------------
 * HELPERS
 */

/* Called when there's data for GPG on a file descriptor */
static gboolean
io_callback (GIOChannel *source, GIOCondition condition, WatchData *watch)
{
    seahorse_debug ("GPGME OP: io for GPGME on %d", watch->fd);
    (watch->fnc) (watch->fnc_data, g_io_channel_unix_get_fd (source));
    return TRUE;
}

/* Register a GPGME callback with GLib. */
static void
register_watch (WatchData *watch)
{
    GIOChannel *channel;
    
    if (watch->registered)
        return;

    seahorse_debug ("GPGME OP: registering watch %d", watch->fd);

    channel = g_io_channel_unix_new (watch->fd);
    watch->stag = g_io_add_watch_full (channel, G_PRIORITY_DEFAULT, 
                                       watch->dir ? READ_CONDITION : WRITE_CONDITION,
                                       (GIOFunc)io_callback, watch, NULL);
    watch->registered = TRUE;
    g_io_channel_unref (channel);
}

/* Unregister GPGME callback with GLib. */
static void
unregister_watch (WatchData *watch)
{
    if (!watch->registered)
        return;

    seahorse_debug ("GPGME OP: unregistering watch %d", watch->fd);

    g_source_remove (watch->stag);
    watch->stag = 0;
    watch->registered = FALSE;
}

/* -----------------------------------------------------------------------------
 * CALLED FROM GPGME
 */

/* Called by GPGME when a change of progress state */
static void 
progress_cb (void *data, const char *what, int type, 
             int current, int total)
{
    SeahorseGpgmeOperation *pop = SEAHORSE_GPGME_OPERATION (data);
    SeahorseGpgmeOperationPrivate *pv = SEAHORSE_GPGME_OPERATION_GET_PRIVATE (pop);

    seahorse_debug ("GPGME OP: got progress: %s %d/%d", what, current, total);

    if (total <= 0)
        total = pv->def_total;
    
    if (seahorse_operation_is_running (SEAHORSE_OPERATION (pop)))
        seahorse_operation_mark_progress_full (SEAHORSE_OPERATION (pop), 
                                               pv->message ? pv->message : what, 
                                               current, total);
}

/* Register a callback. */
static gpg_error_t
register_cb (void *data, int fd, int dir, gpgme_io_cb_t fnc, void *fnc_data, 
             void **tag)
{
    SeahorseGpgmeOperation *pop = SEAHORSE_GPGME_OPERATION (data);
    SeahorseGpgmeOperationPrivate *pv = SEAHORSE_GPGME_OPERATION_GET_PRIVATE (pop);
    WatchData *watch;

    seahorse_debug ("PGPOP: request to register watch %d", fd);

    watch = g_new0 (WatchData, 1);
    watch->registered = FALSE;
    watch->fd = fd;
    watch->dir = dir;
    watch->fnc = fnc;
    watch->fnc_data = fnc_data;
    watch->op = pop;
    
    /* 
     * If the context is busy, we already have a START event, and can
     * register watches freely.
     */
    if (pv->busy)
        register_watch (watch);

    pv->watches = g_list_append (pv->watches, watch);
    *tag = watch;

    return GPG_OK;
}

/* Remove a callback. */
static void
remove_cb (void *tag)
{
    SeahorseGpgmeOperationPrivate *pv;
    WatchData *watch = (WatchData*)tag;

    pv = SEAHORSE_GPGME_OPERATION_GET_PRIVATE (watch->op);

    seahorse_debug ("PGPOP: request to remove watch %d", watch->fd);
    
    pv->watches = g_list_remove (pv->watches, watch);
    unregister_watch (watch);
    g_free (watch);
}

/* The event callback. */
static void
event_cb (void *data, gpgme_event_io_t type, void *type_data)
{
    SeahorseGpgmeOperation *pop = SEAHORSE_GPGME_OPERATION (data);
    SeahorseGpgmeOperationPrivate *pv = SEAHORSE_GPGME_OPERATION_GET_PRIVATE (pop);
    gpg_error_t *gerr;
    GError *error = NULL;
    GList *list;
    
    switch (type) {
        
    /* Called when the GPGME context starts an operation */
    case GPGME_EVENT_START:
        pv->busy = TRUE;

        seahorse_debug ("PGPOP: start event");

        /* Since we weren't supposed to register these before, do it now */
        for (list = pv->watches; list; list = g_list_next (list))
            register_watch ((WatchData*)(list->data));
    
        /* And tell everyone we've begun */
        if (!seahorse_operation_is_running (SEAHORSE_OPERATION (pop)))
            seahorse_operation_mark_start (SEAHORSE_OPERATION (pop));
        
        /* Listens for progress updates from GPG */
        gpgme_set_progress_cb (pop->gctx, progress_cb, pop);

        break;
        
    /* Called when the GPGME context is finished with an op */
    case GPGME_EVENT_DONE:
        pv->busy = FALSE;
        gerr = (gpgme_error_t*)type_data;

        seahorse_debug ("PGPOP: done event (err: %d)", *gerr);

        /* Make sure we have no extra watches left over */
        for (list = pv->watches; list; list = g_list_next (list))
            unregister_watch ((WatchData*)(list->data));
        
        gpgme_set_progress_cb (pop->gctx, NULL, NULL);
        
        /* And try to figure out a good response */
        if (seahorse_operation_is_running (SEAHORSE_OPERATION (pop))) {
            
            /* Cancelled */
            if (gpgme_err_code (*gerr) == GPG_ERR_CANCELED) {
                seahorse_operation_mark_done (SEAHORSE_OPERATION (pop), TRUE, NULL);
                
            } else {
                
                /* Reference guard (marking an op as complete unrefs it) */
                g_object_ref (pop);
                
                /* Other Errors */
                if (*gerr)
                    seahorse_gpgme_to_error (*gerr, &error);
                
                /* No error, results available */
                else
                    g_signal_emit (pop, signals[RESULTS], 0);
                
                /* The above event may have started the action again so double check */
                if (!pv->busy && seahorse_operation_is_running (SEAHORSE_OPERATION (pop)))
                    seahorse_operation_mark_done (SEAHORSE_OPERATION (pop), FALSE, error);
                
                /* End reference guard */
                g_object_unref (pop);
            }
        }
        break;
    
    case GPGME_EVENT_NEXT_KEY:
    case GPGME_EVENT_NEXT_TRUSTITEM:
    default:
        /* Ignore unsupported event types */
        break;
    }
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void 
seahorse_gpgme_operation_init (SeahorseGpgmeOperation *pop)
{
    SeahorseGpgmeOperationPrivate *pv = SEAHORSE_GPGME_OPERATION_GET_PRIVATE (pop);
    
    pop->gctx = seahorse_gpgme_source_new_context ();
    g_return_if_fail (pop->gctx != NULL);
    
    pv->busy = FALSE;
    
    /* Setup with the context */
    pv->io_cbs.add = register_cb;
    pv->io_cbs.add_priv = pop;
    pv->io_cbs.remove = remove_cb;
    pv->io_cbs.event = event_cb;
    pv->io_cbs.event_priv = pop;
    
    gpgme_set_io_cbs (pop->gctx, &(pv->io_cbs));
}

static void 
seahorse_gpgme_operation_set_property (GObject *gobject, guint prop_id, 
                                     const GValue *value, GParamSpec *pspec)
{
    SeahorseGpgmeOperation *pop = SEAHORSE_GPGME_OPERATION (gobject);
    SeahorseGpgmeOperationPrivate *pv = SEAHORSE_GPGME_OPERATION_GET_PRIVATE (pop);
    
    switch (prop_id) {
    case PROP_MESSAGE:
        g_free (pv->message);
        pv->message = g_strdup (g_value_get_string (value));
        break;
    case PROP_DEF_TOTAL:
        pv->def_total = g_value_get_uint (value);
        break;
    }
}

static void 
seahorse_gpgme_operation_get_property (GObject *gobject, guint prop_id, 
                                     GValue *value, GParamSpec *pspec)
{
    SeahorseGpgmeOperation *pop = SEAHORSE_GPGME_OPERATION (gobject);
    SeahorseGpgmeOperationPrivate *pv = SEAHORSE_GPGME_OPERATION_GET_PRIVATE (pop);
    
    switch (prop_id) {
    case PROP_GCTX:
        g_value_set_pointer (value, pop->gctx);
        break;
    case PROP_MESSAGE:
        g_value_set_string (value, pv->message);
        break;
    case PROP_DEF_TOTAL:
        g_value_set_uint (value, pv->def_total);
    }
}

static void 
seahorse_gpgme_operation_dispose (GObject *gobject)
{
    /* Nothing to do */
    G_OBJECT_CLASS (gpgme_operation_parent_class)->dispose (gobject);
}

static void 
seahorse_gpgme_operation_finalize (GObject *gobject)
{
    SeahorseGpgmeOperation *pop = SEAHORSE_GPGME_OPERATION (gobject);
    SeahorseGpgmeOperationPrivate *pv = SEAHORSE_GPGME_OPERATION_GET_PRIVATE (pop);
    GList *list;
    
    if (pv->busy) {
        g_critical ("NASTY BUG. Disposing of a SeahorseGpgmeOperation while GPGME is "
                    "still performing an operation. SeahorseOperation should ref"
                    "itself while active");
    }
    
    /* Make sure we have no extra watches left over */
    for (list = pv->watches; list; list = g_list_next (list)) {
        unregister_watch ((WatchData*)(list->data));
        g_free (list->data);
    }
    
    g_list_free (pv->watches);
    pv->watches = NULL;
    
    if (pop->gctx)
        gpgme_release (pop->gctx);
    pop->gctx = NULL;
    
    g_free (pv->message);
    pv->message = NULL;
    
    G_OBJECT_CLASS (gpgme_operation_parent_class)->finalize (gobject);
}

static void 
seahorse_gpgme_operation_cancel (SeahorseOperation *operation)
{
    SeahorseGpgmeOperation *pop = SEAHORSE_GPGME_OPERATION (operation);
    SeahorseGpgmeOperationPrivate *pv = SEAHORSE_GPGME_OPERATION_GET_PRIVATE (pop);
    
    g_return_if_fail (seahorse_operation_is_running (operation));
    g_return_if_fail (pop->gctx != NULL);
    
    /* This should call in through event_cb and cancel us */
    if (pv->busy)
        gpgme_cancel (pop->gctx);
}

/* -----------------------------------------------------------------------------
 * PUBLIC METHODS
 */

SeahorseGpgmeOperation*
seahorse_gpgme_operation_new (const gchar *message)
{
    return g_object_new (SEAHORSE_TYPE_GPGME_OPERATION, "message", message, NULL);
}

void
seahorse_gpgme_operation_mark_failed (SeahorseGpgmeOperation *pop, gpgme_error_t gerr)
{
    SeahorseOperation *op = SEAHORSE_OPERATION (pop);
    GError *err = NULL;
    
    if (!seahorse_operation_is_running (op))
        seahorse_operation_mark_start (op);
    
    seahorse_gpgme_to_error (gerr, &err);
    seahorse_operation_mark_done (op, FALSE, err);
}
