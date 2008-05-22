/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2005, 2006 Stefan Walter
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

#include <stdlib.h>
#include <string.h>
#include <libintl.h>

#include "seahorse-context.h"
#include "seahorse-marshal.h"
#include "seahorse-libdialogs.h"
#include "seahorse-gconf.h"
#include "seahorse-util.h"
#include "seahorse-dns-sd.h"
#include "seahorse-transfer-operation.h"
#include "seahorse-unknown-source.h"
#include "seahorse-unknown-key.h"

#include "common/seahorse-registry.h"

#include "pgp/seahorse-server-source.h"

/* The application main context */
SeahorseContext* app_context = NULL;

enum {
    ADDED,
    REMOVED,
    CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (SeahorseContext, seahorse_context, GTK_TYPE_OBJECT);

/* 
 * Two hashtables are used to keep track of the keys:
 *
 *  keys_by_source: This contains a reference to the key and allows us to 
 *    lookup keys by their source. Each key/source combination should be 
 *    unique. Hashkeys are made with hashkey_by_source()
 *  keys_by_type: Each value contains a GList of keys with the same keyid
 *    (ie: same key from different key sources). The keys are 
 *    orderred in by preferred usage. 
 */

struct _SeahorseContextPrivate {
    GSList *sources;                        /* Key sources which add keys to this context */
    GHashTable *auto_sources;               /* Automatically added key sources (keyservers) */
    GHashTable *keys_by_source;             /* See explanation above */
    GHashTable *keys_by_type;               /* See explanation above */
    guint notify_id;                        /* Notify for GConf watch */
    SeahorseServiceDiscovery *discovery;    /* Adds key sources from DNS-SD */
};

static void seahorse_context_dispose    (GObject *gobject);
static void seahorse_context_finalize   (GObject *gobject);

/* Forward declarations */
static void refresh_keyservers          (GConfClient *client, guint id, 
                                         GConfEntry *entry, SeahorseContext *sctx);

static void
seahorse_context_class_init (SeahorseContextClass *klass)
{
    GObjectClass *gobject_class;
    
    seahorse_context_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->dispose = seahorse_context_dispose;
    gobject_class->finalize = seahorse_context_finalize;	
    
    signals[ADDED] = g_signal_new ("added", SEAHORSE_TYPE_CONTEXT, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseContextClass, added),
                NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, SEAHORSE_TYPE_KEY);
    signals[REMOVED] = g_signal_new ("removed", SEAHORSE_TYPE_CONTEXT, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseContextClass, removed),
                NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, SEAHORSE_TYPE_KEY);    
    signals[CHANGED] = g_signal_new ("changed", SEAHORSE_TYPE_CONTEXT, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseContextClass, changed),
                NULL, NULL, seahorse_marshal_VOID__OBJECT_UINT, G_TYPE_NONE, 2, SEAHORSE_TYPE_KEY, G_TYPE_UINT);    
}

/* init context, private vars, set prefs, connect signals */
static void
seahorse_context_init (SeahorseContext *sctx)
{
    /* init private vars */
    sctx->pv = g_new0 (SeahorseContextPrivate, 1);

    /* A list of sources */
    sctx->pv->sources = NULL;
    
    /* A table of keys */
    sctx->pv->keys_by_source = g_hash_table_new_full (g_direct_hash, g_direct_equal, 
                                                      NULL, g_object_unref);
    
    sctx->pv->keys_by_type = g_hash_table_new_full (g_direct_hash, g_direct_equal, 
                                                    NULL, NULL);
    
    /* The context is explicitly destroyed */
    g_object_ref (sctx);
    
}

static gboolean
remove_each (gpointer key, gpointer value, gpointer user_data)
{
    return TRUE;
}

/* release all references */
static void
seahorse_context_dispose (GObject *gobject)
{
    SeahorseContext *sctx;
    GSList *l;
    
    sctx = SEAHORSE_CONTEXT (gobject);
    
    /* All the keys */
    g_hash_table_foreach_remove (sctx->pv->keys_by_source, remove_each, NULL);
    g_hash_table_foreach_remove (sctx->pv->keys_by_type, remove_each, NULL);

    /* Gconf notification */
    if (sctx->pv->notify_id)
        seahorse_gconf_unnotify (sctx->pv->notify_id);
    sctx->pv->notify_id = 0;
    
    /* Auto sources */
    if (sctx->pv->auto_sources) 
        g_hash_table_destroy (sctx->pv->auto_sources);
    sctx->pv->auto_sources = NULL;
        
    /* DNS-SD */    
    if (sctx->pv->discovery) 
        g_object_unref (sctx->pv->discovery);
    sctx->pv->discovery = NULL;
        
    /* Release all the sources */
    for (l = sctx->pv->sources; l; l = g_slist_next (l)) {
        seahorse_key_source_stop (SEAHORSE_KEY_SOURCE (l->data));
        g_object_unref (SEAHORSE_KEY_SOURCE (l->data));
    }
    g_slist_free (sctx->pv->sources);
    sctx->pv->sources = NULL;
    
    G_OBJECT_CLASS (seahorse_context_parent_class)->dispose (gobject);
}

