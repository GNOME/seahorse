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
#include "seahorse-dns-sd.h"
#include "seahorse-gconf.h"
#include "seahorse-libdialogs.h"
#include "seahorse-marshal.h"
#include "seahorse-servers.h"
#include "seahorse-transfer-operation.h"
#include "seahorse-unknown.h"
#include "seahorse-unknown-source.h"
#include "seahorse-util.h"

#include "common/seahorse-bind.h"
#include "common/seahorse-registry.h"

#ifdef WITH_PGP
#include "pgp/seahorse-server-source.h"
#endif

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
 * Two hashtables are used to keep track of the objects:
 *
 *  objects_by_source: This contains a reference to the object and allows us to 
 *    lookup objects by their source. Each object/source combination should be 
 *    unique. Hashkeys are made with hashkey_by_source()
 *  objects_by_type: Each value contains a GList of objects with the same id
 *    (ie: same object from different sources). The objects are 
 *    orderred in by preferred usage. 
 */

struct _SeahorseContextPrivate {
    GSList *sources;                        /* Sources which add keys to this context */
    GHashTable *auto_sources;               /* Automatically added sources (keyservers) */
    GHashTable *objects_by_source;          /* See explanation above */
    GHashTable *objects_by_type;            /* See explanation above */
    guint notify_id;                        /* Notify for GConf watch */
    SeahorseServiceDiscovery *discovery;    /* Adds sources from DNS-SD */
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
                NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, SEAHORSE_TYPE_OBJECT);
    signals[REMOVED] = g_signal_new ("removed", SEAHORSE_TYPE_CONTEXT, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseContextClass, removed),
                NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, SEAHORSE_TYPE_OBJECT);    
    signals[CHANGED] = g_signal_new ("changed", SEAHORSE_TYPE_CONTEXT, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseContextClass, changed),
                NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, SEAHORSE_TYPE_OBJECT);    
}

/* init context, private vars, set prefs, connect signals */
static void
seahorse_context_init (SeahorseContext *sctx)
{
    /* init private vars */
    sctx->pv = g_new0 (SeahorseContextPrivate, 1);

    /* A list of sources */
    sctx->pv->sources = NULL;
    
    /* A table of objects */
    sctx->pv->objects_by_source = g_hash_table_new_full (g_direct_hash, g_direct_equal, 
                                                         NULL, g_object_unref);
    
    sctx->pv->objects_by_type = g_hash_table_new_full (g_direct_hash, g_direct_equal, 
                                                       NULL, NULL);
    
    /* The context is explicitly destroyed */
    g_object_ref (sctx);
    
}

static void
hash_to_slist (gpointer object, gpointer value, gpointer user_data)
{
	*((GSList**)user_data) = g_slist_prepend (*((GSList**)user_data), value);
}

/* release all references */
static void
seahorse_context_dispose (GObject *gobject)
{
    SeahorseContext *sctx;
    GSList *objects, *l;
    
    sctx = SEAHORSE_CONTEXT (gobject);
    
    /* Release all the objects */
    objects = NULL;
    g_hash_table_foreach (sctx->pv->objects_by_source, hash_to_slist, &objects);
    for (l = objects; l; l = g_slist_next (l)) 
	    seahorse_context_remove_object (sctx, l->data);
    g_slist_free (objects);

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
    for (l = sctx->pv->sources; l; l = g_slist_next (l))
        g_object_unref (SEAHORSE_SOURCE (l->data));
    g_slist_free (sctx->pv->sources);
    sctx->pv->sources = NULL;
    
    G_OBJECT_CLASS (seahorse_context_parent_class)->dispose (gobject);
}

