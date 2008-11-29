/*
 * Seahorse
 *
 * Copyright (C) 2004-2006 Stefan Walter
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

#include <glib/gi18n.h>
#include <string.h>
#include "seahorse-pgp-source.h"

#include "seahorse-gpgme-io.h"
#include "seahorse-gpgmex.h"
#include "seahorse-gpg-options.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-key-op.h"
#include "seahorse-pgp-operation.h"

#include "seahorse-operation.h"
#include "seahorse-util.h"
#include "seahorse-libdialogs.h"
#include "seahorse-passphrase.h"

#include "common/seahorse-registry.h"

#include <gio/gio.h>

#include <stdlib.h>
#include <libintl.h>
#include <locale.h>

/* TODO: Verify properly that all keys we deal with are PGP keys */

/* Override the DEBUG_REFRESH_ENABLE switch here */
#define DEBUG_REFRESH_ENABLE 0

#ifndef DEBUG_REFRESH_ENABLE
#if _DEBUG
#define DEBUG_REFRESH_ENABLE 1
#else
#define DEBUG_REFRESH_ENABLE 0
#endif
#endif

#if DEBUG_REFRESH_ENABLE
#define DEBUG_REFRESH(x)    g_printerr(x)
#else
#define DEBUG_REFRESH(x)
#endif

/* Amount of keys to load in a batch */
#define DEFAULT_LOAD_BATCH 200

enum {
    LOAD_FULL = 0x01,
    LOAD_PHOTOS = 0x02
};

enum {
    PROP_0,
    PROP_KEY_TYPE,
    PROP_KEY_DESC,
    PROP_LOCATION
};

static gpgme_error_t
passphrase_get (gconstpointer dummy, const gchar *passphrase_hint, 
                const char* passphrase_info, int flags, int fd)
{
    GtkDialog *dialog;
    gpgme_error_t err;
    gchar **split_uid = NULL;
    gchar *label = NULL;
    gchar *errmsg = NULL;
    const gchar *pass;
    
    if (passphrase_info && strlen(passphrase_info) < 16)
        flags |= SEAHORSE_PASS_NEW;
    
    if (passphrase_hint)
        split_uid = g_strsplit (passphrase_hint, " ", 2);

    if (flags & SEAHORSE_PASS_BAD) 
        errmsg = g_strdup_printf (_("Wrong passphrase."));
    
    if (split_uid && split_uid[0] && split_uid[1]) {
        if (flags & SEAHORSE_PASS_NEW) 
            label = g_strdup_printf (_("Enter new passphrase for '%s'"), split_uid[1]);
        else 
            label = g_strdup_printf (_("Enter passphrase for '%s'"), split_uid[1]);
    } else {
        if (flags & SEAHORSE_PASS_NEW) 
            label = g_strdup (_("Enter new passphrase"));
        else 
            label = g_strdup (_("Enter passphrase"));
    }

    g_strfreev (split_uid);

    dialog = seahorse_passphrase_prompt_show (NULL, errmsg ? errmsg : label, 
                                              NULL, NULL, FALSE);
    g_free (label);
    g_free (errmsg);
    
    switch (gtk_dialog_run (dialog)) {
    case GTK_RESPONSE_ACCEPT:
        pass = seahorse_passphrase_prompt_get (dialog);
        seahorse_util_printf_fd (fd, "%s\n", pass);
        err = GPG_OK;
        break;
    default:
        err = GPG_E (GPG_ERR_CANCELED);
        break;
    };
    
    gtk_widget_destroy (GTK_WIDGET (dialog));
    return err;
}

/* Initialise a GPGME context for PGP keys */
static gpgme_error_t
init_gpgme (gpgme_ctx_t *ctx)
{
    gpgme_protocol_t proto = GPGME_PROTOCOL_OpenPGP;
    gpgme_error_t err;
 
    err = gpgme_engine_check_version (proto);
    g_return_val_if_fail (GPG_IS_OK (err), err);
   
    err = gpgme_new (ctx);
    g_return_val_if_fail (GPG_IS_OK (err), err);
   
    err = gpgme_set_protocol (*ctx, proto);
    g_return_val_if_fail (GPG_IS_OK (err), err);
    
    gpgme_set_passphrase_cb (*ctx, (gpgme_passphrase_cb_t)passphrase_get, 
                             NULL);
   
    gpgme_set_keylist_mode (*ctx, GPGME_KEYLIST_MODE_LOCAL);
    return err;
}

/* -----------------------------------------------------------------------------
 * LOAD OPERATION 
 */
 

