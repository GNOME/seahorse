/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2005 Nate Nielsen
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

#include "config.h"

#include "seahorse-pgp-key.h"
#include "seahorse-context.h"
#include "seahorse-marshal.h"
#include "seahorse-libdialogs.h"
#include "seahorse-gconf.h"
#include "seahorse-util.h"
#include "seahorse-server-source.h"
#include "seahorse-pgp-source.h"
#include "seahorse-dns-sd.h"

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

struct _SeahorseContextPrivate {
    GSList *sources;						/* Key sources which add keys to this context */
    GHashTable *keys;                       /* A list of all keys in the context */
    GHashTable *auto_sources;               /* Automatically added key sources (keyservers) */
    guint notify_id;                        /* Notify for GConf watch */
    SeahorseServiceDiscovery *discovery;    /* Adds key sources from DNS-SD */
};

static void	seahorse_context_dispose	(GObject *gobject);
static void seahorse_context_finalize   (GObject *gobject);

/* Forward declarations */
static void refresh_keyservers          (GConfClient *client, guint id, 
                                         GConfEntry *entry, SeahorseContext *sctx);

static GtkObjectClass	*parent_class			= NULL;

static void
seahorse_context_class_init (SeahorseContextClass *klass)
{
	GObjectClass *gobject_class;
	
	parent_class = g_type_class_peek_parent (klass);
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
    sctx->pv->keys = g_hash_table_new_full (g_direct_hash, g_direct_equal, 
                                            NULL, g_object_unref);
    
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
    g_hash_table_foreach_remove (sctx->pv->keys, remove_each, NULL);

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
    
    G_OBJECT_CLASS (parent_class)->dispose (gobject);
}

/* destroy all keys, free private vars */
static void
seahorse_context_finalize (GObject *gobject)
{
	SeahorseContext *sctx;
	
	sctx = SEAHORSE_CONTEXT (gobject);
    
    /* Destroy the hash table */        
    if (sctx->pv->keys)
        g_hash_table_destroy (sctx->pv->keys);
    
    /* Other stuff already done in dispose */
    g_assert (sctx->pv->sources == NULL);
    g_assert (sctx->pv->auto_sources == NULL);
    g_assert (sctx->pv->discovery == NULL);
	g_free (sctx->pv);
   	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

SeahorseContext*
seahorse_context_app (void)
{
    g_return_val_if_fail (app_context != NULL, NULL);
    return app_context;
}
   
SeahorseContext*
seahorse_context_new (gboolean app_ctx)
{
	SeahorseContext *sctx = g_object_new (SEAHORSE_TYPE_CONTEXT, NULL);
    SeahorsePGPSource *pgpsrc;
    
    if (app_ctx) {

        /* Add the default key sources */
        pgpsrc = seahorse_pgp_source_new ();
        seahorse_context_take_key_source (sctx, SEAHORSE_KEY_SOURCE (pgpsrc));     

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
    SeahorseKeySource *ks;
    GSList *l;
    
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc));

    if (!g_slist_find (sctx->pv->sources, sksrc)) 
        return;

    /* Remove all keys from this source */    
    for (l = sctx->pv->sources; l; l = g_slist_next (l)) {
        ks = seahorse_key_get_source (SEAHORSE_KEY (l->data));
        if (ks == sksrc)
            seahorse_context_remove_key (sctx, SEAHORSE_KEY (l->data));
    }
    
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
        
        if (location != SKEY_LOC_UNKNOWN && 
            seahorse_key_source_get_location (ks) != location)
            continue;
        
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
        
        if (location != SKEY_LOC_UNKNOWN && 
            seahorse_key_source_get_location (ks) != location)
            continue;
        
        sources = g_slist_append (sources, ks);
    }
    
    return sources;
}

SeahorseKeySource*  
seahorse_context_remote_key_source (SeahorseContext *sctx, const gchar *uri)
{
    SeahorseKeySource *ks;
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
    
    /* Auto generate one if possible */
    if (sctx->pv->auto_sources) {
        ks = SEAHORSE_KEY_SOURCE (seahorse_server_source_new (uri));
        if (ks != NULL) {
            seahorse_context_take_key_source (sctx, ks);
            g_hash_table_replace (sctx->pv->auto_sources, g_strdup (uri), ks);
        }
    }
    
    return ks;
}