/* destroy all objects, free private vars */
static void
seahorse_context_finalize (GObject *gobject)
{
    SeahorseContext *sctx = SEAHORSE_CONTEXT (gobject);
    
    /* Destroy the hash table */        
    if (sctx->pv->objects_by_source)
        g_hash_table_destroy (sctx->pv->objects_by_source);
    if (sctx->pv->objects_by_type)
        g_hash_table_destroy (sctx->pv->objects_by_type);
    
    /* Other stuff already done in dispose */
    g_assert (sctx->pv->sources == NULL);
    g_assert (sctx->pv->auto_sources == NULL);
    g_assert (sctx->pv->discovery == NULL);
    g_free (sctx->pv);
    
    G_OBJECT_CLASS (seahorse_context_parent_class)->finalize (gobject);
}

SeahorseContext*
seahorse_context_for_app (void)
{
    g_return_val_if_fail (app_context != NULL, NULL);
    return app_context;
}
   
SeahorseContext*
seahorse_context_new (guint flags)
{
	SeahorseContext *sctx = g_object_new (SEAHORSE_TYPE_CONTEXT, NULL);
    
    	if (flags & SEAHORSE_CONTEXT_DAEMON)
	    sctx->is_daemon = TRUE;
    
	if (flags & SEAHORSE_CONTEXT_APP) {

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
seahorse_context_take_source (SeahorseContext *sctx, SeahorseSource *sksrc)
{
    g_return_if_fail (SEAHORSE_IS_SOURCE (sksrc));
    
    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));

    if (!g_slist_find (sctx->pv->sources, sksrc))
        sctx->pv->sources = g_slist_append (sctx->pv->sources, sksrc);
}

void
seahorse_context_add_source (SeahorseContext *sctx, SeahorseSource *sksrc)
{
    g_return_if_fail (SEAHORSE_IS_SOURCE (sksrc));

    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
    
    if (g_slist_find (sctx->pv->sources, sksrc))
        return;
    
    sctx->pv->sources = g_slist_append (sctx->pv->sources, sksrc);
    g_object_ref (sksrc);
}
    
void
seahorse_context_remove_source (SeahorseContext *sctx, SeahorseSource *sksrc)
{
    GList *l, *objects;
    
    g_return_if_fail (SEAHORSE_IS_SOURCE (sksrc));

    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));

    if (!g_slist_find (sctx->pv->sources, sksrc)) 
        return;

    /* Remove all objects from this source */    
    objects = seahorse_context_get_objects (sctx, sksrc);
    for (l = objects; l; l = g_list_next (l)) 
        seahorse_context_remove_object (sctx, SEAHORSE_OBJECT (l->data));
    
    /* Remove the source itself */
    sctx->pv->sources = g_slist_remove (sctx->pv->sources, sksrc);
    g_object_unref (sksrc);
}

SeahorseSource*  
seahorse_context_find_source (SeahorseContext *sctx, GQuark ktype,
                              SeahorseLocation location)
{
    SeahorseSource *ks;
    GSList *l;
    
    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    for (l = sctx->pv->sources; l; l = g_slist_next (l)) {
        ks = SEAHORSE_SOURCE (l->data);
        
        if (ktype != SEAHORSE_TAG_INVALID && 
            seahorse_source_get_ktype (ks) != ktype)
            continue;
        
        if (location != SEAHORSE_LOCATION_INVALID && 
            seahorse_source_get_location (ks) != location)
            continue;
        
        return ks;
    }
    
    /* If we don't have an unknown source for this type, create it */
    if (location == SEAHORSE_LOCATION_MISSING && location != SEAHORSE_TAG_INVALID) {
        ks = SEAHORSE_SOURCE (seahorse_unknown_source_new (ktype));
        seahorse_context_add_source (sctx, ks);
        return ks;
    }
    
    return NULL;
}

GSList*
seahorse_context_find_sources (SeahorseContext *sctx, GQuark ktype,
                               SeahorseLocation location)
{
    SeahorseSource *ks;
    GSList *sources = NULL;
    GSList *l;
    
    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    for (l = sctx->pv->sources; l; l = g_slist_next (l)) {
        ks = SEAHORSE_SOURCE (l->data);
        
        if (ktype != SEAHORSE_TAG_INVALID && 
            seahorse_source_get_ktype (ks) != ktype)
            continue;
        
        if (location != SEAHORSE_LOCATION_INVALID && 
            seahorse_source_get_location (ks) != location)
            continue;
        
        sources = g_slist_append (sources, ks);
    }
    
    return sources;
}

