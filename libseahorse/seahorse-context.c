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

/**
 * SECTION:seahorse-context
 * @short_description: This is where all the action in a Seahorse process comes together.
 * @include:libseahorse/seahorse-context.h
 *
 **/

/* The application main context */
SeahorseContext* app_context = NULL;

enum {
    ADDED,
    REMOVED,
    CHANGED,
    REFRESHING,
    DESTROY,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (SeahorseContext, seahorse_context, G_TYPE_OBJECT);

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
    SeahorseMultiOperation *refresh_ops;    /* Operations for refreshes going on */
    SeahorseServiceDiscovery *discovery;    /* Adds sources from DNS-SD */
    gboolean in_destruction;                /* In destroy signal */
};

static void seahorse_context_dispose    (GObject *gobject);
static void seahorse_context_finalize   (GObject *gobject);

/* Forward declarations */
static void refresh_keyservers          (GConfClient *client, guint id, 
                                         GConfEntry *entry, SeahorseContext *sctx);

static void
seahorse_context_constructed (GObject *obj)
{
	SeahorseContext *self = SEAHORSE_CONTEXT (obj);

	G_OBJECT_CLASS(seahorse_context_parent_class)->constructed (obj);

	/* DNS-SD discovery */
	self->pv->discovery = seahorse_service_discovery_new ();

	/* Automatically added remote key sources */
	self->pv->auto_sources = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                                g_free, NULL);

	/* Listen for new gconf remote key sources automatically */
	self->pv->notify_id = seahorse_gconf_notify (KEYSERVER_KEY,
	                                             (GConfClientNotifyFunc)refresh_keyservers, self);

	refresh_keyservers (NULL, 0, NULL, self);

}
/**
* klass: The class to initialise
*
* Inits the #SeahorseContextClass. Adds the signals "added", "removed", "changed"
* and "refreshing"
**/
static void
seahorse_context_class_init (SeahorseContextClass *klass)
{
    GObjectClass *gobject_class;
    
    seahorse_context_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->constructed = seahorse_context_constructed;
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
    signals[REFRESHING] = g_signal_new ("refreshing", SEAHORSE_TYPE_CONTEXT, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseContextClass, refreshing),
                NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, SEAHORSE_TYPE_OPERATION);    
    signals[DESTROY] = g_signal_new ("destroy", SEAHORSE_TYPE_CONTEXT,
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseContextClass, destroy),
                NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

/* init context, private vars, set prefs, connect signals */
/**
* sctx: The context to initialise
*
* Initialises the Sahorse Context @sctx
*
**/
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
}

/**
* object: ignored
* value: the value to prepend
* user_data: GSList ** to prepend to
*
* Prepends #value to #user_data
*
**/
static void
hash_to_ref_slist (gpointer object, gpointer value, gpointer user_data)
{
	*((GSList**)user_data) = g_slist_prepend (*((GSList**)user_data), g_object_ref (value));
}


/**
* gobject: the object to dispose (#SeahorseContext)
*
* release all references
*
**/
static void
seahorse_context_dispose (GObject *gobject)
{
    SeahorseContext *sctx;
    GSList *objects, *l;
    
    sctx = SEAHORSE_CONTEXT (gobject);
    
    /* Release all the objects */
    objects = NULL;
    g_hash_table_foreach (sctx->pv->objects_by_source, hash_to_ref_slist, &objects);
    for (l = objects; l; l = g_slist_next (l)) {
            seahorse_context_remove_object (sctx, l->data);
            g_object_unref (G_OBJECT (l->data));
    }
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
    
    if (sctx->pv->refresh_ops)
	    g_object_unref (sctx->pv->refresh_ops);
    sctx->pv->refresh_ops = NULL;

	if (!sctx->pv->in_destruction) {
		sctx->pv->in_destruction = TRUE;
		g_signal_emit (sctx, signals[DESTROY], 0);
		sctx->pv->in_destruction = FALSE;
	}

	G_OBJECT_CLASS (seahorse_context_parent_class)->dispose (gobject);
}