/* destroy all keys, free private vars */
static void
seahorse_context_finalize (GObject *gobject)
{
    SeahorseContext *sctx = SEAHORSE_CONTEXT (gobject);
    
    /* Destroy the hash table */        
    if (sctx->pv->keys_by_source)
        g_hash_table_destroy (sctx->pv->keys_by_source);
    if (sctx->pv->keys_by_type)
        g_hash_table_destroy (sctx->pv->keys_by_type);
    
    /* Other stuff already done in dispose */
    g_assert (sctx->pv->sources == NULL);
    g_assert (sctx->pv->auto_sources == NULL);
    g_assert (sctx->pv->discovery == NULL);
    g_free (sctx->pv);
    
    G_OBJECT_CLASS (seahorse_context_parent_class)->finalize (gobject);
}

SeahorseContext*
seahorse_context_app (void)
{
    g_return_val_if_fail (app_context != NULL, NULL);
    return app_context;
}
   
SeahorseContext*
seahorse_context_new (guint flags, guint ktype)
{
	SeahorseContext *sctx = g_object_new (SEAHORSE_TYPE_CONTEXT, NULL);
    
    	if (flags & SEAHORSE_CONTEXT_DAEMON)
	    sctx->is_daemon = TRUE;
    
	if (flags & SEAHORSE_CONTEXT_APP) {
		
		GList *l, *types;
		
		types = seahorse_registry_find_types (NULL, "key-source", "local", NULL);
		for (l = types; l; l = g_list_next (l)) {
			SeahorseKeySource *src = g_object_new (GPOINTER_TO_UINT (l->data), NULL);
			seahorse_context_take_key_source (sctx, src);
		}
		g_list_free (types);

		/* DNS-SD discovery */    
		sctx->pv->discovery = seahorse_service_discovery_new ();
        
		/* Automatically added remote key sources */
		sctx->pv->auto_sources = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                        g_free, NULL);

		/* Listen for new gconf remote key sources automatically */
		sctx->pv->notify_id = seahorse_gconf_notify (KEYSERVER_KEY, 
                                    (GConfClientNotifyFunc)refresh_keyservers, sctx);
        
		if (app_context)
			g_object_unref (app_context);
        
		g_object_ref (sctx);
		gtk_object_sink (GTK_OBJECT (sctx));
		app_context = sctx;
        
		refresh_keyservers (NULL, 0, NULL, sctx);
	}
    
	return sctx;
}

/**
 * seahorse_context_destroy:
 * @sctx: #SeahorseContext to destroy
 *
 * Emits the destroy signal for @sctx.
 **/
void
seahorse_context_destroy (SeahorseContext *sctx)
{
	g_return_if_fail (GTK_IS_OBJECT (sctx));
	
	gtk_object_destroy (GTK_OBJECT (sctx));
    
    if (sctx == app_context)
        app_context = NULL;
}

void                
seahorse_context_take_key_source (SeahorseContext *sctx, SeahorseKeySource *sksrc)
{
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc));

    if (!g_slist_find (sctx->pv->sources, sksrc))
        sctx->pv->sources = g_slist_append (sctx->pv->sources, sksrc);
}

void
seahorse_context_add_key_source (SeahorseContext *sctx, SeahorseKeySource *sksrc)
{
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc));

    if (g_slist_find (sctx->pv->sources, sksrc))
        return;
    
    sctx->pv->sources = g_slist_append (sctx->pv->sources, sksrc);
    g_object_ref (sksrc);
}
    