SeahorseSource*  
seahorse_context_remote_source (SeahorseContext *sctx, const gchar *uri)
{
    SeahorseSource *ks = NULL;
    gboolean found = FALSE;
    gchar *ks_uri;
    GSList *l;
    
    g_return_val_if_fail (uri && *uri, NULL);

    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    for (l = sctx->pv->sources; l; l = g_slist_next (l)) {
        ks = SEAHORSE_SOURCE (l->data);
        
        if (seahorse_source_get_location (ks) != SEAHORSE_LOCATION_REMOTE)
            continue;
        
        g_object_get (ks, "uri", &ks_uri, NULL);
        if (ks_uri && g_str_equal (ks_uri, uri)) 
            found = TRUE;
        g_free (ks_uri);
        
        if (found)
            return ks;
    }
    
    /* TODO: We need to decouple this properly */
#ifdef WITH_KEYSERVER
#ifdef WITH_PGP
    /* Auto generate one if possible */
    if (sctx->pv->auto_sources) {
        ks = SEAHORSE_SOURCE (seahorse_server_source_new (uri));
        if (ks != NULL) {
            seahorse_context_take_source (sctx, ks);
            g_hash_table_replace (sctx->pv->auto_sources, g_strdup (uri), ks);
        }
    }
#endif /* WITH_PGP */
#endif /* WITH_KEYSERVER */	
    
    return ks;
}


static void
object_notify (SeahorseObject *sobj, GParamSpec *spec, SeahorseContext *sctx)
{
	g_signal_emit (sctx, signals[CHANGED], 0, sobj);
}

static gpointer                 
hashkey_by_source (SeahorseSource *sksrc, GQuark id)
{
    return GINT_TO_POINTER (g_direct_hash (sksrc) ^ 
                            g_str_hash (g_quark_to_string (id)));
}

static gint
sort_by_location (gconstpointer a, gconstpointer b)
{
    guint aloc, bloc;
    
    g_assert (SEAHORSE_IS_OBJECT (a));
    g_assert (SEAHORSE_IS_OBJECT (b));
    
    aloc = seahorse_object_get_location (SEAHORSE_OBJECT (a));
    bloc = seahorse_object_get_location (SEAHORSE_OBJECT (b));
    
    if (aloc == bloc)
        return 0;
    return aloc > bloc ? -1 : 1;
}

static void
setup_objects_by_type (SeahorseContext *sctx, SeahorseObject *sobj, gboolean add)
{
    GList *l, *objects = NULL;
    SeahorseObject *aobj, *next;
    gpointer kt = GUINT_TO_POINTER (seahorse_object_get_id (sobj));
    gboolean first;
    
    /* Get current set of objects in this tag/id */
    if (add)
        objects = g_list_prepend (objects, sobj);
    
    for (aobj = g_hash_table_lookup (sctx->pv->objects_by_type, kt); 
         aobj; aobj = seahorse_object_get_preferred (aobj))
    {
        if (aobj != sobj)
            objects = g_list_prepend (objects, aobj);
    }
    
    /* No objects just remove */
    if (!objects) {
        g_hash_table_remove (sctx->pv->objects_by_type, kt);
        return;
    }

    /* Sort and add back */
    objects = g_list_sort (objects, sort_by_location);
    for (l = objects, first = TRUE; l; l = g_list_next (l), first = FALSE) {
        
        aobj = SEAHORSE_OBJECT (l->data);
        
        /* Set first as start of list */
        if (first)
            g_hash_table_replace (sctx->pv->objects_by_type, kt, aobj);
            
        /* Set next one as preferred */
        else {
            next = g_list_next (l) ? SEAHORSE_OBJECT (g_list_next (l)->data) : NULL;
            seahorse_object_set_preferred (aobj, next);
        }
    }
    
    g_list_free (objects);
}