#define SEAHORSE_TYPE_LOAD_OPERATION            (seahorse_load_operation_get_type ())
#define SEAHORSE_LOAD_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_LOAD_OPERATION, SeahorseLoadOperation))
#define SEAHORSE_LOAD_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_LOAD_OPERATION, SeahorseLoadOperationClass))
#define SEAHORSE_IS_LOAD_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_LOAD_OPERATION))
#define SEAHORSE_IS_LOAD_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_LOAD_OPERATION))
#define SEAHORSE_LOAD_OPERATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_LOAD_OPERATION, SeahorseLoadOperationClass))

DECLARE_OPERATION (Load, load)
    /*< private >*/
    SeahorsePGPSource *psrc;        /* Key source to add keys to when found */
    gpgme_ctx_t ctx;                /* GPGME context we're loading from */
    gboolean secret;                /* Loading secret keys */
    guint loaded;                   /* Number of keys we've loaded */
    guint batch;                    /* Number to load in a batch, or 0 for synchronous */
    guint stag;                     /* The event source handler id (for stopping a load) */
    guint parts;                    /* Parts to load*/
    GHashTable *checks;             /* When refreshing this is our set of missing keys */
END_DECLARE_OPERATION        

IMPLEMENT_OPERATION (Load, load)

static SeahorseLoadOperation*   seahorse_load_operation_start   (SeahorsePGPSource *psrc, 
                                                                 const gchar **pattern, 
                                                                 guint parts,
                                                                 gboolean secret);

static gboolean                 scheduled_dummy                 (gpointer data);

/* -----------------------------------------------------------------------------
 * EXPORT 
 */
 
typedef struct _ExportContext {
    GArray *keyids;
    guint at;
    gpgme_data_t data;
} ExportContext;

static void
free_export_context (gpointer p)
{
    ExportContext *ctx = (ExportContext*)p;
    if (!ctx)
        return;
    gpgme_data_release (ctx->data);
    g_array_free (ctx->keyids, TRUE);
    g_free (ctx);
}

static void
export_key_callback (SeahorsePGPOperation *pop, ExportContext *ctx)
{
    gpgme_error_t gerr;
    GError *err = NULL;
    GOutputStream *output;
    
    if (seahorse_operation_is_running (SEAHORSE_OPERATION (pop)))
        seahorse_operation_mark_progress_full (SEAHORSE_OPERATION (pop), NULL, 
                                               ctx->at, ctx->keyids->len);
    
    /* Done, close the output stream */
    if (ctx->at >= ctx->keyids->len) {
	    output = seahorse_operation_get_result (SEAHORSE_OPERATION (pop));
	    g_return_if_fail (G_IS_OUTPUT_STREAM (output));
	    if (!g_output_stream_close (output, NULL, &err))
		    seahorse_operation_mark_done (SEAHORSE_OPERATION (pop), FALSE, err);
	    return;
    }
    
    /* Do the next key in the list */
    gerr = gpgme_op_export_start (pop->gctx, g_array_index (ctx->keyids, const char*, ctx->at), 
                                  0, ctx->data);
    ctx->at++;
    
    if (!GPG_IS_OK (gerr))
        seahorse_pgp_operation_mark_failed (pop, gerr);
}


/* -----------------------------------------------------------------------------
 * PGP Source
 */
    
struct _SeahorsePGPSourcePrivate {
    guint scheduled_refresh;                /* Source for refresh timeout */
    GFileMonitor *monitor_handle;           /* For monitoring the .gnupg directory */
    SeahorseMultiOperation *operations;     /* A list of all current operations */    
    GList *orphan_secret;                   /* Orphan secret keys */
};

G_DEFINE_TYPE (SeahorsePGPSource, seahorse_pgp_source, SEAHORSE_TYPE_SOURCE);

/* GObject handlers */
static void seahorse_pgp_source_dispose         (GObject *gobject);
static void seahorse_pgp_source_finalize        (GObject *gobject);
static void seahorse_pgp_source_set_property    (GObject *object, guint prop_id, 
                                                 const GValue *value, GParamSpec *pspec);
static void seahorse_pgp_source_get_property    (GObject *object, guint prop_id,
                                                 GValue *value, GParamSpec *pspec);

/* SeahorseSource methods */
static SeahorseOperation*  seahorse_pgp_source_load             (SeahorseSource *src,
                                                                 GQuark keyid);
static SeahorseOperation*  seahorse_pgp_source_import           (SeahorseSource *sksrc, 
                                                                 GInputStream *input);
static SeahorseOperation*  seahorse_pgp_source_export           (SeahorseSource *sksrc, 
                                                                 GList *keys,
                                                                 GOutputStream *output);
static SeahorseOperation*  seahorse_pgp_source_remove           (SeahorseSource *sksrc, 
                                                                 SeahorseObject *sobj);

/* Other forward decls */
static void                monitor_gpg_homedir                  (GFileMonitor *handle, 
                                                                 GFile *file,
                                                                 GFile *other_file,
                                                                 GFileMonitorEvent event_type,
                                                                 gpointer user_data);