/**
* gobject: The #SeahorseContext to finalize
*
* destroy all objects, free private vars
*
**/
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
    g_assert (sctx->pv->refresh_ops == NULL);
    g_free (sctx->pv);
    
    G_OBJECT_CLASS (seahorse_context_parent_class)->finalize (gobject);
}

/**
* seahorse_context_instance:
*
* Returns: the application main context as #SeahorseContext
*/
SeahorseContext*
seahorse_context_instance (void)
{
    g_return_val_if_fail (app_context != NULL, NULL);
    return app_context;
}
   
/**
 * seahorse_context_instance:
 *
 * Gets the main seahorse context or returns a new one if none already exist.
 *
 * Returns: The context instance
 */
void
seahorse_context_create (void)
{
	g_return_if_fail (app_context == NULL);
	app_context = g_object_new (SEAHORSE_TYPE_CONTEXT, NULL);
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
	g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
	g_return_if_fail (sctx == app_context);
	g_object_run_dispose (G_OBJECT (sctx));
	g_object_unref (sctx);
	app_context = NULL;
}

/**
* sctx: #SeahorseContext to add the source to
* sksrc: #SeahorseSource to add
*
* Adds the source to the context
*
* Returns TRUE if the source was added, FALSE if it was already there
**/
static gboolean                
take_source (SeahorseContext *sctx, SeahorseSource *sksrc)
{
	g_return_val_if_fail (SEAHORSE_IS_SOURCE (sksrc), FALSE);
	if (!g_slist_find (sctx->pv->sources, sksrc)) {
		sctx->pv->sources = g_slist_append (sctx->pv->sources, sksrc);
		return TRUE;
	}
	
	return FALSE;
}

/**
 * seahorse_context_take_source:
 * @sctx: A context to add a source to, can be NULL
 * @sksrc: The source to add
 *
 * Adds @sksrc to the @sctx. If @sctx is NULL it will use the application context.
 *
 */
void                
seahorse_context_take_source (SeahorseContext *sctx, SeahorseSource *sksrc)
{
	g_return_if_fail (SEAHORSE_IS_SOURCE (sksrc));
    
	if (!sctx)
		sctx = seahorse_context_instance ();
	g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));

	take_source (sctx, sksrc);
}

/**
 * seahorse_context_add_source:
 * @sctx: A context to add a source to, can be NULL
 * @sksrc: The source to add
 *
 * Adds @sksrc to the @sctx. If @sctx is NULL it will use the application context.
 * It also adds a reference to the new added source.
 */
void
seahorse_context_add_source (SeahorseContext *sctx, SeahorseSource *sksrc)
{
	g_return_if_fail (SEAHORSE_IS_SOURCE (sksrc));
    
	if (!sctx)
		sctx = seahorse_context_instance ();
	g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));

	if (take_source (sctx, sksrc))
		g_object_ref (sksrc);
}
    
/**
 * seahorse_context_remove_source:
 * @sctx: Context to remove objects from
 * @sksrc: The source to remove
 *
 * Remove all objects from source @sksrc from the #SeahorseContext @sctx
 *
 */
