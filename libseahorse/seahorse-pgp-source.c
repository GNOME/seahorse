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

#include "seahorse-gpgmex.h"
#include "seahorse-pgp-source.h"
#include "seahorse-operation.h"
#include "seahorse-util.h"
#include "seahorse-key.h"
#include "seahorse-key-pair.h"
#include "seahorse-libdialogs.h"


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
    GHashTable *keys;
    GSList *operations;
};

/* GObject handlers */
static void seahorse_pgp_source_class_init (SeahorsePGPSourceClass *klass);
static void seahorse_pgp_source_init       (SeahorsePGPSource *psrc);
static void seahorse_pgp_source_dispose    (GObject *gobject);
static void seahorse_pgp_source_finalize   (GObject *gobject);

/* SeahorseKeySource methods */
static void         seahorse_pgp_source_refresh     (SeahorseKeySource *src,
                                                     gboolean all);
static void         seahorse_pgp_source_stop        (SeahorseKeySource *src);
static guint        seahorse_pgp_source_get_state   (SeahorseKeySource *src);
SeahorseKey*        seahorse_pgp_source_get_key     (SeahorseKeySource *source,
                                                     const gchar *fpr,
                                                     SeahorseKeyInfo info);
static GList*       seahorse_pgp_source_get_keys    (SeahorseKeySource *src,
                                                     gboolean secret_only);
static guint        seahorse_pgp_source_get_count   (SeahorseKeySource *src,
                                                     gboolean secret_only);
static gpgme_ctx_t  seahorse_pgp_source_new_context (SeahorseKeySource *src);

/* Other forward decls */
static gboolean release_key (const gchar* id, SeahorseKey *skey, SeahorsePGPSource *psrc);

static GObjectClass *parent_class = NULL;

GType
seahorse_pgp_source_get_type (void)
{
    static GType pgp_source_type = 0;
 
    if (!pgp_source_type) {
        
        static const GTypeInfo pgp_source_info = {
            sizeof (SeahorsePGPSourceClass), NULL, NULL,
            (GClassInitFunc) seahorse_pgp_source_class_init, NULL, NULL,
            sizeof (SeahorsePGPSource), 0, (GInstanceInitFunc) seahorse_pgp_source_init
        };
        
        pgp_source_type = g_type_register_static (SEAHORSE_TYPE_KEY_SOURCE, 
                                "SeahorsePGPSource", &pgp_source_info, 0);
    }
  
    return pgp_source_type;
}

/* Initialize the basic class stuff */
static void
seahorse_pgp_source_class_init (SeahorsePGPSourceClass *klass)
{
    GObjectClass *gobject_class;
    SeahorseKeySourceClass *key_class;
   
    parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    key_class = SEAHORSE_KEY_SOURCE_CLASS (klass);
    
    key_class->refresh = seahorse_pgp_source_refresh;
    key_class->stop = seahorse_pgp_source_stop;
    key_class->get_count = seahorse_pgp_source_get_count;
    key_class->get_key = seahorse_pgp_source_get_key;
    key_class->get_keys = seahorse_pgp_source_get_keys;
    key_class->get_state = seahorse_pgp_source_get_state;
    key_class->new_context = seahorse_pgp_source_new_context;    
    
    gobject_class->dispose = seahorse_pgp_source_dispose;
    gobject_class->finalize = seahorse_pgp_source_finalize;
}