void
seahorse_context_remove_key_source (SeahorseContext *sctx, SeahorseKeySource *sksrc)
{
    GList *l, *keys;
    
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc));

    if (!g_slist_find (sctx->pv->sources, sksrc)) 
        return;

    /* Remove all keys from this source */    
    keys = seahorse_context_get_keys (sctx, sksrc);
    for (l = keys; l; l = g_list_next (l)) 
        seahorse_context_remove_key (sctx, SEAHORSE_KEY (l->data));
    
    /* Remove the source itself */
    sctx->pv->sources = g_slist_remove (sctx->pv->sources, sksrc);
    g_object_unref (sksrc);
}

SeahorseKeySource*  
seahorse_context_find_key_source (SeahorseContext *sctx, GQuark ktype,
                                  SeahorseKeyLoc location)
{
    SeahorseKeySource *ks;
    GSList *l;
    
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    for (l = sctx->pv->sources; l; l = g_slist_next (l)) {
        ks = SEAHORSE_KEY_SOURCE (l->data);
        
        if (ktype != SKEY_UNKNOWN && 
            seahorse_key_source_get_ktype (ks) != ktype)
            continue;
        
        if (location != SKEY_LOC_INVALID && 
            seahorse_key_source_get_location (ks) != location)
            continue;
        
        return ks;
    }
    
    /* If we don't have an unknown source for this type, create it */
    if (location == SKEY_LOC_UNKNOWN && location != SKEY_UNKNOWN) {
        ks = SEAHORSE_KEY_SOURCE (seahorse_unknown_source_new (ktype));
        seahorse_context_add_key_source (sctx, ks);
        return ks;
    }
    
    return NULL;
}

GSList*
seahorse_context_find_key_sources (SeahorseContext *sctx, GQuark ktype,
                                   SeahorseKeyLoc location)
{
    SeahorseKeySource *ks;
    GSList *sources = NULL;
    GSList *l;
    
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    for (l = sctx->pv->sources; l; l = g_slist_next (l)) {
        ks = SEAHORSE_KEY_SOURCE (l->data);
        
        if (ktype != SKEY_UNKNOWN && 
            seahorse_key_source_get_ktype (ks) != ktype)
            continue;
        
        if (location != SKEY_LOC_INVALID && 
            seahorse_key_source_get_location (ks) != location)
            continue;
        
        sources = g_slist_append (sources, ks);
    }
    
    return sources;
}

SeahorseKeySource*  
seahorse_context_remote_key_source (SeahorseContext *sctx, const gchar *uri)
{
    SeahorseKeySource *ks = NULL;
    gboolean found = FALSE;
    gchar *ks_uri;
    GSList *l;
    
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);
    g_return_val_if_fail (uri && *uri, NULL);

    for (l = sctx->pv->sources; l; l = g_slist_next (l)) {
        ks = SEAHORSE_KEY_SOURCE (l->data);
        
        if (seahorse_key_source_get_location (ks) != SKEY_LOC_REMOTE)
            continue;
        
        g_object_get (ks, "uri", &ks_uri, NULL);
        if (ks_uri && g_str_equal (ks_uri, uri)) 
            found = TRUE;
        g_free (ks_uri);
        
        if (found)
            return ks;
    }
    
#ifdef WITH_KEYSERVER
    /* Auto generate one if possible */
    if (sctx->pv->auto_sources) {
        ks = SEAHORSE_KEY_SOURCE (seahorse_server_source_new (uri));
        if (ks != NULL) {
            seahorse_context_take_key_source (sctx, ks);
            g_hash_table_replace (sctx->pv->auto_sources, g_strdup (uri), ks);
        }
    }
#endif /* WITH_KEYSERVER */	
    
    return ks;
}


static void
key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorseContext *sctx)
{
    g_signal_emit (sctx, signals[CHANGED], 0, skey, change);
}

static void
key_destroyed (SeahorseKey *skey, SeahorseContext *sctx)
{
    /* When keys are destroyed elsewhere */
    seahorse_context_remove_key (sctx, skey);
}

static gpointer                 
hashkey_by_source (SeahorseKeySource *sksrc, GQuark keyid)
{
    return GINT_TO_POINTER (g_direct_hash (sksrc) ^ 
                            g_str_hash (g_quark_to_string (keyid)));
}

static gint
sort_by_location (gconstpointer a, gconstpointer b)
{
    guint aloc, bloc;
    
    g_assert (SEAHORSE_IS_KEY (a));
    g_assert (SEAHORSE_IS_KEY (b));
    
    aloc = seahorse_key_get_location (SEAHORSE_KEY (a));
    bloc = seahorse_key_get_location (SEAHORSE_KEY (b));
    
    if (aloc == bloc)
        return 0;
    return aloc > bloc ? -1 : 1;
}