void
seahorse_context_remove_source (SeahorseContext *sctx, SeahorseSource *sksrc)
{
    GList *l, *objects;
    
    g_return_if_fail (SEAHORSE_IS_SOURCE (sksrc));

    if (!sctx)
        sctx = seahorse_context_instance ();
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

/**
 * seahorse_context_find_source:
 * @sctx: A #SeahorseContext
 * @ktype: A seahorse tag (SEAHORSE_TAG_INVALID is wildcard)
 * @location: A location (SEAHORSE_LOCATION_INVALID is wildcard)
 *
 * Finds a context where @ktype and @location match
 *
 * Returns: The context
 */
SeahorseSource*  
seahorse_context_find_source (SeahorseContext *sctx, GQuark ktype,
                              SeahorseLocation location)
{
    SeahorseSource *ks;
    GSList *l;
    
    if (!sctx)
        sctx = seahorse_context_instance ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    for (l = sctx->pv->sources; l; l = g_slist_next (l)) {
        ks = SEAHORSE_SOURCE (l->data);
        
        if (ktype != SEAHORSE_TAG_INVALID && 
            seahorse_source_get_tag (ks) != ktype)
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


/**
* seahorse_context_find_sources:
* @sctx: the context to work with
* @ktype: the type of the key to match. Or SEAHORSE_TAG_INVALID
* @location: the location to match. Or SEAHORSE_LOCATION_INVALID
*
* Returns: A list of seahorse sources matching @ktype and @location as #GSList. Must
*  be freed with #g_slist_free
*/
GSList*
seahorse_context_find_sources (SeahorseContext *sctx, GQuark ktype,
                               SeahorseLocation location)
{
    SeahorseSource *ks;
    GSList *sources = NULL;
    GSList *l;
    
    if (!sctx)
        sctx = seahorse_context_instance ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    for (l = sctx->pv->sources; l; l = g_slist_next (l)) {
        ks = SEAHORSE_SOURCE (l->data);
        
        if (ktype != SEAHORSE_TAG_INVALID && 
            seahorse_source_get_tag (ks) != ktype)
            continue;
        
        if (location != SEAHORSE_LOCATION_INVALID && 
            seahorse_source_get_location (ks) != location)
            continue;
        
        sources = g_slist_append (sources, ks);
    }
    
    return sources;
}

/**
 * seahorse_context_remote_source:
 * @sctx: the context to add the source to (can be NULL)
 * @uri: An URI to add as remote source
 *
 * Add a remote source to the Context @sctx. If it already exists, the source
 * object will be returned.
 *
 * Returns: The #SeahorseSource with this URI
 */
SeahorseSource*  
seahorse_context_remote_source (SeahorseContext *sctx, const gchar *uri)
{
    SeahorseSource *ks = NULL;
    gboolean found = FALSE;
    gchar *ks_uri;
    GSList *l;
    
    g_return_val_if_fail (uri && *uri, NULL);

    if (!sctx)
        sctx = seahorse_context_instance ();
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


/**
* sobj: SeahorseObject, sending the signal
* spec: ignored
* sctx: The instance to emit the signal on (#SeahorseContext)
*
* Emits a changed signal.
*
**/
static void
object_notify (SeahorseObject *sobj, GParamSpec *spec, SeahorseContext *sctx)
{
	g_signal_emit (sctx, signals[CHANGED], 0, sobj);
}

/**
* sksrc: a #SeahorseSource, part of the hash
* id: an id, part of the hash
*
* Creates a hash out of @sksrc and @id
*
* Returns an int stored in a pointer, representing the hash
**/
static gpointer                 
hashkey_by_source (SeahorseSource *sksrc, GQuark id)

{
    return GINT_TO_POINTER (g_direct_hash (sksrc) ^ 
                            g_str_hash (g_quark_to_string (id)));
}

/**
* a: the first #SeahorseObject
* b: the second #SeahorseObject
*
* Compares the locations of the two objects
*
* Returns 0 if a==b, -1 or 1 on difference
**/
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

/**
* sctx: The seahorse context to manipulate
* sobj: The object to add/remove
* add: TRUE if the object should be added
*
* Either adds or removes an object from sctx->pv->objects_by_type.
* The new list will be sorted by location.
*
**/
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

/**
 * seahorse_context_add_object:
 * @sctx: The context to add the object to
 * @sobj: The object to add
 *
 * Adds @sobj to @sctx. References @sobj
 *
 */
void
seahorse_context_add_object (SeahorseContext *sctx, SeahorseObject *sobj)
{
    if (!sctx)
        sctx = seahorse_context_instance ();
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));

    g_object_ref (sobj);
    seahorse_context_take_object (sctx, sobj);
}

/**
 * seahorse_context_take_object:
 * @sctx: The #SeahorseContext context to add an object to
 * @sobj: The #SeahorseObject object to add
 *
 * Adds @sobj to @sctx. If a similar object exists, it will be overwritten.
 * Emits the "added" signal.
 */
void
seahorse_context_take_object (SeahorseContext *sctx, SeahorseObject *sobj)
{
    gpointer ks;
    
    if (!sctx)
        sctx = seahorse_context_instance ();
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

/**
 * seahorse_context_get_count:
 * @sctx: The context. If NULL is passed it will take the application context
 *
 *
 *
 * Returns: The number of objects in this context
 */
guint
seahorse_context_get_count (SeahorseContext *sctx)
{
    if (!sctx)
        sctx = seahorse_context_instance ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), 0);
    return g_hash_table_size (sctx->pv->objects_by_source);
}

/**
 * seahorse_context_get_object:
 * @sctx: The #SeahorseContext to look in
 * @sksrc: The source to match
 * @id: the id to match
 *
 * Finds the object with the source @sksrc and @id in the context and returns it
 *
 * Returns: The matching object
 */
SeahorseObject*        
seahorse_context_get_object (SeahorseContext *sctx, SeahorseSource *sksrc,
                             GQuark id)
{
    gconstpointer k;
    
    if (!sctx)
        sctx = seahorse_context_instance ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);
    g_return_val_if_fail (SEAHORSE_IS_SOURCE (sksrc), NULL);
    
    k = hashkey_by_source (sksrc, id);
    return SEAHORSE_OBJECT (g_hash_table_lookup (sctx->pv->objects_by_source, k));
}