static void                cancel_scheduled_refresh             (SeahorsePGPSource *psrc);
                                                                 
static GObjectClass *parent_class = NULL;

/* Initialize the basic class stuff */
static void
seahorse_pgp_source_class_init (SeahorsePGPSourceClass *klass)
{
    GObjectClass *gobject_class;
    SeahorseSourceClass *key_class;
    
    g_message ("init gpgme version %s", gpgme_check_version (NULL));
    
#ifdef ENABLE_NLS
    gpgme_set_locale (NULL, LC_CTYPE, setlocale (LC_CTYPE, NULL));
    gpgme_set_locale (NULL, LC_MESSAGES, setlocale (LC_MESSAGES, NULL));
#endif
   
    parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->dispose = seahorse_pgp_source_dispose;
    gobject_class->finalize = seahorse_pgp_source_finalize;
    gobject_class->set_property = seahorse_pgp_source_set_property;
    gobject_class->get_property = seahorse_pgp_source_get_property;
    
    key_class = SEAHORSE_SOURCE_CLASS (klass);
    key_class->canonize_id = seahorse_pgp_key_get_cannonical_id;
    key_class->load = seahorse_pgp_source_load;
    key_class->import = seahorse_pgp_source_import;
    key_class->export = seahorse_pgp_source_export;
    key_class->remove = seahorse_pgp_source_remove;
 
    g_object_class_install_property (gobject_class, PROP_KEY_TYPE,
        g_param_spec_uint ("key-type", "Key Type", "Key type that originates from this key source.", 
                           0, G_MAXUINT, SEAHORSE_TAG_INVALID, G_PARAM_READABLE));
                           
    g_object_class_install_property (gobject_class, PROP_KEY_DESC,
        g_param_spec_string ("key-desc", "Key Desc", "Description for keys that originate here.",
                             NULL, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_LOCATION,
        g_param_spec_uint ("location", "Key Location", "Where the key is stored. See SeahorseLocation", 
                           0, G_MAXUINT, SEAHORSE_LOCATION_INVALID, G_PARAM_READABLE));    
    
	seahorse_registry_register_type (NULL, SEAHORSE_TYPE_PGP_SOURCE, "source", "local", SEAHORSE_PGP_STR, NULL);

}

/* init context, private vars, set prefs, connect signals */
static void
seahorse_pgp_source_init (SeahorsePGPSource *psrc)
{
	gpgme_error_t gerr;
	GError *err = NULL;
	const gchar *gpg_homedir;
	GFile *file;
    
	gerr = init_gpgme (&(psrc->gctx));
	g_return_if_fail (GPG_IS_OK (gerr));
    
	/* init private vars */
	psrc->pv = g_new0 (SeahorsePGPSourcePrivate, 1);
    
	psrc->pv->operations = seahorse_multi_operation_new ();
    
	psrc->pv->scheduled_refresh = 0;
	psrc->pv->monitor_handle = NULL;
    
	gpg_homedir = seahorse_gpg_homedir ();
	file = g_file_new_for_path (gpg_homedir);
	g_return_if_fail (file != NULL);
    
	psrc->pv->monitor_handle = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, &err);
	g_object_unref (file);
	
	if (psrc->pv->monitor_handle) {
		g_signal_connect (psrc->pv->monitor_handle, "changed", 
		                  G_CALLBACK (monitor_gpg_homedir), psrc);
	} else {
		g_warning ("couldn't monitor the GPG home directory: %s: %s", 
		           gpg_homedir, err && err->message ? err->message : "");
	}
}

/* dispose of all our internal references */
static void
seahorse_pgp_source_dispose (GObject *gobject)
{
    SeahorsePGPSource *psrc;
    GList *l;
    
    /*
     * Note that after this executes the rest of the object should
     * still work without a segfault. This basically nullifies the 
     * object, but doesn't free it.
     * 
     * This function should also be able to run multiple times.
     */
  
    psrc = SEAHORSE_PGP_SOURCE (gobject);
    g_assert (psrc->pv);
    
    /* Clear out all operations */
    if (psrc->pv->operations) {
        if(seahorse_operation_is_running (SEAHORSE_OPERATION (psrc->pv->operations)))
            seahorse_operation_cancel (SEAHORSE_OPERATION (psrc->pv->operations));
        g_object_unref (psrc->pv->operations);
        psrc->pv->operations = NULL;
    }

    cancel_scheduled_refresh (psrc);    
    
	if (psrc->pv->monitor_handle) {
		g_object_unref (psrc->pv->monitor_handle);
		psrc->pv->monitor_handle = NULL;
	}
    
    for (l = psrc->pv->orphan_secret; l; l = g_list_next (l)) 
        g_object_unref (l->data);
    g_list_free (psrc->pv->orphan_secret);
    psrc->pv->orphan_secret = NULL;
    
    if (psrc->gctx)
        gpgme_release (psrc->gctx);
    
    G_OBJECT_CLASS (parent_class)->dispose (gobject);
}