/* init context, private vars, set prefs, connect signals */
static void
seahorse_pgp_source_init (SeahorsePGPSource *psrc)
{
    gpgme_error_t err;
    err = init_gpgme (&(SEAHORSE_KEY_SOURCE (psrc)->ctx));
    g_return_if_fail (GPG_IS_OK (err));
    
    /* init private vars */
    psrc->priv = g_new0 (SeahorsePGPSourcePrivate, 1);
    
    /* Note that we free our own key values, but keys are free automatically */
    psrc->priv->keys = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

/* dispose of all our internal references */
static void
seahorse_pgp_source_dispose (GObject *gobject)
{
    SeahorsePGPSource *psrc;
    
    /*
     * Note that after this executes the rest of the object should
     * still work without a segfault. This basically nullifies the 
     * object, but doesn't free it.
     * 
     * This function should also be able to run multiple times.
     */
  
    psrc = SEAHORSE_PGP_SOURCE (gobject);
    g_assert (psrc->priv);
    
    /* Clear out all operations */
    seahorse_key_source_stop (SEAHORSE_KEY_SOURCE (psrc));

    /* Release our references on the keys */
    g_hash_table_foreach_remove (psrc->priv->keys, (GHRFunc)release_key, psrc);
 
    G_OBJECT_CLASS (parent_class)->dispose (gobject);
}

/* free private vars */
static void
seahorse_pgp_source_finalize (GObject *gobject)
{
    SeahorsePGPSource *psrc;
  
    psrc = SEAHORSE_PGP_SOURCE (gobject);
    g_assert (psrc->priv);
    
    /* We should have no keys at this point */
    g_assert (g_hash_table_size (psrc->priv->keys) == 0);
    
    g_hash_table_destroy (psrc->priv->keys);
    
    g_free (psrc->priv);
 
    G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

/* --------------------------------------------------------------------------
 * HELPERS 
 */
 
/* Release a key from our internal tables and let the world know about it */
static gboolean
release_key_notify (const gchar *id, SeahorseKey *skey, SeahorsePGPSource *psrc)
{
    seahorse_key_source_removed (SEAHORSE_KEY_SOURCE (psrc), skey);
    release_key (id, skey, psrc);
    return TRUE;
}

/* Remove the given key from our source */
static void
remove_key_from_source (const gchar *id, SeahorseKey *dummy, SeahorsePGPSource *psrc)
{
    /* This function gets called as a GHRFunc on the lctx->checks 
     * hashtable. That hash table doesn't contain any keys, only ids,
     * so we lookup the key in the main hashtable, and use that */
     
    SeahorseKey *skey = g_hash_table_lookup (psrc->priv->keys, id);
    if (skey != NULL) {
        g_hash_table_remove (psrc->priv->keys, id);
        release_key_notify (id, skey, psrc);
    }
}

/* A #SeahorseKey has changed. Update it. */
static void
key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorsePGPSource *psrc)
{
    /* We need to fix these key change flags. Currently the only thing
     * that sets 'ALL' is when we replace a key in an skey. We don't 
     * need to reload in that case */
     
    if (change != SKEY_CHANGE_ALL)
        seahorse_pgp_source_load_matching (psrc, seahorse_key_get_id (skey->key));
}

/* A #SeahorseKey has been destroyed. Remove it */
static void
key_destroyed (GObject *object, SeahorsePGPSource *psrc)
{
    SeahorseKey *skey;
    skey = SEAHORSE_KEY (object);

    remove_key_from_source (seahorse_key_get_id (skey->key), skey, psrc);
}

/* Release a key from our internal tables */
static gboolean
release_key (const gchar* id, SeahorseKey *skey, SeahorsePGPSource *psrc)
{
    g_return_val_if_fail (SEAHORSE_IS_KEY (skey), TRUE);
    g_return_val_if_fail (SEAHORSE_IS_PGP_SOURCE (psrc), TRUE);
    
    g_signal_handlers_disconnect_by_func (skey, key_changed, psrc);
    g_signal_handlers_disconnect_by_func (skey, key_destroyed, psrc);
    g_object_unref (skey);
    return TRUE;
}

static gboolean
have_key_in_source (SeahorsePGPSource *psrc, const gchar *id)
{
    g_return_val_if_fail (SEAHORSE_IS_PGP_SOURCE (psrc), FALSE);
    return (g_hash_table_lookup (psrc->priv->keys, id) != NULL);
}

/* Add a key to our internal tables, possibly overwriting or combining with other keys  */
static void
add_key_to_source (SeahorsePGPSource *psrc, gpgme_key_t key)
{
    SeahorseKey *prev;
    SeahorseKey *skey;
    const gchar *id;
    
    id = seahorse_key_get_id (key);
    
    g_return_if_fail (SEAHORSE_IS_PGP_SOURCE (psrc));
    prev = g_hash_table_lookup (psrc->priv->keys, id);
    
    /* Check if we can just replace the key on the object */
    if (prev != NULL) {
        if (key->secret && SEAHORSE_IS_KEY_PAIR (prev)) {
            g_object_set (prev, "secret", key, NULL);
            return;
        } else if (!key->secret) {
            g_object_set (prev, "key", key, NULL);
            return;
        }
    }
        
    /* 
     * When listing the keys we get public and private keys separately
     * which is a bummer. We need to pair up any private keys with 
     * their appropriate public keys. 
     * 
     * Because a secret key without a public key is an invalid state 
     * as far as seahorse is concerned, we need to perform additional
     * logic to find the public key immediately when a secret key is loaded.
     */
     
    if (key->secret) {

        /* We make a key pair. If no public key was present, then just 
         * make a key pair with the secret key twice. The public key
         * will be loaded later and fix it up. */
        skey = seahorse_key_pair_new (SEAHORSE_KEY_SOURCE (psrc), 
                                      prev ? prev->key : key, key);

    } else {
       
        /* A public key */
        skey = seahorse_key_new (SEAHORSE_KEY_SOURCE (psrc), key);
    }

    /* If we had a previous key then remove it */
    if (prev) 
        remove_key_from_source (id, prev, psrc);
        
    /* Add to lookups */ 
    g_hash_table_replace (psrc->priv->keys, g_strdup (id), skey);     

    /* This stuff is 'undone' in release_key */
    g_object_ref (skey);
    g_signal_connect (skey, "changed", G_CALLBACK (key_changed), psrc);             
    g_signal_connect_after (skey, "destroy", G_CALLBACK (key_destroyed), psrc);            
    
    /* notify observers */
    seahorse_key_source_added (SEAHORSE_KEY_SOURCE (psrc), skey);
}

/* Callback for copying our internal key table to a list */
static void
keys_to_list (const gchar *id, SeahorseKey *skey, GList **l)
{
    *l = g_list_append (*l, skey);
}

/* Callback for copying secret keys to a list */
static void
secret_keys_to_list (const gchar *id, SeahorseKey *skey, GList **l)
{
    if (SEAHORSE_IS_KEY_PAIR (skey))
        *l = g_list_append (*l, skey);
}

/* Callback for counting secret keys */
static void
count_secret_keys (const gchar *id, SeahorseKey *skey, guint *n)
{
    if (SEAHORSE_IS_KEY_PAIR (skey))
        (*n)++;
}

/* Called when a key load operation is done */
static void 
keyload_done (SeahorseOperation *operation, SeahorsePGPSource *psrc)
{
    g_return_if_fail (SEAHORSE_IS_PGP_SOURCE (psrc));
    
    /* Marks us as done as far as the key source is concerned */
    psrc->priv->operations = 
        seahorse_operation_list_remove (psrc->priv->operations, operation);
}

static void
add_load_operation (SeahorsePGPSource *psrc, SeahorseLoadOperation *lop)
{
    if (seahorse_operation_is_done (SEAHORSE_OPERATION (lop))) {
        g_object_unref (lop);
    } else {
        psrc->priv->operations = seahorse_operation_list_add (psrc->priv->operations, SEAHORSE_OPERATION (lop));
        g_signal_connect (lop, "done", G_CALLBACK (keyload_done), psrc);         
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
    seahorse_operation_mark_done (operation);
}

/* Completes one batch of key loading */
static gboolean
keyload_handler (SeahorseLoadOperation *lop)
{
    gpgme_key_t key;
    guint batch;
    const gchar *id;
    
    g_return_val_if_fail (SEAHORSE_IS_LOAD_OPERATION (lop), FALSE);
    
    /* We load until done if batch is zero */
    batch = lop->batch == 0 ? ~0 : lop->batch;

    while (batch-- > 0) {
    
        if (!GPG_IS_OK (gpgme_op_keylist_next (lop->ctx, &key))) {
            seahorse_key_source_show_progress (SEAHORSE_KEY_SOURCE (lop->psrc), 
                g_strdup_printf (ngettext("Loaded %d key", "Loaded %d keys", lop->loaded), lop->loaded), -1);
        
            gpgme_op_keylist_end (lop->ctx);
        
            /* If we were a refresh loader, then we remove the keys we didn't find */
            if (lop->checks) 
                g_hash_table_foreach (lop->checks, (GHFunc)remove_key_from_source, lop->psrc);
            
            seahorse_operation_mark_done (SEAHORSE_OPERATION (lop));         
            return FALSE; /* Remove event handler */
        }
        
        id = seahorse_key_get_id (key);
        
        /* During a refresh if only new or removed keys */
        if (lop->checks) {

            /* Make note that this key exists in keyring */
            g_hash_table_remove (lop->checks, id);

            /* When not doing all keys and already have ... */
            if (!lop->all && have_key_in_source (lop->psrc, id)) {

                /* ... then just ignore */
                gpgmex_key_unref (key);
                continue;
            }
        }
        
        add_key_to_source (lop->psrc, key);
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
    
    return TRUE; 
}


/* Callback which copies keys ids into new hashtable */
static void
key_ids_to_hash (const gchar *id, SeahorseKey *skey, GHashTable *ht)
{
    g_hash_table_insert (ht, g_strdup (id), NULL);
}

/* Callback which copies secret key ids into new hashtable */
static void
secret_key_ids_to_hash (const gchar *id, SeahorseKey *skey, GHashTable *ht)
{
    if (SEAHORSE_IS_KEY_PAIR (skey))
        g_hash_table_insert (ht, g_strdup(id), NULL);
}

static SeahorseLoadOperation*
seahorse_load_operation_start (SeahorsePGPSource *psrc, const gchar *pattern,
                               gboolean secret, gboolean refresh, gboolean all)
{
    SeahorsePGPSourcePrivate *priv;
    SeahorseLoadOperation *lop;
    gpgme_error_t err;
    
    g_return_val_if_fail (SEAHORSE_IS_PGP_SOURCE (psrc), NULL);
    priv = psrc->priv;

    lop = g_object_new (SEAHORSE_TYPE_LOAD_OPERATION, NULL);    
    lop->psrc = psrc;
    g_object_ref (psrc);

    /* Start the key listing */
    err = gpgme_op_keylist_start (lop->ctx, pattern, secret);
    g_return_val_if_fail (GPG_IS_OK (err), lop);
    
    if (refresh) {
     
        lop->all = all;
        
        /* This hashtable contains only allocated strings with no 
         * values. We allocate the strings in case a key goes away
         * while we're holding the ids in this table */
        lop->checks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
        
        g_hash_table_foreach (priv->keys, secret ? 
                                (GHFunc)secret_key_ids_to_hash : 
                                (GHFunc)key_ids_to_hash, 
                              lop->checks);
    }
    
    seahorse_operation_mark_start (SEAHORSE_OPERATION (lop));

    /* Run one iteration of the handler */
    keyload_handler (lop);

    return lop;
}    

/* --------------------------------------------------------------------------
 * METHODS
 */

static void         
seahorse_pgp_source_refresh (SeahorseKeySource *src, gboolean all)
{
    SeahorsePGPSource *psrc;
    SeahorseLoadOperation *lop;
    
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (src));
    psrc = SEAHORSE_PGP_SOURCE (src);

    /* Secret keys */
    lop = seahorse_load_operation_start (psrc, NULL, FALSE, TRUE, all);
    add_load_operation (psrc, lop);
    
    /* Public keys */
    lop = seahorse_load_operation_start (psrc, NULL, TRUE, TRUE, all);
    add_load_operation (psrc, lop);
}

static void
seahorse_pgp_source_stop (SeahorseKeySource *src)
{
    SeahorsePGPSource *psrc;
    gboolean found;
    
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (src));
    psrc = SEAHORSE_PGP_SOURCE (src);
    
    found = (psrc->priv->operations != NULL);
    seahorse_operation_list_cancel (psrc->priv->operations);
    psrc->priv->operations = seahorse_operation_list_purge (psrc->priv->operations);
    
    if (found)
        seahorse_key_source_show_progress (src, _("Load Cancelled"), -1);
}

static guint
seahorse_pgp_source_get_count (SeahorseKeySource *src,
                               gboolean secret_only)
{
    SeahorsePGPSource *psrc;
    guint n = 0;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (src), 0);
    psrc = SEAHORSE_PGP_SOURCE (src);
    
    if (!secret_only)
        return g_hash_table_size (psrc->priv->keys);
    
    g_hash_table_foreach (psrc->priv->keys, (GHFunc)count_secret_keys, &n);
    return n;
}

