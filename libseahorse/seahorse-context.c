/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2005, 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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
#include "seahorse-marshal.h"
#include "seahorse-progress.h"
#include "seahorse-servers.h"
#include "seahorse-transfer.h"
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
    GList *sources;                         /* Sources which add keys to this context */
    GHashTable *auto_sources;               /* Automatically added sources (keyservers) */
    GHashTable *objects_by_source;          /* See explanation above */
    GHashTable *objects_by_type;            /* See explanation above */
    SeahorseServiceDiscovery *discovery;    /* Adds sources from DNS-SD */
    gboolean in_destruction;                /* In destroy signal */
    GSettings *seahorse_settings;
    GSettings *crypto_pgp_settings;
};

static void seahorse_context_dispose    (GObject *gobject);
static void seahorse_context_finalize   (GObject *gobject);

#ifdef WITH_KEYSERVER

static void
on_settings_keyservers_changed (GSettings *settings, gchar *key, gpointer user_data)
{
#ifdef WITH_PGP
	SeahorseContext *self = SEAHORSE_CONTEXT (user_data);
	SeahorseServerSource *source;
	gchar **keyservers;
	GHashTable *check;
	const gchar *uri;
	GHashTableIter iter;
	guint i;

	if (!self->pv->auto_sources)
		return;

	/* Make a light copy of the auto_source table */
	check = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_iter_init (&iter, self->pv->auto_sources);
	while (g_hash_table_iter_next (&iter, (gpointer*)&uri, (gpointer*)&source))
		g_hash_table_replace (check, (gpointer)uri, source);

	/* Load and strip names from keyserver list */
	keyservers = seahorse_servers_get_uris ();

	for (i = 0; keyservers[i] != NULL; i++) {
		uri = keyservers[i];

		/* If we don't have a keysource then add it */
		if (!g_hash_table_lookup (self->pv->auto_sources, uri)) {
			source = seahorse_server_source_new (uri);
			if (source != NULL) {
				seahorse_context_take_source (self, SEAHORSE_SOURCE (source));
				g_hash_table_replace (self->pv->auto_sources, g_strdup (uri), source);
			}
		}

		/* Mark this one as present */
		g_hash_table_remove (check, uri);
	}

	/* Now remove any extras */
	g_hash_table_iter_init (&iter, check);
	while (g_hash_table_iter_next (&iter, (gpointer*)&uri, (gpointer*)&source)) {
		g_hash_table_remove (self->pv->auto_sources, uri);
		seahorse_context_remove_source (self, SEAHORSE_SOURCE (source));
	}

	g_hash_table_destroy (check);
	g_strfreev (keyservers);
#endif /* WITH_PGP */
}

#endif /* WITH_KEYSERVER */

static void
seahorse_context_constructed (GObject *obj)
{
	SeahorseContext *self = SEAHORSE_CONTEXT (obj);

	g_return_if_fail (app_context == NULL);

	G_OBJECT_CLASS(seahorse_context_parent_class)->constructed (obj);

	app_context = self;

	/* DNS-SD discovery */
	self->pv->discovery = seahorse_service_discovery_new ();

	/* Automatically added remote key sources */
	self->pv->auto_sources = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                                g_free, NULL);

	self->pv->seahorse_settings = g_settings_new ("org.gnome.seahorse");

#ifdef WITH_PGP
	/* This is installed by gnome-keyring */
	self->pv->crypto_pgp_settings = g_settings_new ("org.gnome.crypto.pgp");

#ifdef WITH_KEYSERVER
	g_signal_connect (self->pv->crypto_pgp_settings, "changed::keyservers",
	                  G_CALLBACK (on_settings_keyservers_changed), self);

	/* Initial loading */
	on_settings_keyservers_changed (self->pv->crypto_pgp_settings, "keyservers", self);
#endif