/* free private vars */
static void
seahorse_pgp_source_finalize (GObject *gobject)
{
    SeahorsePGPSource *psrc;
  
    psrc = SEAHORSE_PGP_SOURCE (gobject);
    g_assert (psrc->pv);
    
    /* All monitoring and scheduling should be done */
    g_assert (psrc->pv->scheduled_refresh == 0);
    g_assert (psrc->pv->monitor_handle == 0);
    
    g_free (psrc->pv);
 
    G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void 
seahorse_pgp_source_set_property (GObject *object, guint prop_id, const GValue *value, 
                                  GParamSpec *pspec)
{
    
}

static void 
seahorse_pgp_source_get_property (GObject *object, guint prop_id, GValue *value, 
                                  GParamSpec *pspec)
{
    switch (prop_id) {
    case PROP_KEY_TYPE:
        g_value_set_uint (value, SEAHORSE_PGP);
        break;
    case PROP_KEY_DESC:
        g_value_set_string (value, _("PGP Key"));
        break;
    case PROP_LOCATION:
        g_value_set_uint (value, SEAHORSE_LOCATION_LOCAL);
        break;
    }
}

/* --------------------------------------------------------------------------
 * HELPERS 
 */

/* Remove the given key from the context */
static void
remove_key_from_context (gpointer kt, SeahorseObject *dummy, SeahorsePGPSource *psrc)
{
    /* This function gets called as a GHRFunc on the lctx->checks hashtable. */
    GQuark keyid = GPOINTER_TO_UINT (kt);
    SeahorseObject *sobj;
    
    sobj = seahorse_context_get_object (SCTX_APP (), SEAHORSE_SOURCE (psrc), keyid);
    if (sobj != NULL)
        seahorse_context_remove_object (SCTX_APP (), sobj);
}

/* Add a key to the context  */
static SeahorsePGPKey*
add_key_to_context (SeahorsePGPSource *psrc, gpgme_key_t key, gpgmex_photo_id_t photos)
{
    SeahorsePGPKey *pkey = NULL;
    SeahorsePGPKey *prev;
    const gchar *id;
    GQuark keyid;
    GList *l;
    
    id = seahorse_pgp_key_get_id (key, 0);
    keyid = seahorse_pgp_key_get_cannonical_id (id);
    g_return_val_if_fail (keyid, NULL);
    
    g_assert (SEAHORSE_IS_PGP_SOURCE (psrc));
    prev = SEAHORSE_PGP_KEY (seahorse_context_get_object (SCTX_APP (), SEAHORSE_SOURCE (psrc), keyid));
    
    /* Check if we can just replace the key on the object */
    if (prev != NULL) {
        if (key->secret) 
            g_object_set (prev, "seckey", key, NULL);
        else
            g_object_set (prev, "pubkey", key, NULL);
        
        if (prev->photoids)
            gpgmex_photo_id_free_all (prev->photoids);
        prev->photoids = photos;
        
        return prev;
    }
    
    /* Create a new key with secret */    
    if (key->secret) {
        pkey = seahorse_pgp_key_new (SEAHORSE_SOURCE (psrc), NULL, key);
        
        /* Since we don't have a public key yet, save this away */
        psrc->pv->orphan_secret = g_list_append (psrc->pv->orphan_secret, pkey);
        
        if (photos)
            gpgmex_photo_id_free_all (photos);
        
        /* No key was loaded as far as everyone is concerned */
        return NULL;
    }
 
    /* Just a new public key */

    /* Check for orphans */
    for (l = psrc->pv->orphan_secret; l; l = g_list_next (l)) {
        
        /* Look for a matching key */
        if (g_str_equal (id, seahorse_pgp_key_get_id (SEAHORSE_PGP_KEY(l->data)->seckey, 0))) {
            
            /* Set it up properly */
            pkey = SEAHORSE_PGP_KEY (l->data);
            g_object_set (pkey, "pubkey", key, NULL);
            
            /* Remove item from orphan list cleanly */
            psrc->pv->orphan_secret = g_list_remove_link (psrc->pv->orphan_secret, l);
            g_list_free (l);
            break;
        }
    }

    if (pkey == NULL)
        pkey = seahorse_pgp_key_new (SEAHORSE_SOURCE (psrc), key, NULL);
    
    /* Add to context */ 
    seahorse_context_take_object (SCTX_APP (), SEAHORSE_OBJECT (pkey));

    if (pkey->photoids)
        gpgmex_photo_id_free_all (pkey->photoids);
    pkey->photoids = photos;
    
    return pkey; 
}

/* -----------------------------------------------------------------------------
 *  GPG HOME DIR MONITORING
 */

static gboolean
scheduled_refresh (gpointer data)
{
    SeahorsePGPSource *psrc = SEAHORSE_PGP_SOURCE (data);

    DEBUG_REFRESH ("scheduled refresh event ocurring now\n");
    cancel_scheduled_refresh (psrc);
    seahorse_source_load_async (SEAHORSE_SOURCE (psrc), 0);
    
    return FALSE; /* don't run again */
}

static gboolean
scheduled_dummy (gpointer data)
{
    SeahorsePGPSource *psrc = SEAHORSE_PGP_SOURCE (data);
    DEBUG_REFRESH ("dummy refresh event occurring now\n");
    psrc->pv->scheduled_refresh = 0;
    return FALSE; /* don't run again */    
}

static void
cancel_scheduled_refresh (SeahorsePGPSource *psrc)
{
    if (psrc->pv->scheduled_refresh != 0) {
        DEBUG_REFRESH ("cancelling scheduled refresh event\n");
        g_source_remove (psrc->pv->scheduled_refresh);
        psrc->pv->scheduled_refresh = 0;
    }
}
        
static void
monitor_gpg_homedir (GFileMonitor *handle, GFile *file, GFile *other_file,
                     GFileMonitorEvent event_type, gpointer user_data)
{
	SeahorsePGPSource *psrc = SEAHORSE_PGP_SOURCE (user_data);
	gchar *name;
	
	if (event_type == G_FILE_MONITOR_EVENT_CHANGED || 
	    event_type == G_FILE_MONITOR_EVENT_DELETED ||
	    event_type == G_FILE_MONITOR_EVENT_CREATED) {

		name = g_file_get_basename (file);
		if (g_str_has_suffix (name, SEAHORSE_EXT_GPG)) {
			if (psrc->pv->scheduled_refresh == 0) {
				DEBUG_REFRESH ("scheduling refresh event due to file changes\n");
				psrc->pv->scheduled_refresh = g_timeout_add (500, scheduled_refresh, psrc);
			}
		}
	}
}

/* --------------------------------------------------------------------------
 *  OPERATION STUFF 
 */
 
static void 
seahorse_load_operation_init (SeahorseLoadOperation *lop)
{
    gpgme_error_t err;
    
    err = init_gpgme (&(lop->ctx));
    if (!GPG_IS_OK (err)) 
        g_return_if_reached ();
    
    lop->checks = NULL;
    lop->batch = DEFAULT_LOAD_BATCH;
    lop->stag = 0;
}

static void 
seahorse_load_operation_dispose (GObject *gobject)
{
    SeahorseLoadOperation *lop = SEAHORSE_LOAD_OPERATION (gobject);
    
    /*
     * Note that after this executes the rest of the object should
     * still work without a segfault. This basically nullifies the 
     * object, but doesn't free it.
     * 
     * This function should also be able to run multiple times.
     */
  
    if (lop->stag) {
        g_source_remove (lop->stag);
        lop->stag = 0;
    }

    if (lop->psrc) {
        g_object_unref (lop->psrc);
        lop->psrc = NULL;
    }

    G_OBJECT_CLASS (load_operation_parent_class)->dispose (gobject);
}

static void 
seahorse_load_operation_finalize (GObject *gobject)
{
    SeahorseLoadOperation *lop = SEAHORSE_LOAD_OPERATION (gobject);
    
    if (lop->checks)    
        g_hash_table_destroy (lop->checks);

    g_assert (lop->stag == 0);
    g_assert (lop->psrc == NULL);

    if (lop->ctx)
        gpgme_release (lop->ctx);
        
    G_OBJECT_CLASS (load_operation_parent_class)->finalize (gobject);
}

static void 
seahorse_load_operation_cancel (SeahorseOperation *operation)
{
    SeahorseLoadOperation *lop = SEAHORSE_LOAD_OPERATION (operation);    

    gpgme_op_keylist_end (lop->ctx);
    seahorse_operation_mark_done (operation, TRUE, NULL);
}

/* Completes one batch of key loading */
static gboolean
keyload_handler (SeahorseLoadOperation *lop)
{
    SeahorsePGPKey *pkey;
    gpgmex_photo_id_t photos;
    gpgme_key_t key;
    guint batch;
    const gchar *id;
    GQuark keyid;
    gchar *t;
    
    g_assert (SEAHORSE_IS_LOAD_OPERATION (lop));
    
    /* We load until done if batch is zero */
    batch = lop->batch == 0 ? ~0 : lop->batch;

    while (batch-- > 0) {
    
        if (!GPG_IS_OK (gpgme_op_keylist_next (lop->ctx, &key))) {
        
            gpgme_op_keylist_end (lop->ctx);
        
            /* If we were a refresh loader, then we remove the keys we didn't find */
            if (lop->checks) 
                g_hash_table_foreach (lop->checks, (GHFunc)remove_key_from_context, lop->psrc);
            
            seahorse_operation_mark_done (SEAHORSE_OPERATION (lop), FALSE, NULL);         
            return FALSE; /* Remove event handler */
        }
        
        id = seahorse_pgp_key_get_id (key, 0);
        keyid = seahorse_pgp_key_get_cannonical_id (id);
        
        /* Invalid id from GPG ? */
        if (!keyid) {
            gpgmex_key_unref (key);
            continue;
        }
        
        /* During a refresh if only new or removed keys */
        if (lop->checks) {

            /* Make note that this key exists in key ring */
            g_hash_table_remove (lop->checks, GUINT_TO_POINTER (keyid));

        }
        
        /* Load additional info */
        photos = NULL;
        if (lop->parts & LOAD_PHOTOS)
            seahorse_pgp_key_op_photoid_load (lop->psrc, key, &photos);

        pkey = add_key_to_context (lop->psrc, key, photos);
        
        gpgmex_key_unref (key);
        lop->loaded++;
    }
    
    /* More to do, so queue for next round */        
    if (lop->stag == 0) {
    
        /* If it returns TRUE (like any good GSourceFunc) it means
         * it needs to stick around, so we register an idle handler */
        lop->stag = g_idle_add_full (G_PRIORITY_LOW, (GSourceFunc)keyload_handler, 
                                     lop, NULL);
    }
    
    /* TODO: We can use the GPGME progress to make this more accurate */
    t = g_strdup_printf (ngettext("Loaded %d key", "Loaded %d keys", lop->loaded), lop->loaded);
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (lop), t, 0.0);
    g_free (t);
    
    return TRUE; 
}