void
seahorse_context_add_object (SeahorseContext *sctx, SeahorseObject *sobj)
{
    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));

    g_object_ref (sobj);
    seahorse_context_take_object (sctx, sobj);
}

void
seahorse_context_take_object (SeahorseContext *sctx, SeahorseObject *sobj)
{
    gpointer ks;
    
    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
    g_return_if_fail (SEAHORSE_IS_OBJECT (sobj));
    g_return_if_fail (seahorse_object_get_id (sobj) != 0);
    
    ks = hashkey_by_source (seahorse_object_get_source (sobj), 
                            seahorse_object_get_id (sobj));
    
    g_return_if_fail (!g_hash_table_lookup (sctx->pv->objects_by_source, ks));

    g_object_ref (sobj);

    g_object_set (sobj, "context", sctx, NULL);
    g_hash_table_replace (sctx->pv->objects_by_source, ks, sobj);
    setup_objects_by_type (sctx, sobj, TRUE);
    g_signal_emit (sctx, signals[ADDED], 0, sobj);
    g_object_unref (sobj);
    
    g_signal_connect (sobj, "notify", G_CALLBACK (object_notify), sctx);
}

guint
seahorse_context_get_count (SeahorseContext *sctx)
{
    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), 0);
    return g_hash_table_size (sctx->pv->objects_by_source);
}

SeahorseObject*        
seahorse_context_get_object (SeahorseContext *sctx, SeahorseSource *sksrc,
                             GQuark id)
{
    gconstpointer k;
    
    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);
    g_return_val_if_fail (SEAHORSE_IS_SOURCE (sksrc), NULL);
    
    k = hashkey_by_source (sksrc, id);
    return SEAHORSE_OBJECT (g_hash_table_lookup (sctx->pv->objects_by_source, k));
}

typedef struct _ObjectMatcher {
	SeahorseObjectPredicate *kp;
	gboolean many;
	SeahorseObjectFunc func;
	gpointer user_data;
} ObjectMatcher;

gboolean
find_matching_objects (gpointer key, SeahorseObject *sobj, ObjectMatcher *km)
{
	gboolean matched;
	
	if (km->kp && seahorse_object_predicate_match (km->kp, SEAHORSE_OBJECT (sobj))) {
		matched = TRUE;
		(km->func) (sobj, km->user_data);
	}

	/* Terminate search */
	if (!km->many && matched)
		return TRUE;

	/* Keep going */
	return FALSE;
}

static void
add_object_to_list (SeahorseObject *object, gpointer user_data)
{
	GList** list = (GList**)user_data;
	*list = g_list_prepend (*list, object);
}

GList*
seahorse_context_find_objects_full (SeahorseContext *self, SeahorseObjectPredicate *pred)
{
	GList *list = NULL;
	ObjectMatcher km;

	if (!self)
		self = seahorse_context_for_app ();
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (self), NULL);
	g_return_val_if_fail (pred, NULL);

	memset (&km, 0, sizeof (km));
	km.kp = pred;
	km.many = TRUE;
	km.func = add_object_to_list;
	km.user_data = &list;

	g_hash_table_find (self->pv->objects_by_source, (GHRFunc)find_matching_objects, &km);
	return list; 
}

void
seahorse_context_for_objects_full (SeahorseContext *self, SeahorseObjectPredicate *pred,
                                   SeahorseObjectFunc func, gpointer user_data)
{
	ObjectMatcher km;

	if (!self)
		self = seahorse_context_for_app ();
	g_return_if_fail (SEAHORSE_IS_CONTEXT (self));
	g_return_if_fail (pred);
	g_return_if_fail (func);

	memset (&km, 0, sizeof (km));
	km.kp = pred;
	km.many = TRUE;
	km.func = func;
	km.user_data = user_data;

	g_hash_table_find (self->pv->objects_by_source, (GHRFunc)find_matching_objects, &km);
}