/**
 * ObjectMatcher:
 * @kp: A #SeahorseObjectPredicate to compare an object to
 * @many: TRUE if there can be several matches
 * @func: A function to call for every match
 * @user_data: user data to pass to the function
 *
 *
 **/
typedef struct _ObjectMatcher {
	SeahorseObjectPredicate *kp;
	gboolean many;
	SeahorseObjectFunc func;
	gpointer user_data;
} ObjectMatcher;

/**
 * find_matching_objects:
 * @key: ignored
 * @sobj: the object to match
 * @km: an #ObjectMatcher
 *
 * Calls km->func for every matched object
 *
 * Returns: TRUE if the search terminates, FALSE if there could be another
 * match or nothing was found
 */
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

/**
* object: A #SeahorseObject to add to the list
* user_data: a #GList to add the object to
*
* Does not return anything....
*
**/
static void
add_object_to_list (SeahorseObject *object, gpointer user_data)
{
	GList** list = (GList**)user_data;
	*list = g_list_prepend (*list, object);
}

/**
 * seahorse_context_find_objects_full:
 * @self: The #SeahorseContext to match objects in
 * @pred: #SeahorseObjectPredicate containing some data to match
 *
 * Finds matching objects and adds them to the list
 *
 * Returns: a #GList list containing the matching objects
 */