static SeahorseLoadOperation*
seahorse_load_operation_start (SeahorsePGPSource *psrc, const gchar **pattern, 
                               guint parts, gboolean secret)
{
    SeahorsePGPSourcePrivate *priv;
    SeahorseLoadOperation *lop;
    gpgme_error_t err;
    GList *keys, *l;
    SeahorseObject *sobj;
    
    g_assert (SEAHORSE_IS_PGP_SOURCE (psrc));
    priv = psrc->pv;

    lop = g_object_new (SEAHORSE_TYPE_LOAD_OPERATION, NULL);    
    lop->psrc = psrc;
    lop->secret = secret;
    g_object_ref (psrc);
    
    /* See which extra parts we should load */
    lop->parts = parts;
    if (parts & LOAD_FULL) 
        gpgme_set_keylist_mode (lop->ctx, GPGME_KEYLIST_MODE_SIGS | 
                gpgme_get_keylist_mode (lop->ctx));
    
    /* Start the key listing */
    if (pattern)
        err = gpgme_op_keylist_ext_start (lop->ctx, pattern, secret, 0);
    else
        err = gpgme_op_keylist_start (lop->ctx, NULL, secret);
    g_return_val_if_fail (GPG_IS_OK (err), lop);
    
    /* Loading all the keys? */
    if (!pattern) {
     
        lop->checks = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);
        keys = seahorse_context_get_objects (SCTX_APP (), SEAHORSE_SOURCE (psrc));
        for (l = keys; l; l = g_list_next (l)) {
            sobj = SEAHORSE_OBJECT (l->data);
            if (secret && seahorse_object_get_usage (sobj) != SEAHORSE_USAGE_PRIVATE_KEY) 
                continue;
            g_hash_table_insert (lop->checks, GUINT_TO_POINTER (seahorse_object_get_id (l->data)), 
                                              GUINT_TO_POINTER (TRUE));
        }
        g_list_free (keys);
        
    }
    
    seahorse_operation_mark_start (SEAHORSE_OPERATION (lop));
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (lop), _("Loading Keys..."), 0.0);
    
    /* Run one iteration of the handler */
    keyload_handler (lop);
 
    return lop;
}    