#endif
}
/**
* klass: The class to initialise
*
* Inits the #SeahorseContextClass. Adds the signals "added", "removed", "changed"
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
* user_data: GList ** to prepend to
*
* Prepends #value to #user_data
*
**/
static void
hash_to_ref_list (gpointer object, gpointer value, gpointer user_data)
{
	*((GList**)user_data) = g_list_prepend (*((GList**)user_data), g_object_ref (value));
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
    GList *objects, *l;
    
    sctx = SEAHORSE_CONTEXT (gobject);
    
    /* Release all the objects */
    objects = NULL;
    g_hash_table_foreach (sctx->pv->objects_by_source, hash_to_ref_list, &objects);
    for (l = objects; l; l = g_list_next (l)) {
            seahorse_context_remove_object (sctx, l->data);
            g_object_unref (G_OBJECT (l->data));
    }
    g_list_free (objects);

#ifdef WITH_KEYSERVER
	if (sctx->pv->crypto_pgp_settings) {
		g_signal_handlers_disconnect_by_func (sctx->pv->crypto_pgp_settings,
		                                      on_settings_keyservers_changed, sctx);
		g_clear_object (&sctx->pv->crypto_pgp_settings);
	}
#endif
	g_clear_object (&sctx->pv->seahorse_settings);

    /* Auto sources */
    if (sctx->pv->auto_sources) 
        g_hash_table_destroy (sctx->pv->auto_sources);
    sctx->pv->auto_sources = NULL;
        
    /* DNS-SD */    
    if (sctx->pv->discovery) 
        g_object_unref (sctx->pv->discovery);
    sctx->pv->discovery = NULL;
        
    /* Release all the sources */
    for (l = sctx->pv->sources; l; l = g_list_next (l))
        g_object_unref (SEAHORSE_SOURCE (l->data));
    g_list_free (sctx->pv->sources);
    sctx->pv->sources = NULL;

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
	g_object_new (SEAHORSE_TYPE_CONTEXT, NULL);
	g_return_if_fail (app_context != NULL);
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
	if (!g_list_find (sctx->pv->sources, sksrc)) {
		sctx->pv->sources = g_list_append (sctx->pv->sources, sksrc);
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

    if (!g_list_find (sctx->pv->sources, sksrc))
        return;

    /* Remove all objects from this source */    
    objects = seahorse_context_get_objects (sctx, sksrc);
    for (l = objects; l; l = g_list_next (l)) 
        seahorse_context_remove_object (sctx, SEAHORSE_OBJECT (l->data));
    
    /* Remove the source itself */
    sctx->pv->sources = g_list_remove (sctx->pv->sources, sksrc);
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
    GList *l;
    
    if (!sctx)
        sctx = seahorse_context_instance ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    for (l = sctx->pv->sources; l; l = g_list_next (l)) {
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
* Returns: A list of seahorse sources matching @ktype and @location as #GList. Must
*  be freed with #g_list_free
*/
GList*
seahorse_context_find_sources (SeahorseContext *sctx, GQuark ktype,
                               SeahorseLocation location)
{
    SeahorseSource *ks;
    GList *sources = NULL;
    GList *l;
    
    if (!sctx)
        sctx = seahorse_context_instance ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    for (l = sctx->pv->sources; l; l = g_list_next (l)) {
        ks = SEAHORSE_SOURCE (l->data);
        
        if (ktype != SEAHORSE_TAG_INVALID && 
            seahorse_source_get_tag (ks) != ktype)
            continue;
        
        if (location != SEAHORSE_LOCATION_INVALID && 
            seahorse_source_get_location (ks) != location)
            continue;
        
        sources = g_list_append (sources, ks);
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
    GList *l;
    
    g_return_val_if_fail (uri && *uri, NULL);

    if (!sctx)
        sctx = seahorse_context_instance ();
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);

    for (l = sctx->pv->sources; l; l = g_list_next (l)) {
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
 * Returns: the PGP secret key that's the default key
 *
 * Deprecated: No replacement
 */
SeahorseObject*
seahorse_context_get_default_key (SeahorseContext *self)
{
	SeahorseObject *key = NULL;
	gchar *keyid;

	if (self == NULL)
		self = seahorse_context_instance ();
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (self), NULL);

	if (self->pv->crypto_pgp_settings) {
		keyid = g_settings_get_string (self->pv->crypto_pgp_settings, "default-key");
		if (keyid != NULL && keyid[0]) {
			key = seahorse_context_find_object (self, g_quark_from_string (keyid),
			                                    SEAHORSE_LOCATION_LOCAL);
		}
		g_free (keyid);
	}

	return key;
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

static void
on_source_refresh_load_ready (GObject *object, GAsyncResult *result, gpointer user_data)
{
	GError *error = NULL;

	if (!seahorse_source_load_finish (SEAHORSE_SOURCE (object), result, &error)) {
		g_message ("failed to refresh: %s", error->message);
		g_clear_error (&error);
	}
}

/**
 * seahorse_context_refresh_auto:
 * @sctx: A #SeahorseContext (can be NULL)
 *
 * Starts a new refresh operation
 *
 */
void
seahorse_context_refresh_auto (SeahorseContext *sctx)
{
	SeahorseSource *ks;
	GList *l;

	if (!sctx)
		sctx = seahorse_context_instance ();
	g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));

	for (l = sctx->pv->sources; l; l = g_list_next (l)) {
		ks = SEAHORSE_SOURCE (l->data);
		if (seahorse_source_get_location (ks) == SEAHORSE_LOCATION_LOCAL)
			seahorse_source_load_async (ks, NULL,
			                            on_source_refresh_load_ready, NULL);
	}
}

typedef struct {
	GCancellable *cancellable;
	gint num_searches;
	GList *objects;
} seahorse_context_search_remote_closure;

static void
seahorse_context_search_remote_free (gpointer user_data)
{
	seahorse_context_search_remote_closure *closure = user_data;
	g_clear_object (&closure->cancellable);
	g_list_free (closure->objects);
	g_free (closure);
}

static void
on_source_search_ready (GObject *source,
                        GAsyncResult *result,
                        gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	seahorse_context_search_remote_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;
	GList *objects;

	g_return_if_fail (closure->num_searches > 0);

	objects = seahorse_source_search_finish (SEAHORSE_SOURCE (source), result, &error);
	closure->objects = g_list_concat (closure->objects, objects);

	if (error != NULL)
		g_simple_async_result_take_error (res, error);

	closure->num_searches--;
	seahorse_progress_end (closure->cancellable, GINT_TO_POINTER (closure->num_searches));

	if (closure->num_searches == 0)
		g_simple_async_result_complete (res);

	g_object_unref (user_data);
}

void
seahorse_context_search_remote_async (SeahorseContext *self,
                                      const gchar *search,
                                      GCancellable *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
	seahorse_context_search_remote_closure *closure;
	GSimpleAsyncResult *res;
	SeahorseSource *source;
	GHashTable *servers = NULL;
	gchar **names;
	gchar *uri;
	GList *l;
	guint i;

	if (self == NULL)
		self = seahorse_context_instance ();

	g_return_if_fail (SEAHORSE_IS_CONTEXT (self));

	/* Get a list of all selected key servers */
	names = g_settings_get_strv (self->pv->seahorse_settings, "last-search-servers");
	if (names != NULL && names[0] != NULL) {
		servers = g_hash_table_new (g_str_hash, g_str_equal);
		for (i = 0; names[i] != NULL; i++)
			g_hash_table_insert (servers, names[i], GINT_TO_POINTER (TRUE));
		g_strfreev (names);
	}

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_context_search_remote_async);
	closure = g_new0 (seahorse_context_search_remote_closure, 1);
	g_simple_async_result_set_op_res_gpointer (res, closure,
	                                           seahorse_context_search_remote_free);
	if (cancellable)
		closure->cancellable = g_object_ref (cancellable);

	for (l = self->pv->sources; l; l = g_list_next (l)) {
		source = SEAHORSE_SOURCE (l->data);

		if (seahorse_source_get_location (source) != SEAHORSE_LOCATION_REMOTE)
			continue;

		if (servers) {
			g_object_get (source, "uri", &uri, NULL);
			if (!g_hash_table_lookup (servers, uri)) {
				g_free (uri);
				continue;
			}
			g_free (uri);
		}

		seahorse_progress_prep_and_begin (closure->cancellable, GINT_TO_POINTER (closure->num_searches), NULL);
		seahorse_source_search_async (source, search, closure->cancellable,
		                              on_source_search_ready, g_object_ref (res));
		closure->num_searches++;
	}

	if (closure->num_searches == 0)
		g_simple_async_result_complete_in_idle (res);

	g_object_unref (res);
}

GList *
seahorse_context_search_remote_finish (SeahorseContext *self,
                                       GAsyncResult *result,
                                       GError **error)
{
	seahorse_context_search_remote_closure *closure;
	GSimpleAsyncResult *res;
	GList *results;

	if (self == NULL)
		self = seahorse_context_instance ();
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (self), NULL);

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
	                      seahorse_context_search_remote_async), NULL);

	res = G_SIMPLE_ASYNC_RESULT (result);

	if (g_simple_async_result_propagate_error (res, error))
		return NULL;

	closure = g_simple_async_result_get_op_res_gpointer (res);
	results = closure->objects;
	closure->objects = NULL;

	return results;
}