GList*
seahorse_context_find_objects_full (SeahorseContext *self, SeahorseObjectPredicate *pred)
{
	GList *list = NULL;
	ObjectMatcher km;

	if (!self)
		self = seahorse_context_instance ();
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

/**
 * seahorse_context_for_objects_full:
 * @self: #SeahorseContext to work with
 * @pred: #SeahorseObjectPredicate to match
 * @func: Function to call for matching objects
 * @user_data: Data to pass to this function
 *
 * Calls @func for every object in @self matching the criteria in @pred. @user_data
 * is passed to this function
 */
void
seahorse_context_for_objects_full (SeahorseContext *self, SeahorseObjectPredicate *pred,
                                   SeahorseObjectFunc func, gpointer user_data)
{
	ObjectMatcher km;

	if (!self)
		self = seahorse_context_instance ();
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

/**
 * seahorse_context_get_objects:
 * @self: A #SeahorseContext to find objects in
 * @source: A #SeahorseSource that must match
 *
 *
 *
 * Returns: A #GList of objects from @self that match the source @source
 */
GList*
seahorse_context_get_objects (SeahorseContext *self, SeahorseSource *source)
{
	SeahorseObjectPredicate pred;

	if (!self)
		self = seahorse_context_instance ();
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (self), NULL);
	g_return_val_if_fail (source == NULL || SEAHORSE_IS_SOURCE (source), NULL);

	seahorse_object_predicate_clear (&pred);
	pred.source = source;
	
	return seahorse_context_find_objects_full (self, &pred);
}

/**
 * seahorse_context_find_object:
 * @sctx: The #SeahorseContext to work with (can be NULL)
 * @id: The id to look for
 * @location: The location to look for (at least)
 *
 * Finds the object with the id @id at location @location or better.
 * Local is better than remote...
 *
 * Returns: the matching #SeahorseObject or NULL if none is found
 */
SeahorseObject*        
seahorse_context_find_object (SeahorseContext *sctx, GQuark id, SeahorseLocation location)
{
    SeahorseObject *sobj; 

    if (!sctx)
        sctx = seahorse_context_instance ();
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


/**
 * seahorse_context_find_objects:
 * @sctx: A #SeahorseContext to look in (can be NULL)
 * @ktype: The tag to look for
 * @usage: the usage (#SeahorseUsage)
 * @location: the location to look for
 *
 *
 *
 * Returns: A list of matching objects
 */
GList*
seahorse_context_find_objects (SeahorseContext *sctx, GQuark ktype, 
                               SeahorseUsage usage, SeahorseLocation location)
{
    SeahorseObjectPredicate pred;
    memset (&pred, 0, sizeof (pred));

    if (!sctx)
        sctx = seahorse_context_instance ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);
    
    pred.tag = ktype;
    pred.usage = usage;
    pred.location = location;
    
    return seahorse_context_find_objects_full (sctx, &pred);
}

/**
 * seahorse_context_remove_object:
 * @sctx: The #SeahorseContext (can be NULL)
 * @sobj: The #SeahorseObject to remove
 *
 * Removes the object from the context
 *
 */
void 
seahorse_context_remove_object (SeahorseContext *sctx, SeahorseObject *sobj)
{
    gconstpointer k;
    
    if (!sctx)
        sctx = seahorse_context_instance ();
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
    g_return_if_fail (SEAHORSE_IS_OBJECT (sobj));
    g_return_if_fail (seahorse_object_get_id (sobj) != 0);
    
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
 * seahorse_context_get_default_key:
 * @sctx: Current #SeahorseContext
 *
 * Returns: the secret key that's the default key
 *
 * Deprecated: No replacement
 */
SeahorseObject*
seahorse_context_get_default_key (SeahorseContext *sctx)
{
    SeahorseObject *sobj = NULL;
    gchar *id;
    
    if (!sctx)
        sctx = seahorse_context_instance ();
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
 * seahorse_context_get_discovery:
 * @sctx: #SeahorseContext object
 *
 * Gets the Service Discovery object for this context.
 *
 * Returns: The Service Discovery object.
 *
 * Deprecated: No replacement
 */
SeahorseServiceDiscovery*
seahorse_context_get_discovery (SeahorseContext *sctx)
{
    if (!sctx)
        sctx = seahorse_context_instance ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);
    g_return_val_if_fail (sctx->pv->discovery != NULL, NULL);
    
    return sctx->pv->discovery;
}

/**
 * seahorse_context_refresh_auto:
 * @sctx: A #SeahorseContext (can be NULL)
 *
 * Starts a new refresh operation and emits the "refreshing" signal
 *
 */
void
seahorse_context_refresh_auto (SeahorseContext *sctx)
{
	SeahorseSource *ks;
	SeahorseOperation *op = NULL;
	GSList *l;
    
	if (!sctx)
		sctx = seahorse_context_instance ();
	g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
	
	if (!sctx->pv->refresh_ops)
		sctx->pv->refresh_ops = seahorse_multi_operation_new ();

	for (l = sctx->pv->sources; l; l = g_slist_next (l)) {
		ks = SEAHORSE_SOURCE (l->data);
        
		if (seahorse_source_get_location (ks) == SEAHORSE_LOCATION_LOCAL) {

			op = seahorse_source_load (ks);
			g_return_if_fail (op);
			seahorse_multi_operation_take (sctx->pv->refresh_ops, op);
		}
		
	}
	
	g_signal_emit (sctx, signals[REFRESHING], 0, sctx->pv->refresh_ops);
}

/**
 * seahorse_context_search_remote:
 * @sctx: A #SeahorseContext (can be NULL)
 * @search: a keyword (name, email address...) to search for
 *
 * Searches for the key matching @search o the remote servers
 *
 * Returns: The created search operation
 */
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
        sctx = seahorse_context_instance ();
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
/**
* uri: the uri of the source
* sksrc: the source to add or replace
* ht: the hash table to modify
*
* Adds the @sksrc to the hash table @ht
*
**/
static void 
auto_source_to_hash (const gchar *uri, SeahorseSource *sksrc, GHashTable *ht)

{
    g_hash_table_replace (ht, (gpointer)uri, sksrc);
}

/**
* uri: The uri of this source
* sksrc: The source to remove
* sctx: The Context to remove data from
*
*
*
**/
static void
auto_source_remove (const gchar* uri, SeahorseSource *sksrc, SeahorseContext *sctx)
{
    seahorse_context_remove_source (sctx, sksrc);
    g_hash_table_remove (sctx->pv->auto_sources, uri);
}
#endif 
#endif

/**
* client: ignored
* id: ignored
* entry: used for validation only
* sctx: The context to work with
*
* Refreshes the sources generated from the keyservers
*
**/
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

/**
 * seahorse_context_transfer_objects:
 * @sctx: The #SeahorseContext (can be NULL)
 * @objects: the objects to import
 * @to: a source to import to (can be NULL)
 *
 *
 *
 * Returns: A transfer operation
 */
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
        sctx = seahorse_context_instance ();
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
        ktype = seahorse_source_get_tag (from);
        
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
        if (ktype != seahorse_source_get_tag (to)) {
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

/**
 * seahorse_context_retrieve_objects:
 * @sctx: A #SeahorsecContext
 * @ktype: The type of the keys to transfer
 * @ids: The key ids to transfer
 * @to: A #SeahorseSource. If NULL, it will use @ktype to find a source
 *
 * Copies remote objects to a local source
 *
 * Returns: A #SeahorseOperation
 */
SeahorseOperation*
seahorse_context_retrieve_objects (SeahorseContext *sctx, GQuark ktype, 
                                   GSList *ids, SeahorseSource *to)
{
    SeahorseMultiOperation *mop = NULL;
    SeahorseOperation *op = NULL;
    SeahorseSource *sksrc;
    GSList *sources, *l;
    
    if (!sctx)
        sctx = seahorse_context_instance ();
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


/**
 * seahorse_context_discover_objects:
 * @sctx: the context to work with (can be NULL)
 * @ktype: the type of key to discover
 * @rawids: a list of ids to discover
 *
 * Downloads a list of keys from the keyserver
 *
 * Returns: The imported keys
 */
GList*
seahorse_context_discover_objects (SeahorseContext *sctx, GQuark ktype, 
                                   GSList *rawids)
{
    SeahorseOperation *op = NULL;
    GList *robjects = NULL;
    GQuark id = 0;
    GSList *todiscover = NULL;
    GList *toimport = NULL;
    SeahorseSource *sksrc;
    SeahorseObject* sobj;
    SeahorseLocation loc;
    GSList *l;

    if (!sctx)
        sctx = seahorse_context_instance ();
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
        
        /* Running operations ref themselves */
        g_object_unref (op);
    }

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

    return robjects;
}

/**
 * seahorse_context_canonize_id:
 * @ktype: a keytype defining the canonization function
 * @id: The id to canonize
 *
 *
 *
 * Returns: A canonized GQuark
 */
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
