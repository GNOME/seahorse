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

/* Amount of keys to load in a batch */
#define DEFAULT_LOAD_BATCH 30

#include "config.h"
#include "seahorse-gpgmex.h"
#include "seahorse-operation.h"
#include "seahorse-server-source.h"
#include "seahorse-multi-source.h"
#include "seahorse-util.h"
#include "seahorse-key.h"
#include "seahorse-key-pair.h"

enum {
    PROP_0,
    PROP_KEY_SERVER,
    PROP_LOCAL_SOURCE
};

/* -----------------------------------------------------------------------------
 *  SEARCH OPERATION     
 */
 
#define SEAHORSE_TYPE_SEARCH_OPERATION            (seahorse_search_operation_get_type ())
#define SEAHORSE_SEARCH_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SEARCH_OPERATION, SeahorseSearchOperation))
#define SEAHORSE_SEARCH_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SEARCH_OPERATION, SeahorseSearchOperationClass))
#define SEAHORSE_IS_SEARCH_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SEARCH_OPERATION))
#define SEAHORSE_IS_SEARCH_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SEARCH_OPERATION))
#define SEAHORSE_SEARCH_OPERATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SEARCH_OPERATION, SeahorseSearchOperationClass))

DECLARE_OPERATION (Search, search)
    SeahorseServerSource *ssrc; /* The source to add keys to */
    gpgmex_keyserver_op_t kop;  /* The keyserver operation */
    gpgme_error_t status;       /* The status of the operation */
    guint loaded;
    gchar *message;             /* An error message */
END_DECLARE_OPERATION 

IMPLEMENT_OPERATION (Search, search)
    
/* -----------------------------------------------------------------------------
 *  SERVER SOURCE
 */
 
struct _SeahorseServerSourcePrivate {
    SeahorseKeySource *local;
    GHashTable *keys;
    GSList *operations;
    guint progress_stag;
    gchar *server;
        
    /* Cache of the last stuff we looked up */
    gchar *last_pattern;
};

/* GObject handlers */
static void seahorse_server_source_class_init (SeahorseServerSourceClass *klass);
static void seahorse_server_source_init       (SeahorseServerSource *ssrc);
static void seahorse_server_source_dispose    (GObject *gobject);
static void seahorse_server_source_finalize   (GObject *gobject);
static void seahorse_server_set_property      (GObject *object,
                                               guint prop_id,
                                               const GValue *value,
                                               GParamSpec *pspec);
                                               
/* SeahorseKeySource methods */
static void         seahorse_server_source_refresh     (SeahorseKeySource *src,
                                                        gboolean all);
static void         seahorse_server_source_stop        (SeahorseKeySource *src);
static guint        seahorse_server_source_get_state   (SeahorseKeySource *src);
SeahorseKey*        seahorse_server_source_get_key     (SeahorseKeySource *source,
                                                        const gchar *fpr,
                                                        SeahorseKeyInfo info);
static GList*       seahorse_server_source_get_keys    (SeahorseKeySource *src,
                                                        gboolean secret_only);
static guint        seahorse_server_source_get_count   (SeahorseKeySource *src,
                                                        gboolean secret_only);
static gpgme_ctx_t  seahorse_server_source_new_context (SeahorseKeySource *src);

/* Other forward decls */
static gboolean release_key (const gchar* id, SeahorseKey *skey, SeahorseServerSource *ssrc);

static GObjectClass *parent_class = NULL;

GType
seahorse_server_source_get_type (void)
{
    static GType server_source_type = 0;
 
    if (!server_source_type) {
        
        static const GTypeInfo server_source_info = {
            sizeof (SeahorseServerSourceClass), NULL, NULL,
            (GClassInitFunc) seahorse_server_source_class_init, NULL, NULL,
            sizeof (SeahorseServerSource), 0, (GInstanceInitFunc) seahorse_server_source_init
        };
        
        server_source_type = g_type_register_static (SEAHORSE_TYPE_KEY_SOURCE, 
                                 "SeahorseServerSource", &server_source_info, 0);
    }
  
    return server_source_type;
}