static void
prepare_import_results (SeahorsePGPOperation *pop, SeahorsePGPSource *psrc)
{
    SeahorseObject *sobj;
    SeahorseLoadOperation *lop;
    gpgme_import_result_t results;
    gpgme_import_status_t import;
    SeahorseSource *sksrc;
    GList *keys = NULL;
    const gchar **patterns = NULL;
    GError *err = NULL;
    gchar *msg;
    GQuark keyid;
    guint i;
    
    sksrc = SEAHORSE_SOURCE (psrc);

    /* Figure out which keys were imported */
    results = gpgme_op_import_result (pop->gctx);
    if (results) {
            
        /* Dig out all the fingerprints for use as load patterns */
        patterns = (const gchar**)g_new0(gchar*, results->considered + 1);
        for (i = 0, import = results->imports; 
             i < results->considered && import; 
             import = import->next) {
            if (GPG_IS_OK (import->result))
                patterns[i++] = import->fpr;
        }
        
        /* See if we've managed to import any ... */
        if (!patterns[0] && results->considered > 0) {
            
            /* ... try and find out why */
            if (results->no_user_id) {
                msg = _("Invalid key data (missing UIDs). This may be due to a computer with a date set in the future or a missing self-signature.");
                g_set_error (&err, SEAHORSE_ERROR, -1, "%s", msg);
                seahorse_operation_mark_done (SEAHORSE_OPERATION (pop), FALSE, err);
                return;
            }
        }

        /* Reload public keys */
        lop = seahorse_load_operation_start (psrc, patterns, LOAD_FULL, FALSE);
        seahorse_operation_wait (SEAHORSE_OPERATION (lop));
        g_object_unref (lop);

        /* Reload secret keys */
        lop = seahorse_load_operation_start (psrc, patterns, LOAD_FULL, TRUE);
        seahorse_operation_wait (SEAHORSE_OPERATION (lop));
        g_object_unref (lop);
            
        g_free (patterns);
            
        /* Now get a list of the new keys */
        for (import = results->imports; import; import = import->next) {
            if (!GPG_IS_OK (import->result))
                continue;
            
            keyid = seahorse_pgp_key_get_cannonical_id (import->fpr);
            if (!keyid) {
                g_warning ("imported non key with strange keyid: %s", import->fpr);
                continue;
            }
            
            sobj = seahorse_context_get_object (SCTX_APP (), sksrc, keyid);
            if (sobj == NULL) {
                g_warning ("imported key but then couldn't find it in keyring: %s", 
                           import->fpr);
                continue;
            }
            
            keys = g_list_prepend (keys, sobj);
        }
    }
    
    seahorse_operation_mark_result (SEAHORSE_OPERATION (pop), keys, 
                                    (GDestroyNotify)g_list_free);
}