GList*
seahorse_context_get_objects (SeahorseContext *self, SeahorseSource *source)
{
	SeahorseObjectPredicate pred;

	if (!self)
		self = seahorse_context_for_app ();
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (self), NULL);
	g_return_val_if_fail (source == NULL || SEAHORSE_IS_SOURCE (source), NULL);

	seahorse_object_predicate_clear (&pred);
	pred.source = source;
	
	return seahorse_context_find_objects_full (self, &pred);
}

SeahorseObject*        
seahorse_context_find_object (SeahorseContext *sctx, GQuark id, SeahorseLocation location)
{
    SeahorseObject *sobj; 

    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    sobj = (SeahorseObject*)g_hash_table_lookup (sctx->pv->objects_by_type, GUINT_TO_POINTER (id));
    while (sobj) {
        
        /* If at the end and no more objects in list, return */
        if (location == SEAHORSE_LOCATION_INVALID && !seahorse_object_get_preferred (sobj))
            return sobj;
        
        if (location >= seahorse_object_get_location (sobj))
            return sobj;
        
        /* Look down the list for this location */
        sobj = seahorse_object_get_preferred (sobj);
    }
    
    return NULL;
}

GList*
seahorse_context_find_objects (SeahorseContext *sctx, GQuark ktype, 
                               SeahorseUsage usage, SeahorseLocation location)
{
    SeahorseObjectPredicate pred;
    memset (&pred, 0, sizeof (pred));

    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);
    
    pred.tag = ktype;
    pred.usage = usage;
    pred.location = location;
    
    return seahorse_context_find_objects_full (sctx, &pred);
}

void 
seahorse_context_remove_object (SeahorseContext *sctx, SeahorseObject *sobj)
{
    gconstpointer k;
    
    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
    g_return_if_fail (SEAHORSE_IS_OBJECT (sobj));
    
    k = hashkey_by_source (seahorse_object_get_source (sobj), 
                           seahorse_object_get_id (sobj));
    
    if (g_hash_table_lookup (sctx->pv->objects_by_source, k)) {
        g_return_if_fail (seahorse_object_get_context (sobj) == sctx);

        g_object_ref (sobj);
        g_signal_handlers_disconnect_by_func (sobj, object_notify, sctx);
        g_object_set (sobj, "context", NULL, NULL);
        g_hash_table_remove (sctx->pv->objects_by_source, k);
        setup_objects_by_type (sctx, sobj, FALSE);
        g_signal_emit (sctx, signals[REMOVED], 0, sobj);    
        g_object_unref (sobj);
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
SeahorseObject*
seahorse_context_get_default_key (SeahorseContext *sctx)
{
    SeahorseObject *sobj = NULL;
    gchar *id;
    
    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    /* TODO: All of this needs to take multiple key types into account */
    
    id = seahorse_gconf_get_string (SEAHORSE_DEFAULT_KEY);
    if (id != NULL && id[0]) {
        GQuark keyid = g_quark_from_string (id);
        sobj = seahorse_context_find_object (sctx, keyid, SEAHORSE_LOCATION_LOCAL);
    }
    
    g_free (id);
    
    return sobj;
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
    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);
    g_return_val_if_fail (sctx->pv->discovery != NULL, NULL);
    
    return sctx->pv->discovery;
}

SeahorseOperation*  
seahorse_context_refresh_local (SeahorseContext *sctx)
{
    SeahorseSource *ks;
    SeahorseMultiOperation *mop = NULL;
    SeahorseOperation *op = NULL;
    GSList *l;
    
    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    for (l = sctx->pv->sources; l; l = g_slist_next (l)) {
        ks = SEAHORSE_SOURCE (l->data);
        
        if (seahorse_source_get_location (ks) == SEAHORSE_LOCATION_LOCAL) {
            if (mop == NULL && op != NULL) {
                mop = seahorse_multi_operation_new ();
                seahorse_multi_operation_take (mop, op);
            }
            
            op = seahorse_source_load (ks);
            
            if (mop != NULL)
                seahorse_multi_operation_take (mop, op);
        }
    }   

    return mop ? SEAHORSE_OPERATION (mop) : op;  
}

static gboolean 
refresh_local (SeahorseContext *sctx)
{
    SeahorseOperation *op = seahorse_context_refresh_local (sctx);
    g_return_val_if_fail (op != NULL, FALSE);
    g_object_unref (op);
    return FALSE;
}

void
seahorse_context_refresh_local_async (SeahorseContext *sctx)
{
    g_idle_add_full (G_PRIORITY_LOW, (GSourceFunc)refresh_local, sctx, NULL);
}


SeahorseOperation*  
seahorse_context_search_remote (SeahorseContext *sctx, const gchar *search)
{
    SeahorseSource *ks;
    SeahorseMultiOperation *mop = NULL;
    SeahorseOperation *op = NULL;
    GSList *l, *names;
    GHashTable *servers = NULL;
    gchar *uri;
    
    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);
    
    /* Get a list of all selected key servers */
    names = seahorse_gconf_get_string_list (LASTSERVERS_KEY);
    if (names) {
        servers = g_hash_table_new (g_str_hash, g_str_equal);
        for (l = names; l; l = g_slist_next (l)) 
            g_hash_table_insert (servers, l->data, GINT_TO_POINTER (TRUE));
    }
        
    for (l = sctx->pv->sources; l; l = g_slist_next (l)) {
        ks = SEAHORSE_SOURCE (l->data);
        
        if (seahorse_source_get_location (ks) != SEAHORSE_LOCATION_REMOTE)
            continue;

        if (servers) {
            g_object_get (ks, "uri", &uri, NULL);
            if (!g_hash_table_lookup (servers, uri)) {
                g_free (uri);
                continue;
            }
            
            g_free (uri);
        }

        if (mop == NULL && op != NULL) {
            mop = seahorse_multi_operation_new ();
            seahorse_multi_operation_take (mop, op);
        }
            
        op = seahorse_source_search (ks, search);
            
        if (mop != NULL)
            seahorse_multi_operation_take (mop, op);
    }   

    seahorse_util_string_slist_free (names);
    return mop ? SEAHORSE_OPERATION (mop) : op;  
}