typedef struct {
	GCancellable *cancellable;
	gint num_transfers;
} seahorse_context_transfer_closure;

static void
seahorse_context_transfer_free (gpointer user_data)
{
	seahorse_context_search_remote_closure *closure = user_data;
	g_clear_object (&closure->cancellable);
	g_free (closure);
}

static void
on_source_transfer_ready (GObject *source,
                          GAsyncResult *result,
                          gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	seahorse_context_transfer_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	g_return_if_fail (closure->num_transfers > 0);

	seahorse_transfer_finish (result, &error);
	if (error != NULL)
		g_simple_async_result_take_error (res, error);

	closure->num_transfers--;
	seahorse_progress_end (closure->cancellable, GINT_TO_POINTER (closure->num_transfers));

	if (closure->num_transfers == 0) {
		g_simple_async_result_complete (res);
	}

	g_object_unref (user_data);
}


void
seahorse_context_transfer_objects_async (SeahorseContext *self,
                                         GList *objects,
                                         SeahorseSource *to,
                                         GCancellable *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer user_data)
{
	seahorse_context_transfer_closure *closure;
	SeahorseObject *object;
	GSimpleAsyncResult *res;
	SeahorseSource *from;
	GQuark ktype;
	GList *next, *l;
	GList *ids = NULL;

	if (self == NULL)
		self = seahorse_context_instance ();
	g_return_if_fail (SEAHORSE_IS_CONTEXT (self));

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_context_transfer_objects_async);
	closure = g_new0 (seahorse_context_transfer_closure, 1);
	g_simple_async_result_set_op_res_gpointer (res, closure,
	                                           seahorse_context_transfer_free);
	if (cancellable)
		closure->cancellable = g_object_ref (cancellable);

	objects = g_list_copy (objects);

	/* Sort by key source */
	objects = seahorse_util_objects_sort (objects);

	while (objects) {

		/* break off one set (same keysource) */
		next = seahorse_util_objects_splice (objects);

		g_assert (SEAHORSE_IS_OBJECT (objects->data));
		object = SEAHORSE_OBJECT (objects->data);

		/* Export from this key source */
		from = seahorse_object_get_source (object);
		g_return_if_fail (from != NULL);
		ktype = seahorse_source_get_tag (from);

		/* Find a local keysource to import to */
		if (!to) {
			to = seahorse_context_find_source (self, ktype, SEAHORSE_LOCATION_LOCAL);
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

			/* Build id list */
			for (l = objects; l; l = g_list_next (l))
				ids = g_list_prepend (ids, GUINT_TO_POINTER (seahorse_object_get_id (l->data)));
			ids = g_list_reverse (ids);

			/* Start a new transfer operation between the two sources */
			seahorse_progress_prep_and_begin (cancellable, GINT_TO_POINTER (closure->num_transfers), NULL);
			seahorse_transfer_async (from, to, ids, cancellable,
			                         on_source_transfer_ready, g_object_ref (res));
			closure->num_transfers++;

			g_list_free (ids);
			ids = NULL;
		}

		g_list_free (objects);
		objects = next;
	}

	if (closure->num_transfers == 0)
		g_simple_async_result_complete_in_idle (res);

	g_object_unref (res);
}