/* --------------------------------------------------------------------------
 * METHODS
 */

static SeahorseOperation*
seahorse_pgp_source_load (SeahorseSource *src, GQuark keyid)
{
    SeahorsePGPSource *psrc;
    SeahorseLoadOperation *lop;
    const gchar *match = NULL;
    const gchar *patterns[2];
    
    g_assert (SEAHORSE_IS_SOURCE (src));
    psrc = SEAHORSE_PGP_SOURCE (src);
    
    /* Schedule a dummy refresh. This blocks all monitoring for a while */
    cancel_scheduled_refresh (psrc);
    psrc->pv->scheduled_refresh = g_timeout_add (500, scheduled_dummy, psrc);
    DEBUG_REFRESH ("scheduled a dummy refresh\n");
 
    if (keyid)
        match = seahorse_pgp_key_get_rawid (keyid);
    
    patterns[0] = match;
    patterns[1] = NULL;

    DEBUG_REFRESH ("refreshing keys...\n");

    /* Secret keys */
    lop = seahorse_load_operation_start (psrc, match ? patterns : NULL, 
                                         match ? LOAD_FULL | LOAD_PHOTOS : 0, FALSE);
    seahorse_multi_operation_take (psrc->pv->operations, SEAHORSE_OPERATION (lop));

    /* Public keys */
    lop = seahorse_load_operation_start (psrc, match ? patterns : NULL, 
                                         match ? LOAD_FULL | LOAD_PHOTOS : 0, TRUE);
    seahorse_multi_operation_take (psrc->pv->operations, SEAHORSE_OPERATION (lop));

    g_object_ref (psrc->pv->operations);
    return SEAHORSE_OPERATION (psrc->pv->operations);
}

static SeahorseOperation* 
seahorse_pgp_source_import (SeahorseSource *sksrc, GInputStream *input)
{
	SeahorsePGPOperation *pop;
	SeahorsePGPSource *psrc;
	gpgme_error_t gerr;
	gpgme_data_t data;
    
    	g_return_val_if_fail (SEAHORSE_IS_PGP_SOURCE (sksrc), NULL);
    	psrc = SEAHORSE_PGP_SOURCE (sksrc);
    
    	g_return_val_if_fail (G_IS_INPUT_STREAM (input), NULL);
    
    	pop = seahorse_pgp_operation_new (_("Importing Keys"));
    	g_return_val_if_fail (pop != NULL, NULL);
    
	data = seahorse_gpgme_input_data (input);
	g_return_val_if_fail (data, NULL);
    
	gerr = gpgme_op_import_start (pop->gctx, data);
    
	g_signal_connect (pop, "results", G_CALLBACK (prepare_import_results), psrc);
	g_object_set_data_full (G_OBJECT (pop), "source-data", data, 
	                        (GDestroyNotify)gpgmex_data_release);
    
	/* Couldn't start import */
	if (!GPG_IS_OK (gerr))
		seahorse_pgp_operation_mark_failed (pop, gerr);
    
	return SEAHORSE_OPERATION (pop);
}