static void
setup_keys_by_type (SeahorseContext *sctx, SeahorseKey *skey, gboolean add)
{
    GList *l, *keys = NULL;
    SeahorseKey *akey, *next;
    gpointer kt = GUINT_TO_POINTER (seahorse_key_get_keyid (skey));
    gboolean first;
    
    /* Get current set of keys in this ktype/keyid */
    if (add)
        keys = g_list_prepend (keys, skey);
    
    for (akey = g_hash_table_lookup (sctx->pv->keys_by_type, kt); 
         akey; akey = akey->preferred)
    {
        if (akey != skey)
            keys = g_list_prepend (keys, akey);
    }
    
    /* No keys just remove */
    if (!keys) {
        g_hash_table_remove (sctx->pv->keys_by_type, kt);
        return;
    }

    /* Sort and add back */
    keys = g_list_sort (keys, sort_by_location);
    for (l = keys, first = TRUE; l; l = g_list_next (l), first = FALSE) {
        
        akey = SEAHORSE_KEY (l->data);
        
        /* Set first as start of list */
        if (first)
            g_hash_table_replace (sctx->pv->keys_by_type, kt, akey);
            
        /* Set next one as preferred */
        else {
            next = g_list_next (l) ? SEAHORSE_KEY (g_list_next (l)->data) : NULL;
            seahorse_key_set_preferred (akey, next);
        }
    }
    
    g_list_free (keys);
}

void
seahorse_context_add_key (SeahorseContext *sctx, SeahorseKey *skey)
{
    g_object_ref (skey);
    seahorse_context_take_key (sctx, skey);
}

void
seahorse_context_take_key (SeahorseContext *sctx, SeahorseKey *skey)
{
    gpointer ks;
    
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
    g_return_if_fail (SEAHORSE_IS_KEY (skey));
    g_return_if_fail (skey->keyid != 0);
    g_return_if_fail (!skey->attached_to);
    
    ks = hashkey_by_source (seahorse_key_get_source (skey), 
                            seahorse_key_get_keyid (skey));
    
    g_return_if_fail (!g_hash_table_lookup (sctx->pv->keys_by_source, ks));

    g_object_ref (skey);

    skey->attached_to = sctx;
    g_hash_table_replace (sctx->pv->keys_by_source, ks, skey);
    setup_keys_by_type (sctx, skey, TRUE);
    g_signal_emit (sctx, signals[ADDED], 0, skey);
    g_object_unref (skey);
    
    g_signal_connect (skey, "changed", G_CALLBACK (key_changed), sctx);
    g_signal_connect (skey, "destroy", G_CALLBACK (key_destroyed), sctx);
}

guint
seahorse_context_get_count (SeahorseContext *sctx)
{
    return g_hash_table_size (sctx->pv->keys_by_source);
}

SeahorseKey*        
seahorse_context_get_key (SeahorseContext *sctx, SeahorseKeySource *sksrc,
                          GQuark keyid)
{
    gconstpointer k;
    
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc), NULL);
    
    k = hashkey_by_source (sksrc, keyid);
    return SEAHORSE_KEY (g_hash_table_lookup (sctx->pv->keys_by_source, k));
}

typedef struct _KeyMatcher {
    
    SeahorseKeyPredicate *kp;
    gboolean many;
    GList *keys;
    
} KeyMatcher;

gboolean
find_matching_keys (gpointer key, SeahorseKey *skey, KeyMatcher *km)
{
    if (km->kp && seahorse_key_predicate_match (km->kp, skey))
        km->keys = g_list_prepend (km->keys, skey);

    /* Terminate search */
    if (!km->many && km->keys)
        return TRUE;

    /* Keep going */
    return FALSE;
}

GList*             
seahorse_context_get_keys (SeahorseContext *sctx, SeahorseKeySource *sksrc)
{
    SeahorseKeyPredicate kp;
    KeyMatcher km;

    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);
    g_return_val_if_fail (sksrc == NULL || SEAHORSE_IS_KEY_SOURCE (sksrc), NULL);

    memset (&kp, 0, sizeof (kp));
    memset (&km, 0, sizeof (km));
    
    km.kp = &kp;
    km.many = TRUE;
    kp.sksrc = sksrc;
    
    g_hash_table_find (sctx->pv->keys_by_source, (GHRFunc)find_matching_keys, &km);
    return km.keys;
}