static void
key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorseContext *sctx)
{
    g_signal_emit (sctx, signals[CHANGED], 0, skey, change);
}

static gpointer                 
hashtable_key (SeahorseKeySource *sksrc, const gchar *keyid)
{
    return GINT_TO_POINTER (g_direct_hash (sksrc) ^ 
                            g_str_hash (keyid));
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
    gpointer k;
    
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
    g_return_if_fail (SEAHORSE_IS_KEY (skey));
    
    k = hashtable_key (seahorse_key_get_source (skey), 
                       seahorse_key_get_keyid (skey));
    
    g_return_if_fail (!g_hash_table_lookup (sctx->pv->keys, k));

    g_object_ref (skey);
    g_hash_table_replace (sctx->pv->keys, k, skey);
    g_signal_emit (sctx, signals[ADDED], 0, skey);
    g_object_unref (skey);
    
    g_signal_connect (skey, "changed", G_CALLBACK (key_changed), sctx);
}

guint
seahorse_context_get_count (SeahorseContext *sctx)
{
    return g_hash_table_size (sctx->pv->keys);
}

SeahorseKey*        
seahorse_context_get_key (SeahorseContext *sctx, SeahorseKeySource  *sksrc,
                          const gchar *keyid)
{
    gconstpointer k;
    guint l;
    
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc), NULL);
    
    l = strlen (keyid);
    if (l >= 16)
        keyid += l - 16;

    k = hashtable_key (sksrc, keyid);

    /* TODO: We need a provision to create this key on the fly (dummy) */
    return SEAHORSE_KEY (g_hash_table_lookup (sctx->pv->keys, k));
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
    
    g_hash_table_find (sctx->pv->keys, (GHRFunc)find_matching_keys, &km);
    return km.keys;
}

SeahorseKey*        
seahorse_context_find_key (SeahorseContext *sctx, GQuark ktype,
                           SeahorseKeyLoc location, const gchar *keyid)
{
    SeahorseKeyPredicate kp;
    SeahorseKey *skey = NULL;
    KeyMatcher km;
    guint l;
    
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    memset (&kp, 0, sizeof (kp));
    memset (&km, 0, sizeof (km));
    
    if (keyid) {
        l = strlen (keyid);
        if (l >= 16)
            keyid += l - 16;
    }
    
    km.kp = &kp;
    km.many = FALSE;
    kp.ktype = ktype;
    kp.location = location;
    kp.keyid = keyid;
    
    g_hash_table_find (sctx->pv->keys, (GHRFunc)find_matching_keys, &km);
    
    if (km.keys) {
        skey = km.keys->data;
        g_list_free (km.keys);
    }
    
    return skey;
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
    
    g_hash_table_find (sctx->pv->keys, (GHRFunc)find_matching_keys, &km);
    return km.keys; 
}

void 
seahorse_context_remove_key (SeahorseContext *sctx, SeahorseKey *skey)
{
    gconstpointer k;
    
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
    g_return_if_fail (SEAHORSE_IS_KEY (skey));
    
    k = hashtable_key (seahorse_key_get_source (skey), 
                       seahorse_key_get_keyid (skey));
    
    g_return_if_fail (g_hash_table_lookup (sctx->pv->keys, k));

    g_object_ref (skey);
    g_signal_handlers_disconnect_by_func (skey, key_changed, sctx);
    g_hash_table_remove (sctx->pv->keys, k);
    g_signal_emit (sctx, signals[REMOVED], 0, skey);    
    g_object_unref (skey);
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
    
    id = seahorse_gconf_get_string (DEFAULT_KEY);
    if (id != NULL && id[0]) 
        skey = seahorse_context_find_key (sctx, SKEY_PGP, SKEY_LOC_LOCAL, id);
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
            if (op != NULL) {
                mop = seahorse_multi_operation_new ();
                seahorse_multi_operation_take (mop, op);
            }
            
            op = seahorse_key_source_load (ks, SKSRC_LOAD_ALL, NULL);
            
            if (mop != NULL)
                seahorse_multi_operation_take (mop, op);
        }
    }   

    return mop ? SEAHORSE_OPERATION (mop) : op;  
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
            
        op = seahorse_key_source_load (ks, SKSRC_LOAD_SEARCH, search);
            
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
}