static SeahorseOperation* 
seahorse_pgp_source_export (SeahorseSource *sksrc, GList *keys, GOutputStream *output)
{
	SeahorsePGPOperation *pop;
	SeahorsePGPSource *psrc;
	SeahorsePGPKey *pkey;
	SeahorseObject *object;
	ExportContext *ctx;
	gpgme_data_t data;
	const gchar *keyid;
	GList *l;
    
    	g_return_val_if_fail (SEAHORSE_IS_PGP_SOURCE (sksrc), NULL);
    	g_return_val_if_fail (output == NULL || G_IS_OUTPUT_STREAM (output), NULL);
    
    	psrc = SEAHORSE_PGP_SOURCE (sksrc);

    	pop = seahorse_pgp_operation_new (_("Exporting Keys"));
    	g_return_val_if_fail (pop != NULL, NULL);

	g_object_ref (output);
        seahorse_operation_mark_result (SEAHORSE_OPERATION (pop), output, 
                                        (GDestroyNotify)g_object_unref);
    	
        gpgme_set_armor (pop->gctx, TRUE);
        gpgme_set_textmode (pop->gctx, TRUE);
        
        data = seahorse_gpgme_output_data (output);
        g_return_val_if_fail (data, NULL);

        /* Export context for asynchronous export */
        ctx = g_new0 (ExportContext, 1);
        ctx->keyids = g_array_new (TRUE, TRUE, sizeof (gchar*));
        ctx->at = 0;
        ctx->data = data;
        g_object_set_data_full (G_OBJECT (pop), "export-context", ctx, free_export_context);
    
        for (l = keys; l != NULL; l = g_list_next (l)) {
        	
        	/* Ignore PGP Uids */
        	if (SEAHORSE_IS_PGP_UID (l->data))
        		continue;
        
        	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (l->data), NULL);
        	pkey = SEAHORSE_PGP_KEY (l->data);
        
        	object = SEAHORSE_OBJECT (l->data);
        	g_return_val_if_fail (seahorse_object_get_source (object) == sksrc, NULL);
        
        	/* Building list */
        	keyid = seahorse_pgp_key_get_id (pkey->pubkey, 0);
        	g_array_append_val (ctx->keyids, keyid);
        }

        g_signal_connect (pop, "results", G_CALLBACK (export_key_callback), ctx);
        export_key_callback (pop, ctx);
    
        return SEAHORSE_OPERATION (pop);
}

static SeahorseOperation*          
seahorse_pgp_source_remove (SeahorseSource *sksrc, SeahorseObject *sobj)
{
	SeahorseObject *parent;
	GError *error = NULL;
	gpgme_error_t gerr;
    
	g_return_val_if_fail (SEAHORSE_IS_SOURCE (sksrc), NULL);
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (sobj), NULL);
	g_return_val_if_fail (seahorse_object_get_source (sobj) == sksrc, NULL);

	if (SEAHORSE_IS_PGP_UID (sobj)) {
		parent = seahorse_object_get_parent (sobj);
		g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (parent), NULL);
		gerr = seahorse_pgp_key_op_del_uid (SEAHORSE_PGP_KEY (parent), 
		                                    SEAHORSE_PGP_UID (sobj)->index + 1);
	} else if (SEAHORSE_IS_PGP_KEY (sobj)) {
		if (seahorse_object_get_usage (sobj) == SEAHORSE_USAGE_PRIVATE_KEY) 
			gerr = seahorse_pgp_key_pair_op_delete (SEAHORSE_PGP_KEY (sobj));
		else 
			gerr = seahorse_pgp_key_op_delete (SEAHORSE_PGP_KEY (sobj));
	} else {
		g_return_val_if_reached (NULL);
	}
    
	if (!GPG_IS_OK (gerr)) 
		seahorse_gpgme_to_error (gerr, &error);
	return seahorse_operation_new_complete (error);
}

/* -------------------------------------------------------------------------- 
 * FUNCTIONS
 */

/**
 * seahorse_pgp_source_new
 * 
 * Creates a new PGP key source
 * 
 * Returns: The key source.
 **/
SeahorsePGPSource*
seahorse_pgp_source_new (void)
{
   return g_object_new (SEAHORSE_TYPE_PGP_SOURCE, NULL);
}   

gpgme_ctx_t          
seahorse_pgp_source_new_context ()
{
    gpgme_ctx_t ctx = NULL;
    g_return_val_if_fail (GPG_IS_OK (init_gpgme (&ctx)), NULL);
    return ctx;
}