SeahorseKey*        
seahorse_context_find_key (SeahorseContext *sctx, GQuark keyid, SeahorseKeyLoc location)
{
    SeahorseKey *skey; 
    
    skey = (SeahorseKey*)g_hash_table_lookup (sctx->pv->keys_by_type, GUINT_TO_POINTER (keyid));
    while (skey) {
        
        /* If at the end and no more keys in list, return */
        if (location == SKEY_LOC_INVALID && !skey->preferred)
            return skey;
        
        if (location >= seahorse_key_get_location (skey))
            return skey;
        
        /* Look down the list for this location */
        skey = skey->preferred;
    }
    
    return NULL;
}

GList*
seahorse_context_find_keys (SeahorseContext *sctx, GQuark ktype, 
                            SeahorseKeyEType etype, SeahorseKeyLoc location)
{
    SeahorseKeyPredicate pred;
    memset (&pred, 0, sizeof (pred));
    
    pred.ktype = ktype;
    pred.etype = etype;
    pred.location = location;
    
    return seahorse_context_find_keys_full (sctx, &pred);
}

GList*
seahorse_context_find_keys_full (SeahorseContext *sctx, SeahorseKeyPredicate *skpred)
{
    KeyMatcher km;

    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    memset (&km, 0, sizeof (km));
    
    km.kp = skpred;
    km.many = TRUE;
    
    g_hash_table_find (sctx->pv->keys_by_source, (GHRFunc)find_matching_keys, &km);
    return km.keys; 
}

gboolean
seahorse_context_owns_key (SeahorseContext *sctx, SeahorseKey *skey)
{
    return skey->attached_to == sctx;
}

void 
seahorse_context_remove_key (SeahorseContext *sctx, SeahorseKey *skey)
{
    gconstpointer k;
    
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
    g_return_if_fail (SEAHORSE_IS_KEY (skey));
    
    k = hashkey_by_source (seahorse_key_get_source (skey), 
                           seahorse_key_get_keyid (skey));
    
    if (g_hash_table_lookup (sctx->pv->keys_by_source, k)) {
        g_return_if_fail (skey->attached_to == sctx);

        g_object_ref (skey);
        g_signal_handlers_disconnect_by_func (skey, key_changed, sctx);
        g_signal_handlers_disconnect_by_func (skey, key_destroyed, sctx);
        skey->attached_to = NULL;
        g_hash_table_remove (sctx->pv->keys_by_source, k);
        setup_keys_by_type (sctx, skey, FALSE);
        g_signal_emit (sctx, signals[REMOVED], 0, skey);    
        g_object_unref (skey);
    }
}


/* -----------------------------------------------------------------------------
 * DEPRECATED 
 */

/**
 * seahorse_context_get_default_key
 * @sctx: Current #SeahorseContext
 * 
 * Returns: the secret key that's the default key 
 */
SeahorseKey*
seahorse_context_get_default_key (SeahorseContext *sctx)
{
    SeahorseKey *skey = NULL;
    gchar *id;
    
    /* TODO: All of this needs to take multiple key types into account */
    
    id = seahorse_gconf_get_string (SEAHORSE_DEFAULT_KEY);
    if (id != NULL && id[0]) {
        GQuark keyid = g_quark_from_string (id);
        skey = seahorse_context_find_key (sctx, keyid, SKEY_LOC_LOCAL);
    }
    
    g_free (id);
    return skey;
}

/**
 * seahorse_context_get_discovery
 * @sctx: #SeahorseContext object
 * 
 * Gets the Service Discovery object for this context.
 *
 * Return: The Service Discovery object. 
 */
SeahorseServiceDiscovery*
seahorse_context_get_discovery (SeahorseContext *sctx)
{
    g_return_val_if_fail (sctx->pv->discovery != NULL, NULL);
    return sctx->pv->discovery;
}

SeahorseOperation*  
seahorse_context_load_local_keys (SeahorseContext *sctx)
{
    SeahorseKeySource *ks;
    SeahorseMultiOperation *mop = NULL;
    SeahorseOperation *op = NULL;
    GSList *l;
    
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    for (l = sctx->pv->sources; l; l = g_slist_next (l)) {
        ks = SEAHORSE_KEY_SOURCE (l->data);
        
        if (seahorse_key_source_get_location (ks) == SKEY_LOC_LOCAL) {
            if (mop == NULL && op != NULL) {
                mop = seahorse_multi_operation_new ();
                seahorse_multi_operation_take (mop, op);
            }
            
            op = seahorse_key_source_load (ks, 0);
            
            if (mop != NULL)
                seahorse_multi_operation_take (mop, op);
        }
    }   

    return mop ? SEAHORSE_OPERATION (mop) : op;  
}