SeahorseKey*        
seahorse_pgp_source_get_key (SeahorseKeySource *src, const gchar *fpr, 
                             SeahorseKeyInfo info)
{
    SeahorseLoadOperation *lop;
    SeahorsePGPSource *psrc;
    SeahorsePGPSourcePrivate *priv;
    SeahorseKey *skey;
    gpgme_error_t err;
    gchar *fingerprint;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (src), NULL);
    psrc = SEAHORSE_PGP_SOURCE (src);
    priv = psrc->priv;
    
    skey = g_hash_table_lookup (priv->keys, fpr);
    
    /* Make sure we have enough info on this key */
    if (skey && info <= seahorse_key_get_loaded_info(skey))
        return skey;
        
    /* If info is NONE then we also return (no loading) */
    if (info <= SKEY_INFO_NONE)
        return skey;

    lop = g_object_new (SEAHORSE_TYPE_LOAD_OPERATION, NULL);    
    lop->psrc = psrc;
    g_object_ref (psrc);

    /* Load sigs if asking for a complete load */
    if (info >= SKEY_INFO_COMPLETE) {
        gpgme_set_keylist_mode (lop->ctx, GPGME_KEYLIST_MODE_SIGS | 
                gpgme_get_keylist_mode (lop->ctx));
    }    
    
    /* Start the key listing */
    err = gpgme_op_keylist_start (lop->ctx, fpr, FALSE);
    g_return_val_if_fail (GPG_IS_OK (err), skey);

    /* Set the batch size to zero which forces it to be synchronous */
    lop->batch = 0;

    /* We need to backup the fingerprint, as the old key can be freed */
    fingerprint = g_strdup (fpr);
    
    seahorse_operation_mark_start (SEAHORSE_OPERATION (lop));
    keyload_handler (lop);
    
    /* Because we set batch to 0, should be done at this point */
    g_assert (seahorse_operation_is_done (SEAHORSE_OPERATION (lop)));
    g_object_unref (lop);
            
    /* Try again with our lookup */
    skey = g_hash_table_lookup (priv->keys, fingerprint);
    g_free (fingerprint);
    
    return skey;
}