gboolean
seahorse_context_transfer_objects_finish (SeahorseContext *self,
                                          GAsyncResult *result,
                                          GError **error)
{
	GSimpleAsyncResult *res;

	if (self == NULL)
		self = seahorse_context_instance ();
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (self), FALSE);

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
	                      seahorse_context_transfer_objects_async), FALSE);

	res = G_SIMPLE_ASYNC_RESULT (result);

	if (g_simple_async_result_propagate_error (res, error))
		return FALSE;

	return TRUE;
}

void
seahorse_context_retrieve_objects_async (SeahorseContext *self,
                                         GQuark ktype,
                                         GList *ids,
                                         SeahorseSource *to,
                                         GCancellable *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer user_data)
{
	seahorse_context_transfer_closure *closure;
	GSimpleAsyncResult *res;
	SeahorseSource *source;
	GList *sources, *l;

	if (self == NULL)
		self = seahorse_context_instance ();
	g_return_if_fail (SEAHORSE_IS_CONTEXT (self));

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_context_retrieve_objects_async);
	closure = g_new0 (seahorse_context_transfer_closure, 1);
	g_simple_async_result_set_op_res_gpointer (res, closure,
	                                           seahorse_context_transfer_free);
	if (cancellable)
		closure->cancellable = g_object_ref (cancellable);

	if (to == NULL) {
		to = seahorse_context_find_source (self, ktype, SEAHORSE_LOCATION_LOCAL);
		if (to == NULL) {
			/* TODO: How can we warn caller about this. Do we need to? */
			g_warning ("couldn't find a local source for: %s",
			           g_quark_to_string (ktype));
		}
	}

	sources = seahorse_context_find_sources (self, ktype, SEAHORSE_LOCATION_REMOTE);
	for (l = sources; to != NULL && l != NULL; l = g_list_next (l)) {
		source = SEAHORSE_SOURCE (l->data);

		/* Start a new transfer operation between the two key sources */
		seahorse_progress_prep_and_begin (cancellable, GINT_TO_POINTER (closure->num_transfers), NULL);
		seahorse_transfer_async (source, to, ids, cancellable,
		                         on_source_transfer_ready, g_object_ref (res));
		closure->num_transfers++;
	}

	if (closure->num_transfers == 0)
		g_simple_async_result_complete_in_idle (res);

	g_object_unref (res);
}