static gboolean 
load_local_keys (SeahorseContext *sctx)
{
    SeahorseOperation *op = seahorse_context_load_local_keys (sctx);
    g_return_val_if_fail (op != NULL, FALSE);
    g_object_unref (op);
    return FALSE;
}

void
seahorse_context_load_local_keys_async (SeahorseContext *sctx)
{
    g_idle_add_full (G_PRIORITY_LOW, (GSourceFunc)load_local_keys, sctx, NULL);
}


SeahorseOperation*  
seahorse_context_load_remote_keys (SeahorseContext *sctx, const gchar *search)
{
    SeahorseKeySource *ks;
    SeahorseMultiOperation *mop = NULL;
    SeahorseOperation *op = NULL;
    GSList *l, *names;
    GHashTable *servers = NULL;
    gchar *uri;
    
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);
    
    /* Get a list of all selected key servers */
    names = seahorse_gconf_get_string_list (LASTSERVERS_KEY);
    if (names) {
        servers = g_hash_table_new (g_str_hash, g_str_equal);
        for (l = names; l; l = g_slist_next (l)) 
            g_hash_table_insert (servers, l->data, GINT_TO_POINTER (TRUE));
    }
        
    for (l = sctx->pv->sources; l; l = g_slist_next (l)) {
        ks = SEAHORSE_KEY_SOURCE (l->data);
        
        if (seahorse_key_source_get_location (ks) != SKEY_LOC_REMOTE)
            continue;

        if (servers) {
            g_object_get (ks, "uri", &uri, NULL);
            if (!g_hash_table_lookup (servers, uri)) {
                g_free (uri);
                continue;
            }
            
            g_free (uri);
        }

        if (op != NULL) {
            mop = seahorse_multi_operation_new ();
            seahorse_multi_operation_take (mop, op);
        }
            
        op = seahorse_key_source_search (ks, search);
            
        if (mop != NULL)
            seahorse_multi_operation_take (mop, op);
    }   

    seahorse_util_string_slist_free (names);
    return mop ? SEAHORSE_OPERATION (mop) : op;  
}

/* For copying the keys */
static void 
auto_source_to_hash (const gchar *uri, SeahorseKeySource *sksrc, GHashTable *ht)
{
    g_hash_table_replace (ht, (gpointer)uri, sksrc);
}

static void
auto_source_remove (const gchar* uri, SeahorseKeySource *sksrc, SeahorseContext *sctx)
{
    seahorse_context_remove_key_source (sctx, sksrc);
    g_hash_table_remove (sctx->pv->auto_sources, uri);
}

static void
refresh_keyservers (GConfClient *client, guint id, GConfEntry *entry, 
                    SeahorseContext *sctx)
{
#ifdef WITH_KEYSERVER
    SeahorseServerSource *ssrc;
    GSList *keyservers, *l;
    GHashTable *check;
    const gchar *uri;
    
    if (!sctx->pv->auto_sources)
        return;
    
    if (entry && !g_str_equal (KEYSERVER_KEY, gconf_entry_get_key (entry)))
        return;

    /* Make a light copy of the auto_source table */    
    check = g_hash_table_new (g_str_hash, g_str_equal);
    g_hash_table_foreach (sctx->pv->auto_sources, (GHFunc)auto_source_to_hash, check);

    
    /* Load and strip names from keyserver list */
    keyservers = seahorse_gconf_get_string_list (KEYSERVER_KEY);
    seahorse_server_source_purge_keyservers (keyservers);
    
    for (l = keyservers; l; l = g_slist_next (l)) {
        uri = (const gchar*)(l->data);
        
        /* If we don't have a keysource then add it */
        if (!g_hash_table_lookup (sctx->pv->auto_sources, uri)) {
            ssrc = seahorse_server_source_new (uri);
            if (ssrc != NULL) {
                seahorse_context_take_key_source (sctx, SEAHORSE_KEY_SOURCE (ssrc));
                g_hash_table_replace (sctx->pv->auto_sources, g_strdup (uri), ssrc);
            }
        }
        
        /* Mark this one as present */
        g_hash_table_remove (check, uri);
    }
    
    /* Now remove any extras */
    g_hash_table_foreach (check, (GHFunc)auto_source_remove, sctx);
    
    g_hash_table_destroy (check);
    seahorse_util_string_slist_free (keyservers);    
#endif /* WITH_KEYSERVER */
}