/* Initialize the basic class stuff */
static void
seahorse_server_source_class_init (SeahorseServerSourceClass *klass)
{
    GObjectClass *gobject_class;
    SeahorseKeySourceClass *key_class;
   
    parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    key_class = SEAHORSE_KEY_SOURCE_CLASS (klass);
        
    key_class->refresh = seahorse_server_source_refresh;
    key_class->stop = seahorse_server_source_stop;
    key_class->get_count = seahorse_server_source_get_count;
    key_class->get_key = seahorse_server_source_get_key;
    key_class->get_keys = seahorse_server_source_get_keys;
    key_class->get_state = seahorse_server_source_get_state;
    key_class->new_context = seahorse_server_source_new_context;    
    
    gobject_class->dispose = seahorse_server_source_dispose;
    gobject_class->finalize = seahorse_server_source_finalize;
    gobject_class->set_property = seahorse_server_set_property;
    
    g_object_class_install_property (gobject_class, PROP_LOCAL_SOURCE,
            g_param_spec_pointer ("local-source", "Local Source",
                                  "Local Source that this represents",
                                  G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
                                  
    g_object_class_install_property (gobject_class, PROP_KEY_SERVER,
            g_param_spec_string ("key-server", "Key Server",
                                 "Key Server to search on", "",
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}

/* init context, private vars, set prefs, connect signals */
static void
seahorse_server_source_init (SeahorseServerSource *ssrc)
{
    /* init private vars */
    ssrc->priv = g_new0 (SeahorseServerSourcePrivate, 1);
    ssrc->priv->keys = g_hash_table_new (g_str_hash, g_str_equal);
}

/* dispose of all our internal references */
static void
seahorse_server_source_dispose (GObject *gobject)
{
    SeahorseServerSource *ssrc;
    SeahorseKeySource *sksrc;
    
    /*
     * Note that after this executes the rest of the object should
     * still work without a segfault. This basically nullifies the 
     * object, but doesn't free it.
     * 
     * This function should also be able to run multiple times.
     */
  
    ssrc = SEAHORSE_SERVER_SOURCE (gobject);
    sksrc = SEAHORSE_KEY_SOURCE (gobject);
    g_assert (ssrc->priv);
    
    /* Clear out all the operations */
    seahorse_key_source_stop (sksrc);

    /* Release our references on the keys */
    g_hash_table_foreach_remove (ssrc->priv->keys, (GHRFunc)release_key, ssrc);

    if (ssrc->priv->local) {
        g_object_unref(ssrc->priv->local);
        ssrc->priv->local = NULL;
        sksrc->ctx = NULL;
    }
    
    if (ssrc->priv->progress_stag) {
        g_source_remove (ssrc->priv->progress_stag);
        ssrc->priv->progress_stag = 0;
    }
 
    G_OBJECT_CLASS (parent_class)->dispose (gobject);
}

/* free private vars */
static void
seahorse_server_source_finalize (GObject *gobject)
{
    SeahorseServerSource *ssrc;
  
    ssrc = SEAHORSE_SERVER_SOURCE (gobject);
    g_assert (ssrc->priv);
    
    /* We should have no keys at this point */
    g_assert (g_hash_table_size (ssrc->priv->keys) == 0);
    
    g_free (ssrc->priv->server);
    g_free (ssrc->priv->last_pattern);
    g_hash_table_destroy (ssrc->priv->keys);
    g_assert (ssrc->priv->operations == NULL);
    g_assert (ssrc->priv->local == NULL);
    
    g_free (ssrc->priv);
 
    G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void 
seahorse_server_set_property (GObject *object, guint prop_id, 
                              const GValue *value, GParamSpec *pspec)
{
    SeahorseServerSource *ssrc;
    SeahorseKeySource *sksrc;
 
    ssrc = SEAHORSE_SERVER_SOURCE (object);
    sksrc = SEAHORSE_KEY_SOURCE (object);
  
    switch (prop_id) {
    case PROP_LOCAL_SOURCE:
        g_return_if_fail (ssrc->priv->local == NULL);
        ssrc->priv->local = g_value_get_pointer (value);
        g_object_ref (ssrc->priv->local);
        sksrc->ctx = ssrc->priv->local->ctx;
        g_return_if_fail (gpgme_get_protocol(sksrc->ctx) == GPGME_PROTOCOL_OpenPGP);
        break;
    case PROP_KEY_SERVER:
        g_return_if_fail (ssrc->priv->server == NULL);
        ssrc->priv->server = g_strdup (g_value_get_string (value));
        g_return_if_fail (ssrc->priv->server && ssrc->priv->server[0] != 0);
        break;
    default:
        break;
    }  
}

/* --------------------------------------------------------------------------
 * HELPERS 
 */
 
/* The progress timer */
static gboolean
progress_timer (SeahorseServerSource *ssrc)
{
    g_return_val_if_fail (SEAHORSE_IS_SERVER_SOURCE (ssrc), FALSE);
    
    if (ssrc->priv->operations) {
        seahorse_key_source_show_progress (SEAHORSE_KEY_SOURCE (ssrc), NULL, 0);
        return TRUE;
    } else {
        seahorse_key_source_show_progress (SEAHORSE_KEY_SOURCE (ssrc), NULL, -1);
        ssrc->priv->progress_stag = 0;
        return FALSE;
    }
}

/* Starts an indeterminate progress timer */
static void
start_progress_notify (SeahorseServerSource *ssrc, const gchar *msg)
{
    g_return_if_fail (SEAHORSE_IS_SERVER_SOURCE (ssrc));
        
    seahorse_key_source_show_progress (SEAHORSE_KEY_SOURCE (ssrc), msg, 0);
    
    if (!ssrc->priv->progress_stag)
        ssrc->priv->progress_stag = g_timeout_add (100, (GSourceFunc)progress_timer, ssrc);
}


/* Release a key from our internal tables and let the world know about it */
static gboolean
release_key_notify (const gchar *id, SeahorseKey *skey, SeahorseServerSource *ssrc)
{
    seahorse_key_source_removed (SEAHORSE_KEY_SOURCE (ssrc), skey);
    release_key (id, skey, ssrc);
    return TRUE;
}

/* Remove the given key from our source */
static void
remove_key_from_source (const gchar *id, SeahorseKey *dummy, SeahorseServerSource *ssrc)
{
    /* This function gets called as a GHRFunc on the lctx->checks 
     * hashtable. That hash table doesn't contain any keys, only ids,
     * so we lookup the key in the main hashtable, and use that */
     
    SeahorseKey *skey = g_hash_table_lookup (ssrc->priv->keys, id);
    if (skey != NULL) {
        g_hash_table_remove (ssrc->priv->keys, id);
        release_key_notify (id, skey, ssrc);
    }
}

/* A #SeahorseKey has been destroyed. Remove it */
static void
key_destroyed (GObject *object, SeahorseServerSource *ssrc)
{
    SeahorseKey *skey;
    skey = SEAHORSE_KEY (object);

    remove_key_from_source (seahorse_key_get_id (skey->key), skey, ssrc);
}

/* Release a key from our internal tables */
static gboolean
release_key (const gchar* id, SeahorseKey *skey, SeahorseServerSource *ssrc)
{
    g_return_val_if_fail (SEAHORSE_IS_KEY (skey), TRUE);
    g_return_val_if_fail (SEAHORSE_IS_SERVER_SOURCE (ssrc), TRUE);
    
    g_signal_handlers_disconnect_by_func (skey, key_destroyed, ssrc);
    g_object_unref (skey);
    return TRUE;
}

/* Combine information from one key and tack onto others */
static void 
combine_keys (SeahorseServerSource *ssrc, SeahorseKey *skey, gpgme_key_t key)
{
    gpgme_user_id_t uid;
    gpgme_user_id_t u;
    gpgme_subkey_t subkey;
    gpgme_subkey_t s;
    gpgme_key_t k = skey->key; 
    gboolean found;
    
    g_return_if_fail (k != NULL);
    g_return_if_fail (key != NULL);
    
    /* Go through user ids */
    for (uid = key->uids; uid != NULL; uid = uid->next) {
        g_assert (uid->uid);
        found = FALSE;
        
        for (u = k->uids; u != NULL; u = u->next) {
            g_assert (u->uid);
            
            if (strcmp (u->uid, uid->uid) == 0) {
                found = TRUE;
                break;
            }
        }
        
        if (!found)
            gpgmex_key_copy_uid (k, uid);
    }
    
    /* Go through subkeys */
    for (subkey = key->subkeys; subkey != NULL; subkey = subkey->next) {
        g_assert (subkey->fpr);
        found = FALSE;
        
        for (s = k->subkeys; s != NULL; s = s->next) {
            g_assert (s->fpr);
            
            if (strcmp (s->fpr, subkey->fpr) == 0) {
                found = TRUE;
                break;
            }
        }
        
        if (!found)
            gpgmex_key_copy_subkey (k, subkey);
    }
}

/* Add a key to our internal tables, possibly overwriting or combining with other keys  */
static void
add_key_to_source (SeahorseServerSource *ssrc, gpgme_key_t key)
{
    SeahorseKey *prev;
    SeahorseKey *skey;
    const gchar *id;
       
    g_return_if_fail (SEAHORSE_IS_SERVER_SOURCE (ssrc));

    id = seahorse_key_get_id (key);
    prev = g_hash_table_lookup (ssrc->priv->keys, id);
    
    if (prev != NULL) {
        combine_keys (ssrc, prev, key);
        seahorse_key_changed (prev, SKEY_CHANGE_UIDS);
        return;    
    }

    /* A public key */
    skey = seahorse_key_new (SEAHORSE_KEY_SOURCE (ssrc), key);

    /* Add to lookups */ 
    g_hash_table_replace (ssrc->priv->keys, (gpointer)id, skey);     

    /* This stuff is 'undone' in release_key */
    g_object_ref (skey);
    g_signal_connect_after (skey, "destroy", G_CALLBACK (key_destroyed), ssrc);            
    
    /* notify observers */
    seahorse_key_source_added (SEAHORSE_KEY_SOURCE (ssrc), skey);
}
 
/* Callback for copying our internal key table to a list */
static void
keys_to_list (const gchar *id, SeahorseKey *skey, GList **l)
{
    *l = g_list_append (*l, skey);
}

/* Called when a key load operation is done */
static void 
keysearch_done (SeahorseOperation *operation, SeahorseServerSource *ssrc)
{
    g_return_if_fail (SEAHORSE_IS_SERVER_SOURCE (ssrc));

    /* Marks us as done as far as the key source is concerned */
    ssrc->priv->operations = 
        seahorse_operation_list_remove (ssrc->priv->operations, operation);
}

static void
add_search_operation (SeahorseServerSource *ssrc, SeahorseSearchOperation *sop)
{
    g_return_if_fail (SEAHORSE_IS_SERVER_SOURCE (ssrc));
    g_return_if_fail (SEAHORSE_IS_SEARCH_OPERATION (sop));
    
    if (seahorse_operation_is_done (SEAHORSE_OPERATION (sop))) {
        g_object_unref (sop);
    } else {
        ssrc->priv->operations = seahorse_operation_list_add (ssrc->priv->operations, SEAHORSE_OPERATION (sop));
        g_signal_connect (sop, "done", G_CALLBACK (keysearch_done), ssrc);         
        start_progress_notify (ssrc, _("Searching for remote keys..."));
    }
}


/* -----------------------------------------------------------------------------
 *  SEARCH OPERATION STUFF 
 */
 
static void 
seahorse_search_operation_init (SeahorseSearchOperation *sop)
{
    sop->status = GPG_OK;
}

static void 
seahorse_search_operation_dispose (GObject *gobject)
{
    SeahorseSearchOperation *sop = SEAHORSE_SEARCH_OPERATION (gobject);
    
    if (sop->ssrc) {
        g_object_unref (sop->ssrc);
        sop->ssrc = NULL;
    }
    
    G_OBJECT_CLASS (operation_parent_class)->dispose (gobject);  
}

static void 
seahorse_search_operation_finalize (GObject *gobject)
{
    SeahorseSearchOperation *sop = SEAHORSE_SEARCH_OPERATION (gobject);

    g_assert (sop->ssrc == NULL);
        
    if (sop->message)
        g_free (sop->message);
        
    G_OBJECT_CLASS (operation_parent_class)->finalize (gobject);  
}

static void 
seahorse_search_operation_cancel (SeahorseOperation *operation)
{
    SeahorseSearchOperation *sop;
    
    g_return_if_fail (SEAHORSE_IS_SEARCH_OPERATION (operation));
    sop = SEAHORSE_SEARCH_OPERATION (operation);
    
    if (sop->kop) {
        gpgmex_keyserver_op_t kop = sop->kop;
        sop->kop = NULL;
        gpgmex_keyserver_cancel (kop);
    }

    seahorse_operation_mark_done (operation);
}


/* Called when a key is found and loaded from the keyserver */
static void
keyserver_listed_key (gpgme_ctx_t ctx, gpgmex_keyserver_op_t op, gpgme_key_t key,
                      unsigned int total, void *userdata)
{
    SeahorseSearchOperation *sop;
    
    g_return_if_fail (SEAHORSE_IS_SEARCH_OPERATION (userdata));
    sop = SEAHORSE_SEARCH_OPERATION (userdata);
    
    add_key_to_source (sop->ssrc, key);
    sop->loaded++;
    gpgmex_key_unref (key);
}

/*
 * This is called once a keyserver operation is done. This is the lower level
 * callback called by GPGMEX. It should mark its parent seahorse operation as
 * done.
 */
static void
keyserver_list_done (gpgme_ctx_t ctx, gpgmex_keyserver_op_t op, gpgme_error_t status,
                     const char *message, void *userdata)
{
    SeahorseSearchOperation *sop;
    gchar *t;
    
    g_return_if_fail (SEAHORSE_IS_SEARCH_OPERATION (userdata));
    sop = SEAHORSE_SEARCH_OPERATION (userdata);

    sop->status = status;
    sop->kop = NULL;
    
    /* Successful completion */
    if (GPG_IS_OK (status)) {
        t = g_strdup_printf (ngettext("Found %d key", "Found %d keys", sop->loaded), sop->loaded);
        
    /* There was an error */            
    } else {
        
        t = g_strdup_printf (_("Couldn't search keyserver: %s"), message ? message : "");
        g_warning (t);
    }
        
    seahorse_key_source_show_progress (SEAHORSE_KEY_SOURCE (sop->ssrc), t, -1);
    g_free (t);
    
    seahorse_operation_mark_done (SEAHORSE_OPERATION (sop));
}

static SeahorseSearchOperation*
seahorse_search_operation_start (SeahorseServerSource *ssrc, const gchar *pattern)
{
    SeahorseServerSourcePrivate *priv;
    SeahorseSearchOperation *sop;
    
    g_return_val_if_fail (SEAHORSE_IS_SERVER_SOURCE (ssrc), NULL);
    priv = ssrc->priv;

    g_return_val_if_fail (priv->server != NULL && priv->server[0] != 0, NULL);
    
    sop = g_object_new (SEAHORSE_TYPE_SEARCH_OPERATION, NULL);    
    sop->ssrc = ssrc;
    g_object_ref (ssrc);
    
    gpgmex_keyserver_start_list (SEAHORSE_KEY_SOURCE (ssrc)->ctx, priv->server, 
                                 pattern, GPGMEX_KEYLIST_REVOKED, keyserver_listed_key, 
                                 keyserver_list_done, sop, &(sop->kop));

    seahorse_operation_mark_start (SEAHORSE_OPERATION (sop));
    return sop;
}    


/* --------------------------------------------------------------------------
 * METHODS
 */

static void         
seahorse_server_source_refresh (SeahorseKeySource *src, gboolean all)
{
    SeahorseServerSource *ssrc;
    
    g_return_if_fail (ssrc->priv->last_pattern);
    g_return_if_fail (ssrc->priv->server);
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (src));

    ssrc = SEAHORSE_SERVER_SOURCE (src);

    /* Stop all operations */
    seahorse_server_source_stop (src);

    /* Remove all keys */    
    g_hash_table_foreach_remove (ssrc->priv->keys, (GHRFunc)release_key_notify, ssrc);
    
    /* Now start our load again */
    seahorse_server_source_search (ssrc, ssrc->priv->last_pattern);
}

static void
seahorse_server_source_stop (SeahorseKeySource *src)
{
    SeahorseServerSource *ssrc;
    gboolean found;
    
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (src));
    ssrc = SEAHORSE_SERVER_SOURCE (src);

    found = (ssrc->priv->operations != NULL);
    seahorse_operation_list_cancel (ssrc->priv->operations);
    ssrc->priv->operations = seahorse_operation_list_purge (ssrc->priv->operations);
    
    if (found)
        seahorse_key_source_show_progress (src, _("Load Cancelled"), -1);
}

static guint
seahorse_server_source_get_count (SeahorseKeySource *src,
                                  gboolean secret_only)
{
    SeahorseServerSource *ssrc;
    guint n = 0;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (src), 0);
    ssrc = SEAHORSE_SERVER_SOURCE (src);

    /* Note that we don't deal with secret keys */    
    if (!secret_only)
        n = g_hash_table_size (ssrc->priv->keys);

    return n;
}

SeahorseKey*        
seahorse_server_source_get_key (SeahorseKeySource *src, const gchar *fpr, 
                                SeahorseKeyInfo info)
{
    SeahorseServerSource *ssrc;
    SeahorseKey *skey;
    gchar *fingerprint;

    g_return_val_if_fail (fpr != NULL && fpr[0] != 0, NULL);
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (src), NULL);
    ssrc = SEAHORSE_SERVER_SOURCE (src);
    
    /*
     * Note that there's no way to request a key from a keyserver
     * by fingerprint through gpgkeys_* plugins. So we just resort
     * to listing our searched keys. Now if they're asking for an 
     * upgrade (ie: import) then we can do that :) 
     */

    skey = g_hash_table_lookup (ssrc->priv->keys, fpr);

    /* If we have enough info on this key then just return */
    if (skey && info <= seahorse_key_get_loaded_info(skey))
        return skey;    
     
    /* Anything at this level or below we can't do much about */
    if (info <= SKEY_INFO_REMOTE)
        return skey;

    /* We need to backup the fingerprint, as the old key can be freed */
    fingerprint = g_strdup (fpr);
           
    /* See if our local key source has it */    
    skey = seahorse_key_source_get_key (ssrc->priv->local, fpr, info);
    
    /* Now they want a key imported, lets try and do that */
    /* TODO: We don't support key importing yet */
    g_warning ("TODO: key importing not supported yet");
       
    g_free (fingerprint); 
    return skey;
}

static GList*
seahorse_server_source_get_keys (SeahorseKeySource *src,
                                 gboolean secret_only)  
{
    SeahorseServerSource *ssrc;
    GList *l = NULL;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (src), NULL);
    ssrc = SEAHORSE_SERVER_SOURCE (src);
    
    /* We never deal with secret keys */
    if (!secret_only)
        g_hash_table_foreach (ssrc->priv->keys, (GHFunc)keys_to_list, &l);
        
    return l;
}