#ifdef WITH_KEYSERVER
#ifdef WITH_PGP
/* For copying the keys */
static void 
auto_source_to_hash (const gchar *uri, SeahorseSource *sksrc, GHashTable *ht)
{
    g_hash_table_replace (ht, (gpointer)uri, sksrc);
}

static void
auto_source_remove (const gchar* uri, SeahorseSource *sksrc, SeahorseContext *sctx)
{
    seahorse_context_remove_source (sctx, sksrc);
    g_hash_table_remove (sctx->pv->auto_sources, uri);
}
#endif 
#endif

static void
refresh_keyservers (GConfClient *client, guint id, GConfEntry *entry, 
                    SeahorseContext *sctx)
{
#ifdef WITH_KEYSERVER
#ifdef WITH_PGP
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
    keyservers = seahorse_servers_get_uris ();
    
    for (l = keyservers; l; l = g_slist_next (l)) {
        uri = (const gchar*)(l->data);
        
        /* If we don't have a keysource then add it */
        if (!g_hash_table_lookup (sctx->pv->auto_sources, uri)) {
            ssrc = seahorse_server_source_new (uri);
            if (ssrc != NULL) {
                seahorse_context_take_source (sctx, SEAHORSE_SOURCE (ssrc));
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
#endif /* WITH_PGP */
#endif /* WITH_KEYSERVER */
}

SeahorseOperation*
seahorse_context_transfer_objects (SeahorseContext *sctx, GList *objects, 
                                   SeahorseSource *to)
{
    SeahorseSource *from;
    SeahorseOperation *op = NULL;
    SeahorseMultiOperation *mop = NULL;
    SeahorseObject *sobj;
    GSList *ids = NULL;
    GList *next, *l;
    GQuark ktype;

    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    objects = g_list_copy (objects);
    
    /* Sort by key source */
    objects = seahorse_util_objects_sort (objects);
    
    while (objects) {
        
        /* break off one set (same keysource) */
        next = seahorse_util_objects_splice (objects);

        g_assert (SEAHORSE_IS_OBJECT (objects->data));
        sobj = SEAHORSE_OBJECT (objects->data);

        /* Export from this key source */
        from = seahorse_object_get_source (sobj);
        g_return_val_if_fail (from != NULL, FALSE);
        ktype = seahorse_source_get_ktype (from);
        
        /* Find a local keysource to import to */
        if (!to) {
            to = seahorse_context_find_source (sctx, ktype, SEAHORSE_LOCATION_LOCAL);
            if (!to) {
                /* TODO: How can we warn caller about this. Do we need to? */
                g_warning ("couldn't find a local source for: %s", 
                           g_quark_to_string (ktype));
            }
        }
        
        /* Make sure it's the same type */
        if (ktype != seahorse_source_get_ktype (to)) {
            /* TODO: How can we warn caller about this. Do we need to? */
            g_warning ("destination is not of type: %s", 
                       g_quark_to_string (ktype));
        }
        
        if (to != NULL && from != to) {
            
            if (op != NULL) {
                if (mop == NULL)
                    mop = seahorse_multi_operation_new ();
                seahorse_multi_operation_take (mop, op);
            }
            
            /* Build id list */
            for (l = objects; l; l = g_list_next (l)) 
                ids = g_slist_prepend (ids, GUINT_TO_POINTER (seahorse_object_get_id (l->data)));
            ids = g_slist_reverse (ids);
        
            /* Start a new transfer operation between the two sources */
            op = seahorse_transfer_operation_new (NULL, from, to, ids);
            g_return_val_if_fail (op != NULL, FALSE);
            
            g_slist_free (ids);
            ids = NULL;
        }

        g_list_free (objects);
        objects = next;
    } 
    
    /* No objects done, just return success */
    if (!mop && !op) {
        g_warning ("no valid objects to transfer found");
        return seahorse_operation_new_complete (NULL);
    }
    
    return mop ? SEAHORSE_OPERATION (mop) : op;
}

SeahorseOperation*
seahorse_context_retrieve_objects (SeahorseContext *sctx, GQuark ktype, 
                                   GSList *ids, SeahorseSource *to)
{
    SeahorseMultiOperation *mop = NULL;
    SeahorseOperation *op = NULL;
    SeahorseSource *sksrc;
    GSList *sources, *l;
    
    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    if (!to) {
        to = seahorse_context_find_source (sctx, ktype, SEAHORSE_LOCATION_LOCAL);
        if (!to) {
            /* TODO: How can we warn caller about this. Do we need to? */
            g_warning ("couldn't find a local source for: %s", 
                       g_quark_to_string (ktype));
            return seahorse_operation_new_complete (NULL);
        }
    }
    
    sources = seahorse_context_find_sources (sctx, ktype, SEAHORSE_LOCATION_REMOTE);
    if (!sources) {
        g_warning ("no sources found for type: %s", g_quark_to_string (ktype));
        return seahorse_operation_new_complete (NULL);
    }

    for (l = sources; l; l = g_slist_next (l)) {
        
        sksrc = SEAHORSE_SOURCE (l->data);
        g_return_val_if_fail (SEAHORSE_IS_SOURCE (sksrc), NULL);
        
        if (op != NULL) {
            if (mop == NULL)
                mop = seahorse_multi_operation_new ();
            seahorse_multi_operation_take (mop, op);
        }
        
        /* Start a new transfer operation between the two key sources */
        op = seahorse_transfer_operation_new (NULL, sksrc, to, ids);
        g_return_val_if_fail (op != NULL, FALSE);
    }
    
    return mop ? SEAHORSE_OPERATION (mop) : op;
}


GList*
seahorse_context_discover_objects (SeahorseContext *sctx, GQuark ktype, 
                                   GSList *rawids)
{
    GList *robjects = NULL;
    GQuark id = 0;
    GSList *todiscover = NULL;
    GList *toimport = NULL;
    SeahorseSource *sksrc;
    SeahorseObject* sobj;
    SeahorseLocation loc;
    SeahorseOperation *op;
    GSList *l;

    if (!sctx)
        sctx = seahorse_context_for_app ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    /* Check all the ids */
    for (l = rawids; l; l = g_slist_next (l)) {
        
        id = seahorse_context_canonize_id (ktype, (gchar*)l->data);
        if (!id) {
            /* TODO: Try and match this partial id */
            g_warning ("invalid id: %s", (gchar*)l->data);
            continue;
        }
        
        /* Do we know about this object? */
        sobj = seahorse_context_find_object (sctx, id, SEAHORSE_LOCATION_INVALID);

        /* No such object anywhere, discover it */
        if (!sobj) {
            todiscover = g_slist_prepend (todiscover, GUINT_TO_POINTER (id));
            id = 0;
            continue;
        }
        
        /* Our return value */
        robjects = g_list_prepend (robjects, sobj);
        
        /* We know about this object, check where it is */
        loc = seahorse_object_get_location (sobj);
        g_assert (loc != SEAHORSE_LOCATION_INVALID);
        
        /* Do nothing for local objects */
        if (loc >= SEAHORSE_LOCATION_LOCAL)
            continue;
        
        /* Remote objects get imported */
        else if (loc >= SEAHORSE_LOCATION_REMOTE)
            toimport = g_list_prepend (toimport, sobj);
        
        /* Searching objects are ignored */
        else if (loc >= SEAHORSE_LOCATION_SEARCHING)
            continue;
        
        /* TODO: Should we try SEAHORSE_LOCATION_MISSING objects again? */
    }
    
    /* Start an import process on all toimport */
    if (toimport) {
        op = seahorse_context_transfer_objects (sctx, toimport, NULL);
        
        g_list_free (toimport);
        
        /* Running operations ref themselves */
        g_object_unref (op);
    }
    
    /* Start a discover process on all todiscover */
    if (seahorse_gconf_get_boolean (AUTORETRIEVE_KEY) && todiscover) {
        op = seahorse_context_retrieve_objects (sctx, ktype, todiscover, NULL);

        /* Add unknown objects for all these */
        sksrc = seahorse_context_find_source (sctx, ktype, SEAHORSE_LOCATION_MISSING);
        for (l = todiscover; l; l = g_slist_next (l)) {
            if (sksrc) {
                sobj = seahorse_unknown_source_add_object (SEAHORSE_UNKNOWN_SOURCE (sksrc), 
                                                           GPOINTER_TO_UINT (l->data), op);
                robjects = g_list_prepend (robjects, sobj);
            }
        }
        
        g_slist_free (todiscover);
        
        /* Running operations ref themselves */
        g_object_unref (op);
    }
    
    return robjects;
}

SeahorseObject*
seahorse_context_object_from_dbus (SeahorseContext *sctx, const gchar *key)
{
    SeahorseObject *sobj;
    
    /* This will always get the most preferred key */
    sobj = seahorse_context_find_object (sctx, g_quark_from_string (key), 
                                         SEAHORSE_LOCATION_INVALID);
    
    return sobj;
}

gchar*
seahorse_context_object_to_dbus (SeahorseContext *sctx, SeahorseObject *sobj)
{
    return seahorse_context_id_to_dbus (sctx, seahorse_object_get_id (sobj));
}

gchar*
seahorse_context_id_to_dbus (SeahorseContext* sctx, GQuark id)
{
	return g_strdup (g_quark_to_string (id));
}

GQuark
seahorse_context_canonize_id (GQuark ktype, const gchar *id)
{
	SeahorseCanonizeFunc canonize;

	g_return_val_if_fail (id != NULL, 0);
    
	canonize = seahorse_registry_lookup_function (NULL, "canonize", g_quark_to_string (ktype), NULL);
	if (!canonize) 
		return 0;
	
	return (canonize) (id);
}