SeahorseOperation*
seahorse_context_transfer_keys (SeahorseContext *sctx, GList *keys, 
                                SeahorseKeySource *to)
{
    SeahorseKeySource *from;
    SeahorseOperation *op = NULL;
    SeahorseMultiOperation *mop = NULL;
    SeahorseKey *skey;
    GSList *keyids = NULL;
    GList *next, *l;
    GQuark ktype;

    keys = g_list_copy (keys);
    
    /* Sort by key source */
    keys = seahorse_util_keylist_sort (keys);
    
    while (keys) {
        
        /* break off one set (same keysource) */
        next = seahorse_util_keylist_splice (keys);

        g_assert (SEAHORSE_IS_KEY (keys->data));
        skey = SEAHORSE_KEY (keys->data);

        /* Export from this key source */
        from = seahorse_key_get_source (skey);
        g_return_val_if_fail (from != NULL, FALSE);
        ktype = seahorse_key_source_get_ktype (from);
        
        /* Find a local keysource to import to */
        if (!to) {
            to = seahorse_context_find_key_source (sctx, ktype, SKEY_LOC_LOCAL);
            if (!to) {
                /* TODO: How can we warn caller about this. Do we need to? */
                g_warning ("couldn't find a local key source for: %s", 
                           g_quark_to_string (ktype));
            }
        }
        
        /* Make sure it's the same type */
        if (ktype != seahorse_key_source_get_ktype (to)) {
            /* TODO: How can we warn caller about this. Do we need to? */
            g_warning ("destination key source is not of type: %s", 
                       g_quark_to_string (ktype));
        }
        
        if (to != NULL && from != to) {
            
            if (op != NULL) {
                if (mop == NULL)
                    mop = seahorse_multi_operation_new ();
                seahorse_multi_operation_take (mop, op);
            }
            
            /* Build keyid list */
            for (l = keys; l; l = g_list_next (l)) 
                keyids = g_slist_prepend (keyids, GUINT_TO_POINTER (seahorse_key_get_keyid (l->data)));
            keyids = g_slist_reverse (keyids);
        
            /* Start a new transfer operation between the two key sources */
            op = seahorse_transfer_operation_new (NULL, from, to, keyids);
            g_return_val_if_fail (op != NULL, FALSE);
            
            g_slist_free (keyids);
            keyids = NULL;
        }

        g_list_free (keys);
        keys = next;
    } 
    
    /* No keys done, just return success */
    if (!mop && !op) {
        g_warning ("no valid keys to transfer found");
        return seahorse_operation_new_complete (NULL);
    }
    
    return mop ? SEAHORSE_OPERATION (mop) : op;
}

SeahorseOperation*
seahorse_context_retrieve_keys (SeahorseContext *sctx, GQuark ktype, 
                                GSList *keyids, SeahorseKeySource *to)
{
    SeahorseMultiOperation *mop = NULL;
    SeahorseOperation *op = NULL;
    SeahorseKeySource *sksrc;
    GSList *sources, *l;
    
    if (!to) {
        to = seahorse_context_find_key_source (sctx, ktype, SKEY_LOC_LOCAL);
        if (!to) {
            /* TODO: How can we warn caller about this. Do we need to? */
            g_warning ("couldn't find a local key source for: %s", 
                       g_quark_to_string (ktype));
            return seahorse_operation_new_complete (NULL);
        }
    }
    
    sources = seahorse_context_find_key_sources (sctx, ktype, SKEY_LOC_REMOTE);
    if (!sources) {
        g_warning ("no key sources found for type: %s", g_quark_to_string (ktype));
        return seahorse_operation_new_complete (NULL);
    }

    for (l = sources; l; l = g_slist_next (l)) {
        
        sksrc = SEAHORSE_KEY_SOURCE (l->data);
        g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc), NULL);
        
        if (op != NULL) {
            if (mop == NULL)
                mop = seahorse_multi_operation_new ();
            seahorse_multi_operation_take (mop, op);
        }
        
        /* Start a new transfer operation between the two key sources */
        op = seahorse_transfer_operation_new (NULL, sksrc, to, keyids);
        g_return_val_if_fail (op != NULL, FALSE);
    }
    
    return mop ? SEAHORSE_OPERATION (mop) : op;
}