static guint
seahorse_server_source_get_state (SeahorseKeySource *src)
{
    SeahorseServerSource *ssrc;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (src), 0);
    ssrc = SEAHORSE_SERVER_SOURCE (src);
    
    guint state = SEAHORSE_KEY_SOURCE_REMOTE;
    if (ssrc->priv->operations)
        state |= SEAHORSE_KEY_SOURCE_LOADING;
    return state;
}

static gpgme_ctx_t  
seahorse_server_source_new_context (SeahorseKeySource *src)
{
    SeahorseServerSource *ssrc;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (src), 0);
    ssrc = SEAHORSE_SERVER_SOURCE (src);

    /* We generate contexts from our local source */
    return seahorse_key_source_new_context (ssrc->priv->local);
}

/* -------------------------------------------------------------------------- 
 * FUNCTIONS
 */

/**
 * seahorse_server_source_new
 * @sksrc: A local source for this server source to be based on.
 * @server: The server to search
 * 
 * Creates a new Server key source
 * 
 * Returns: The key source.
 **/
SeahorseServerSource*
seahorse_server_source_new (SeahorseKeySource *sksrc, const gchar *server)
{
    g_return_val_if_fail (server != NULL, NULL);
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc), NULL);
    
    if (SEAHORSE_IS_MULTI_SOURCE (sksrc)) {
        sksrc = seahorse_multi_source_get_primary (SEAHORSE_MULTI_SOURCE (sksrc));
        g_assert (sksrc != NULL);
    }
    
    return g_object_new (SEAHORSE_TYPE_SERVER_SOURCE, "local-source", sksrc, 
                         "key-server", server, NULL);
}   

/**
 * seahorse_search_source_search
 * @psrc: The Server key source
 * @pattern: Pattern to match keys with
 * 
 * Starts a load operation on the key source for keys matching the pattern.
 **/
void
seahorse_server_source_search (SeahorseServerSource *ssrc, const gchar *pattern)
{
    SeahorseSearchOperation *sop;
    gchar *t;
    
    g_return_if_fail (SEAHORSE_IS_SERVER_SOURCE (ssrc));
    g_return_if_fail (pattern != NULL && pattern[0] != 0);
    
    /* Round about because we could be passed ssrc->priv->last_xxxx */

    t = g_strdup (pattern);
    g_free (ssrc->priv->last_pattern);
    ssrc->priv->last_pattern = t;
   
    sop = seahorse_search_operation_start (ssrc, pattern);
    add_search_operation (ssrc, sop);
}