static GList*
seahorse_pgp_source_get_keys (SeahorseKeySource *src,
                              gboolean secret_only)  
{
    SeahorsePGPSource *psrc;
    GList *l = NULL;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (src), NULL);
    psrc = SEAHORSE_PGP_SOURCE (src);
    
    g_hash_table_foreach (psrc->priv->keys, 
            (GHFunc)(secret_only ? secret_keys_to_list : keys_to_list), &l);
    return l;
}

static guint
seahorse_pgp_source_get_state (SeahorseKeySource *src)
{
    SeahorsePGPSource *psrc;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (src), 0);
    psrc = SEAHORSE_PGP_SOURCE (src);
    
    return psrc->priv->operations ? SEAHORSE_KEY_SOURCE_LOADING : 0;
}

static gpgme_ctx_t  
seahorse_pgp_source_new_context (SeahorseKeySource *src)
{
    gpgme_ctx_t ctx = NULL;
    g_return_val_if_fail (GPG_IS_OK (init_gpgme (&ctx)), NULL);
    return ctx;
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

/**
 * seahorse_pgp_source_load
 * @psrc: The PGP key source
 * @secret_only: Only load secret keys.
 * 
 * Starts a load operation on the key source.
 **/
void        
seahorse_pgp_source_load (SeahorsePGPSource *psrc, gboolean secret_only)
{
    SeahorseLoadOperation *lop;
    g_return_if_fail (SEAHORSE_IS_PGP_SOURCE (psrc));
    
    /* Secret keys */
    lop = seahorse_load_operation_start (psrc, NULL, TRUE, FALSE, FALSE);
    add_load_operation (psrc, lop);
    
    if (!secret_only) {
        /* Public keys */
        lop = seahorse_load_operation_start (psrc, NULL, FALSE, FALSE, FALSE);
        add_load_operation (psrc, lop);
    }
}

/**
 * seahorse_pgp_source_load
 * @psrc: The PGP key source
 * @pattern: Pattern to match keys with
 * 
 * Starts a load operation on the key source for keys matching the pattern.
 **/
 void
seahorse_pgp_source_load_matching (SeahorsePGPSource *psrc, const gchar *pattern)
{
    SeahorseLoadOperation *lop;
    g_return_if_fail (SEAHORSE_IS_PGP_SOURCE (psrc));
        
    lop = seahorse_load_operation_start (psrc, pattern, TRUE, FALSE, FALSE);
    add_load_operation (psrc, lop);
    
    /* Public keys */
    lop = seahorse_load_operation_start (psrc, pattern, FALSE, FALSE, FALSE);
    add_load_operation (psrc, lop);
}
