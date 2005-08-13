/*
 * Seahorse
 *
 * Copyright (C) 2004 Nate Nielsen
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

#include <stdlib.h>
#include <libintl.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>

/* Amount of keys to load in a batch */
#define DEFAULT_LOAD_BATCH 200

#include "seahorse-gpgmex.h"
#include "seahorse-pgp-source.h"
#include "seahorse-operation.h"
#include "seahorse-util.h"
#include "seahorse-pgp-key.h"
#include "seahorse-libdialogs.h"
#include "seahorse-gpg-options.h"

/* TODO: Verify properly that all keys we deal with are PGP keys */

enum {
	PROP_0,
    PROP_KEY_TYPE,
    PROP_LOCATION
};

/* Set to one to print refresh/monitoring status on console */
#define DEBUG_REFRESH_CODE 0

#if DEBUG_REFRESH_CODE
#define DEBUG_REFRESH(x)    g_printerr(x)
#else
#define DEBUG_REFRESH(x)
#endif

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
    
    gpgme_set_passphrase_cb (*ctx, (gpgme_passphrase_cb_t)seahorse_passphrase_get, 
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
    gboolean all;                   /* When refreshing this is the refresh all keys flag */
    GHashTable *checks;             /* When refreshing this is our set of missing keys */
END_DECLARE_OPERATION        

IMPLEMENT_OPERATION (Load, load)


/* -----------------------------------------------------------------------------
 * PGP Source
 */
    
struct _SeahorsePGPSourcePrivate {
    guint scheduled_refresh;                /* Source for refresh timeout */
    GnomeVFSMonitorHandle *monitor_handle;  /* For monitoring the .gnupg directory */
    SeahorseMultiOperation *operations;     /* A list of all current operations */    
    GList *orphan_secret;                   /* Orphan secret keys */
};

G_DEFINE_TYPE (SeahorsePGPSource, seahorse_pgp_source, SEAHORSE_TYPE_KEY_SOURCE);

/* GObject handlers */
static void seahorse_pgp_source_dispose         (GObject *gobject);
static void seahorse_pgp_source_finalize        (GObject *gobject);
static void seahorse_pgp_source_set_property    (GObject *object, guint prop_id, 
                                                 const GValue *value, GParamSpec *pspec);
static void seahorse_pgp_source_get_property    (GObject *object, guint prop_id,
                                                 GValue *value, GParamSpec *pspec);

/* SeahorseKeySource methods */
static void                seahorse_pgp_source_stop             (SeahorseKeySource *src);
static guint               seahorse_pgp_source_get_state        (SeahorseKeySource *src);
static SeahorseOperation*  seahorse_pgp_source_load             (SeahorseKeySource *src,
                                                                 SeahorseKeySourceLoad load,
                                                                 const gchar *key);
static SeahorseOperation*  seahorse_pgp_source_import           (SeahorseKeySource *sksrc, 
                                                                 gpgme_data_t data);
static SeahorseOperation*  seahorse_pgp_source_export           (SeahorseKeySource *sksrc, 
                                                                 GList *keys,
                                                                 gboolean complete, 
                                                                 gpgme_data_t data);

/* Other forward decls */
static void                monitor_gpg_homedir                  (GnomeVFSMonitorHandle *handle, 
                                                                 const gchar *monitor_uri,
                                                                 const gchar *info_uri, 
                                                                 GnomeVFSMonitorEventType event_type,
                                                                 gpointer user_data);
static void                cancel_scheduled_refresh             (SeahorsePGPSource *psrc);
                                                                 
static GObjectClass *parent_class = NULL;

/* Initialize the basic class stuff */
static void
seahorse_pgp_source_class_init (SeahorsePGPSourceClass *klass)
{
    GObjectClass *gobject_class;
    SeahorseKeySourceClass *key_class;
   
    parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->dispose = seahorse_pgp_source_dispose;
    gobject_class->finalize = seahorse_pgp_source_finalize;
    gobject_class->set_property = seahorse_pgp_source_set_property;
    gobject_class->get_property = seahorse_pgp_source_get_property;
    
    key_class = SEAHORSE_KEY_SOURCE_CLASS (klass);    
    key_class->load = seahorse_pgp_source_load;
    key_class->stop = seahorse_pgp_source_stop;
    key_class->get_state = seahorse_pgp_source_get_state;
    key_class->import = seahorse_pgp_source_import;
    key_class->export = seahorse_pgp_source_export;
 
    g_object_class_install_property (gobject_class, PROP_KEY_TYPE,
        g_param_spec_uint ("key-type", "Key Type", "Key type that originates from this key source.", 
                           0, G_MAXUINT, SKEY_INVALID, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_LOCATION,
        g_param_spec_uint ("location", "Key Location", "Where the key is stored. See SeahorseKeyLoc", 
                           0, G_MAXUINT, SKEY_LOC_UNKNOWN, G_PARAM_READABLE));    
}

/* init context, private vars, set prefs, connect signals */
static void
seahorse_pgp_source_init (SeahorsePGPSource *psrc)
{
    gpgme_error_t err;
    const gchar *gpg_homedir;
    GnomeVFSResult res;
    gchar *uri;
    
    err = init_gpgme (&(psrc->gctx));
    g_return_if_fail (GPG_IS_OK (err));
    
    /* init private vars */
    psrc->pv = g_new0 (SeahorsePGPSourcePrivate, 1);
    
    psrc->pv->operations = seahorse_multi_operation_new ();
    
    psrc->pv->scheduled_refresh = 0;
    psrc->pv->monitor_handle = NULL;
    
    gpg_homedir = seahorse_gpg_homedir ();
    uri = gnome_vfs_make_uri_canonical (gpg_homedir);
    g_return_if_fail (uri != NULL);
    
    res = gnome_vfs_monitor_add (&(psrc->pv->monitor_handle), uri, 
                                 GNOME_VFS_MONITOR_DIRECTORY, 
                                 monitor_gpg_homedir, psrc);
    g_free (uri);
    
    if (res != GNOME_VFS_OK) {
        psrc->pv->monitor_handle = NULL;        
        g_return_if_reached ();
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
        if(!seahorse_operation_is_done (SEAHORSE_OPERATION (psrc->pv->operations)))
            seahorse_operation_cancel (SEAHORSE_OPERATION (psrc->pv->operations));
        g_object_unref (psrc->pv->operations);
        psrc->pv->operations = NULL;
    }

    cancel_scheduled_refresh (psrc);    
    
    if (psrc->pv->monitor_handle) {
        gnome_vfs_monitor_cancel (psrc->pv->monitor_handle);
        psrc->pv->monitor_handle = NULL;
    }
    
    for (l = psrc->pv->orphan_secret; l; l = g_list_next (l)) 
        g_object_unref (l->data);
    g_list_free (psrc->pv->orphan_secret);
    psrc->pv->orphan_secret = NULL;

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
        g_value_set_uint (value, SKEY_PGP);
        break;
    case PROP_LOCATION:
        g_value_set_uint (value, SKEY_LOC_LOCAL);
        break;
    }
}

/* --------------------------------------------------------------------------
 * HELPERS 
 */

/* Remove the given key from the context */
static void
remove_key_from_context (const gchar *id, SeahorseKey *dummy, SeahorsePGPSource *psrc)
{
    /* This function gets called as a GHRFunc on the lctx->checks hashtable. */
    
    SeahorseKey *skey;
    
    skey = seahorse_context_get_key (SCTX_APP (), SEAHORSE_KEY_SOURCE (psrc), id);
    if (skey != NULL)
        seahorse_context_remove_key (SCTX_APP (), skey);
}

static gboolean
have_key_in_context (SeahorsePGPSource *psrc, const gchar *id, gboolean secret)
{
    SeahorseKey *skey;
    g_return_val_if_fail (SEAHORSE_IS_PGP_SOURCE (psrc), FALSE);
    skey = seahorse_context_get_key (SCTX_APP (), SEAHORSE_KEY_SOURCE (psrc), id);
    return skey && (!secret || seahorse_key_get_etype (skey) == SKEY_PRIVATE);
}

/* Add a key to the context  */
static void
add_key_to_context (SeahorsePGPSource *psrc, gpgme_key_t key)
{
    SeahorsePGPKey *pkey = NULL;
    SeahorsePGPKey *prev;
    const gchar *id;
    GList *l;
    
    id = seahorse_pgp_key_get_id (key, 0);
    
    g_return_if_fail (SEAHORSE_IS_PGP_SOURCE (psrc));
    prev = SEAHORSE_PGP_KEY (seahorse_context_get_key (SCTX_APP (), SEAHORSE_KEY_SOURCE (psrc), id));
    
    /* Check if we can just replace the key on the object */
    if (prev != NULL) {
        if (key->secret) 
            g_object_set (prev, "seckey", key, NULL);
        else
            g_object_set (prev, "pubkey", key, NULL);
        return;
    }
    
    /* Create a new key with secret */    
    if (key->secret) {
        pkey = seahorse_pgp_key_new (SEAHORSE_KEY_SOURCE (psrc), NULL, key);
        
        /* Since we don't have a public key yet, save this away */
        psrc->pv->orphan_secret = g_list_append (psrc->pv->orphan_secret, pkey);
        
    /* Just a new public key */
    } else {
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
            pkey = seahorse_pgp_key_new (SEAHORSE_KEY_SOURCE (psrc), key, NULL);

        /* Add to context */ 
        seahorse_context_add_key (SCTX_APP (), SEAHORSE_KEY (pkey));
    }
 
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
    seahorse_key_source_load_async (SEAHORSE_KEY_SOURCE (psrc), SKSRC_LOAD_ALL, NULL);    
    
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
monitor_gpg_homedir (GnomeVFSMonitorHandle *handle, const gchar *monitor_uri,
                     const gchar *info_uri, GnomeVFSMonitorEventType event_type,
                     gpointer user_data)
{
    SeahorsePGPSource *psrc = SEAHORSE_PGP_SOURCE (user_data);
    
    if (g_str_has_suffix (info_uri, SEAHORSE_EXT_GPG) && 
        (event_type == GNOME_VFS_MONITOR_EVENT_CREATED || 
         event_type == GNOME_VFS_MONITOR_EVENT_CHANGED ||
         event_type == GNOME_VFS_MONITOR_EVENT_DELETED)) {
        if (psrc->pv->scheduled_refresh == 0) {
            DEBUG_REFRESH ("scheduling refresh event due to file changes\n");
            psrc->pv->scheduled_refresh = g_timeout_add (500, scheduled_refresh, psrc);
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
    lop->all = FALSE;
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

    G_OBJECT_CLASS (operation_parent_class)->dispose (gobject);  
}

static void 
seahorse_load_operation_finalize (GObject *gobject)
{
    SeahorseLoadOperation *lop = SEAHORSE_LOAD_OPERATION (gobject);
    
    if (lop->checks)    
        g_hash_table_destroy (lop->checks);

    g_assert (lop->stag == 0);
    g_assert (lop->psrc == NULL);

    G_OBJECT_CLASS (operation_parent_class)->finalize (gobject);  
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
    gpgme_key_t key;
    guint batch;
    const gchar *id;
    gchar *t;
    
    g_return_val_if_fail (SEAHORSE_IS_LOAD_OPERATION (lop), FALSE);
    
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
        
        /* During a refresh if only new or removed keys */
        if (lop->checks) {

            /* Make note that this key exists in keyring */
            g_hash_table_remove (lop->checks, id);

            /* When not doing all keys and already have ... */
            if (!lop->all && have_key_in_context (lop->psrc, id, lop->secret)) {

                /* ... then just ignore */
                gpgmex_key_unref (key);
                continue;
            }
        }
        
        add_key_to_context (lop->psrc, key);
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
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (lop), t, 0, 0);
    g_free (t);
    
    return TRUE; 
}

static SeahorseLoadOperation*
seahorse_load_operation_start (SeahorsePGPSource *psrc, const gchar *pattern,
                               gboolean secret, gboolean refresh, gboolean all)
{
    SeahorsePGPSourcePrivate *priv;
    SeahorseLoadOperation *lop;
    gpgme_error_t err;
    GList *keys, *l;
    SeahorseKey *skey;
    
    g_return_val_if_fail (SEAHORSE_IS_PGP_SOURCE (psrc), NULL);
    priv = psrc->pv;

    lop = g_object_new (SEAHORSE_TYPE_LOAD_OPERATION, NULL);    
    lop->psrc = psrc;
    lop->secret = secret;
    g_object_ref (psrc);
    
    /* When loading a specific key, we load extra info */
    if (pattern != NULL) {
        gpgme_set_keylist_mode (lop->ctx, GPGME_KEYLIST_MODE_SIGS | 
                gpgme_get_keylist_mode (lop->ctx));
    }
    
    /* Start the key listing */
    err = gpgme_op_keylist_start (lop->ctx, pattern, secret);
    g_return_val_if_fail (GPG_IS_OK (err), lop);
    
    if (refresh) {
     
        lop->all = all;
        
        /* This hashtable contains only allocated strings with no 
         * values. We allocate the strings in case a key goes away
         * while we're holding the ids in this table */
        lop->checks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
        
        keys = seahorse_context_get_keys (SCTX_APP (), SEAHORSE_KEY_SOURCE (psrc));
        for (l = keys; l; l = g_list_next (l)) {
            skey = SEAHORSE_KEY (l->data);
            if (secret && seahorse_key_get_etype (skey) != SKEY_PRIVATE) 
                continue;
            g_hash_table_insert (lop->checks, g_strdup (seahorse_key_get_keyid (l->data)), NULL);
        }
        
        g_list_free (keys);
        
    }
    
    seahorse_operation_mark_start (SEAHORSE_OPERATION (lop));
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (lop), _("Loading Keys..."), 0, 0);
    
    /* Run one iteration of the handler */
    keyload_handler (lop);

    return lop;
}    

/* --------------------------------------------------------------------------
 * METHODS
 */

static SeahorseOperation*
seahorse_pgp_source_load (SeahorseKeySource *src, SeahorseKeySourceLoad load,
                          const gchar *match)
{
    SeahorsePGPSource *psrc;
    SeahorseLoadOperation *lop;
    gboolean all, ref;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (src), NULL);
    psrc = SEAHORSE_PGP_SOURCE (src);
    
    /* Schedule a dummy refresh. This blocks all monitoring for a while */
    cancel_scheduled_refresh (psrc);
    psrc->pv->scheduled_refresh = g_timeout_add (500, scheduled_dummy, psrc);
    DEBUG_REFRESH ("scheduled a dummy refresh\n");
    
    ref = (load == SKSRC_LOAD_NEW);
    all = (load == SKSRC_LOAD_ALL);
    
    if (ref || all) {
        ref = TRUE;
        match = NULL;
    }

    DEBUG_REFRESH ("refreshing keys...\n");

    /* Secret keys */
    lop = seahorse_load_operation_start (psrc, match, FALSE, ref, all);
    seahorse_multi_operation_take (psrc->pv->operations, SEAHORSE_OPERATION (lop));

    /* Public keys */
    lop = seahorse_load_operation_start (psrc, match, TRUE, ref, all);
    seahorse_multi_operation_take (psrc->pv->operations, SEAHORSE_OPERATION (lop));

    g_object_ref (psrc->pv->operations);
    return SEAHORSE_OPERATION (psrc->pv->operations);
}

static void
seahorse_pgp_source_stop (SeahorseKeySource *src)
{
    SeahorsePGPSource *psrc;
    
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (src));
    psrc = SEAHORSE_PGP_SOURCE (src);
    
    if(!seahorse_operation_is_done (SEAHORSE_OPERATION (psrc->pv->operations)))
        seahorse_operation_cancel (SEAHORSE_OPERATION (psrc->pv->operations));
}