gboolean
seahorse_context_retrieve_objects_finish  (SeahorseContext *self,
                                           GAsyncResult *result,
                                           GError **error)
{
	GSimpleAsyncResult *res;

	if (self == NULL)
		self = seahorse_context_instance ();
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (self), FALSE);

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
	                      seahorse_context_retrieve_objects_async), FALSE);

	res = G_SIMPLE_ASYNC_RESULT (result);

	if (g_simple_async_result_propagate_error (res, error))
		return FALSE;

	return TRUE;
}

GList*
seahorse_context_discover_objects (SeahorseContext *self,
                                   GQuark ktype,
                                   GList *rawids,
                                   GCancellable *cancellable)
{
	GList *robjects = NULL;
	GQuark id = 0;
	GList *todiscover = NULL;
	GList *toimport = NULL;
	SeahorseSource *source;
	SeahorseObject* object;
	SeahorseLocation loc;
	GList *l;

	if (self == NULL)
		self = seahorse_context_instance ();
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (self), NULL);

	/* Check all the ids */
	for (l = rawids; l != NULL; l = g_list_next (l)) {

		id = seahorse_context_canonize_id (ktype, (gchar*)l->data);
		if (!id) {
			/* TODO: Try and match this partial id */
			g_warning ("invalid id: %s", (gchar*)l->data);
			continue;
		}

		/* Do we know about this object? */
		object = seahorse_context_find_object (self, id, SEAHORSE_LOCATION_INVALID);

		/* No such object anywhere, discover it */
		if (object == NULL) {
			todiscover = g_list_prepend (todiscover, GUINT_TO_POINTER (id));
			id = 0;
			continue;
		}

		/* Our return value */
		robjects = g_list_prepend (robjects, object);

		/* We know about this object, check where it is */
		loc = seahorse_object_get_location (object);
		g_assert (loc != SEAHORSE_LOCATION_INVALID);

		/* Do nothing for local objects */
		if (loc >= SEAHORSE_LOCATION_LOCAL)
			continue;

		/* Remote objects get imported */
		else if (loc >= SEAHORSE_LOCATION_REMOTE)
			toimport = g_list_prepend (toimport, object);

		/* Searching objects are ignored */
		else if (loc >= SEAHORSE_LOCATION_SEARCHING)
			continue;

		/* TODO: Should we try SEAHORSE_LOCATION_MISSING objects again? */
	}

	/* Start an import process on all toimport */
	if (toimport) {
		seahorse_context_transfer_objects_async (self, toimport, NULL,
		                                         cancellable, NULL, NULL);

		g_list_free (toimport);
	}

	/* Start a discover process on all todiscover */
	if (todiscover != NULL &&
	    g_settings_get_boolean (self->pv->seahorse_settings, "server-auto-retrieve")) {
		seahorse_context_retrieve_objects_async (self, ktype, todiscover, NULL,
		                                         cancellable, NULL, NULL);
	}

	/* Add unknown objects for all these */
	source = seahorse_context_find_source (self, ktype, SEAHORSE_LOCATION_MISSING);
	for (l = todiscover; l != NULL; l = g_list_next (l)) {
		if (source) {
			object = seahorse_unknown_source_add_object (SEAHORSE_UNKNOWN_SOURCE (source),
			                                             GPOINTER_TO_UINT (l->data),
			                                             cancellable);
			robjects = g_list_prepend (robjects, object);
		}
	}

	g_list_free (todiscover);

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

GSettings *
seahorse_context_settings (SeahorseContext *self)
{
	if (self == NULL)
		self = seahorse_context_instance ();
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (self), NULL);

	return self->pv->seahorse_settings;
}

GSettings *
seahorse_context_pgp_settings (SeahorseContext *self)
{
	if (self == NULL)
		self = seahorse_context_instance ();
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (self), NULL);

	return self->pv->crypto_pgp_settings;
}