GList*
seahorse_context_discover_keys (SeahorseContext *sctx, GQuark ktype, 
                                GSList *rawids)
{
    GList *rkeys = NULL;
    GQuark keyid = 0;
    GSList *todiscover = NULL;
    GList *toimport = NULL;
    SeahorseKeySource *sksrc;
    SeahorseKey* skey;
    SeahorseKeyLoc loc;
    SeahorseOperation *op;
    GSList *l;

    /* Check all the keyids */
    for (l = rawids; l; l = g_slist_next (l)) {
        
        keyid = seahorse_key_source_canonize_keyid (ktype, (gchar*)l->data);
        if (!keyid) {
            /* TODO: Try and match this partial keyid */
            g_warning ("invalid keyid: %s", (gchar*)l->data);
            continue;
        }
        
        /* Do we know about this key? */
        skey = seahorse_context_find_key (sctx, keyid, SKEY_LOC_INVALID);

        /* No such key anywhere, discover it */
        if (!skey) {
            todiscover = g_slist_prepend (todiscover, GUINT_TO_POINTER (keyid));
            keyid = 0;
            continue;
        }
        
        /* Our return value */
        rkeys = g_list_prepend (rkeys, skey);
        
        /* We know about this key, check where it is */
        loc = seahorse_key_get_location (skey);
        g_assert (loc != SKEY_LOC_INVALID);
        
        /* Do nothing for local keys */
        if (loc >= SKEY_LOC_LOCAL)
            continue;
        
        /* Remote keys get imported */
        else if (loc >= SKEY_LOC_REMOTE)
            toimport = g_list_prepend (toimport, skey);
        
        /* Searching keys are ignored */
        else if (loc >= SKEY_LOC_SEARCHING)
            continue;
        
        /* TODO: Should we try SKEY_LOC_UNKNOWN keys again? */
    }
    
    /* Start an import process on all toimport */
    if (toimport) {
        op = seahorse_context_transfer_keys (sctx, toimport, NULL);
        
        g_list_free (toimport);
        
        /* Running operations ref themselves */
        g_object_unref (op);
    }
    
    /* Start a discover process on all todiscover */
    if (seahorse_gconf_get_boolean (AUTORETRIEVE_KEY) && todiscover) {
        op = seahorse_context_retrieve_keys (sctx, ktype, todiscover, NULL);

        /* Add unknown keys for all these */
        sksrc = seahorse_context_find_key_source (sctx, ktype, SKEY_LOC_UNKNOWN);
        for (l = todiscover; l; l = g_slist_next (l)) {
            if (sksrc) {
                skey = seahorse_unknown_source_add_key (SEAHORSE_UNKNOWN_SOURCE (sksrc), 
                                                        GPOINTER_TO_UINT (l->data), op);
                rkeys = g_list_prepend (rkeys, skey);
            }
        }
        
        g_slist_free (todiscover);
        
        /* Running operations ref themselves */
        g_object_unref (op);
    }
    
    return rkeys;
}

SeahorseKey*
seahorse_context_key_from_dbus (SeahorseContext *sctx, const gchar *key, guint *uid)
{
    SeahorseKey *skey;
    const gchar *t = NULL;
    gchar *x, *alloc = NULL;
    
    /* Find the second colon, if any */
    t = strchr (key, ':');
    if (t != NULL) {
        t = strchr (t + 1, ':');
        if(t != NULL) {
            key = alloc = g_strndup (key, t - key);
            ++t;
        }
    }
    
    /* This will always get the most preferred key */
    skey = seahorse_context_find_key (sctx, g_quark_from_string (key), 
                                      SKEY_LOC_INVALID);
    
    if (uid)
        *uid = 0;
        
    /* Parse out the uid */
    if (skey && t) {
        glong l = strtol (t, &x, 10);
            
        /* Make sure it's valid */
        if (*x || l < 0 || l >= seahorse_key_get_num_names (skey))
            skey = NULL;
        else if (uid)
            *uid = (guint)l;
    }
    
    return skey;
}

gchar*
seahorse_context_key_to_dbus (SeahorseContext *sctx, SeahorseKey *skey, guint uid)
{
    return seahorse_context_keyid_to_dbus (sctx, seahorse_key_get_keyid (skey), uid);
}

gchar*
seahorse_context_keyid_to_dbus (SeahorseContext* sctx, GQuark keyid, guint uid)
{
    if (uid == 0)
        return g_strdup (g_quark_to_string (keyid));
    else
        return g_strdup_printf ("%s:%d", g_quark_to_string (keyid), uid);
}