static guint
seahorse_pgp_source_get_state (SeahorseKeySource *src)
{
    SeahorsePGPSource *psrc;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (src), 0);
    psrc = SEAHORSE_PGP_SOURCE (src);
    
    return seahorse_operation_is_done (SEAHORSE_OPERATION (psrc->pv->operations)) ? 
                                       SKSRC_LOADING : 0;
}

static SeahorseOperation* 
seahorse_pgp_source_import (SeahorseKeySource *sksrc, gpgme_data_t data)
{
    SeahorseOperation *operation;
    SeahorsePGPSource *psrc;
    gpgme_import_result_t results;
    gpgme_import_status_t import;
    SeahorseKey *skey;
    gpgme_error_t gerr;
    gpgme_ctx_t new_ctx;
    GList *keys = NULL;
    GError *err = NULL;
    
    g_return_val_if_fail (SEAHORSE_IS_PGP_SOURCE (sksrc), NULL);
    psrc = SEAHORSE_PGP_SOURCE (sksrc);
    
    /* Eventually this should probably be asynchronous, but for now
     * we leave it as a sync operation for simplicity. */
    
    new_ctx = seahorse_pgp_source_new_context (psrc);
    g_return_val_if_fail (new_ctx != NULL, NULL);
    
    operation = g_object_new (SEAHORSE_TYPE_OPERATION, NULL);
    seahorse_operation_mark_start (operation);
    
    gerr = gpgme_op_import (new_ctx, data);
    if (GPG_IS_OK (gerr)) {

        seahorse_key_source_load_sync (sksrc, SKSRC_LOAD_NEW, NULL);
        
        /* Figure out which keys were imported */
        results = gpgme_op_import_result (new_ctx);
        if (results) {
            for (import = results->imports; import; import = import->next) {
                if (!GPG_IS_OK (import->result))
                    continue;
                    
                skey = seahorse_context_get_key (SCTX_APP (), sksrc, import->fpr);
                if (skey != NULL)
                    keys = g_list_append (keys, skey);
            }
        }        
        
        g_object_set_data_full (G_OBJECT (operation), "result", keys, (GDestroyNotify)g_list_free);        
        seahorse_operation_mark_done (operation, FALSE, NULL);
        
    } else {
       
        /* Marks the operation as failed */
        seahorse_util_gpgme_to_error (gerr, &err);
        seahorse_operation_mark_done (operation, FALSE, err);
    }

    gpgme_release (new_ctx);
    return operation;                    
}

static SeahorseOperation* 
seahorse_pgp_source_export (SeahorseKeySource *sksrc, GList *keys, 
                            gboolean complete, gpgme_data_t data)
{
    SeahorseOperation *operation;
    SeahorsePGPSource *psrc;
    SeahorsePGPKey *pkey;
    SeahorseKey *skey;
    gpgme_error_t gerr;
    gpgme_ctx_t new_ctx;
    GError *err = NULL;
    GList *l;
    
    g_return_val_if_fail (SEAHORSE_IS_PGP_SOURCE (sksrc), NULL);
    psrc = SEAHORSE_PGP_SOURCE (sksrc);
    
    /* Eventually this should probably be asynchronous, but for now
     * we leave it as a sync operation for simplicity. */

    operation = g_object_new (SEAHORSE_TYPE_OPERATION, NULL);
    seahorse_operation_mark_start (operation);
     
    if (data == NULL) {
        
        /* Tie to operation when we create our own (auto-free) */
        gerr = gpgme_data_new (&data);
        g_return_val_if_fail (GPG_IS_OK (gerr), NULL);
        g_object_set_data_full (G_OBJECT (operation), "result", data, 
                               (GDestroyNotify)gpgme_data_release);        
    } else {
       
        /* Don't free when people pass us their own data */
        g_object_set_data (G_OBJECT (operation), "result", data);
    }
     
    new_ctx = seahorse_pgp_source_new_context (psrc);
    g_return_val_if_fail (new_ctx != NULL, NULL);

    gpgme_set_armor (new_ctx, TRUE);
    gpgme_set_textmode (new_ctx, TRUE);

    for (l = keys; l != NULL; l = g_list_next (l)) {
       
        g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (l->data), NULL);
        pkey = SEAHORSE_PGP_KEY (l->data);
        
        skey = SEAHORSE_KEY (l->data);       
        g_return_val_if_fail (skey->sksrc == sksrc, NULL);
        
        gerr = gpgme_op_export (new_ctx, skey->keyid, 0, data);

        if (!GPG_IS_OK (gerr))
            break;
        
        if (complete && skey->etype == SKEY_PRIVATE) {
            
            gerr = gpgmex_op_export_secret (new_ctx, 
                        seahorse_pgp_key_get_id (pkey->seckey, 0), data);
            
            if (!GPG_IS_OK (gerr))
                break;
        }
    }

    if (!GPG_IS_OK (gerr)) 
        seahorse_util_gpgme_to_error (gerr, &err);
                                 
    seahorse_operation_mark_done (operation, FALSE, err);
    return operation;    
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
seahorse_pgp_source_new_context (SeahorsePGPSource *source)
{
    gpgme_ctx_t ctx = NULL;
    g_return_val_if_fail (GPG_IS_OK (init_gpgme (&ctx)), NULL);
    return ctx;
}

/**
 * seahorse_pgp_source_load
 * @psrc: The PGP key source
 * @secret_only: Only load secret keys.
 * 
 * Starts a load operation on the key source.
 **/
void      
seahorse_pgp_source_loadkeys (SeahorsePGPSource *psrc, gboolean secret_only)
{
    SeahorseLoadOperation *lop;
    g_return_if_fail (SEAHORSE_IS_PGP_SOURCE (psrc));

    if (!secret_only) {
        /* Public keys */
        lop = seahorse_load_operation_start (psrc, NULL, FALSE, FALSE, FALSE);
        seahorse_multi_operation_take (psrc->pv->operations, SEAHORSE_OPERATION (lop));
    }
    
    /* Secret keys */
    lop = seahorse_load_operation_start (psrc, NULL, TRUE, FALSE, FALSE);
    seahorse_multi_operation_take (psrc->pv->operations, SEAHORSE_OPERATION (lop));
}
