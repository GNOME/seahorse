/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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

#include "seahorse-set.h"
#include "seahorse-marshal.h"
#include "seahorse-gconf.h"

enum {
    PROP_0,
    PROP_PREDICATE
};

enum {
    ADDED,
    REMOVED,
    CHANGED,
    SET_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _SeahorseSetPrivate {
    GHashTable *objects;
    SeahorseObjectPredicate *pred;
};

G_DEFINE_TYPE (SeahorseSet, seahorse_set, G_TYPE_OBJECT);

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static gboolean
remove_update (SeahorseObject *sobj, gpointer closure, SeahorseSet *skset)
{
    if (closure == GINT_TO_POINTER (TRUE))
        closure = NULL;
    
    g_signal_emit (skset, signals[REMOVED], 0, sobj, closure);
    g_signal_emit (skset, signals[SET_CHANGED], 0);
    return TRUE;
}

static void
remove_object  (SeahorseObject *sobj, gpointer closure, SeahorseSet *skset)
{
    if (!closure)
        closure = g_hash_table_lookup (skset->pv->objects, sobj);
    
    g_hash_table_remove (skset->pv->objects, sobj);
    remove_update (sobj, closure, skset);
}

static gboolean
maybe_add_object (SeahorseSet *skset, SeahorseObject *sobj)
{
    if (!seahorse_context_owns_object (SCTX_APP (), sobj))
        return FALSE;

    if (g_hash_table_lookup (skset->pv->objects, sobj))
        return FALSE;
    
    if (!skset->pv->pred || !seahorse_object_predicate_match (skset->pv->pred, sobj))
        return FALSE;
    
    g_hash_table_replace (skset->pv->objects, sobj, GINT_TO_POINTER (TRUE));
    g_signal_emit (skset, signals[ADDED], 0, sobj);
    g_signal_emit (skset, signals[SET_CHANGED], 0);
    return TRUE;
}

static gboolean
maybe_remove_object (SeahorseSet *skset, SeahorseObject *sobj)
{
    if (!g_hash_table_lookup (skset->pv->objects, sobj))
        return FALSE;
    
    if (skset->pv->pred && seahorse_object_predicate_match (skset->pv->pred, sobj))
        return FALSE;
    
    remove_object (sobj, NULL, skset);
    return TRUE;
}

static void
object_added (SeahorseContext *sctx, SeahorseObject *sobj, SeahorseSet *skset)
{
    g_assert (SEAHORSE_IS_OBJECT (sobj));
    g_assert (SEAHORSE_IS_SET (skset));

    maybe_add_object (skset, sobj);
}

static void 
object_removed (SeahorseContext *sctx, SeahorseObject *sobj, SeahorseSet *skset)
{
    g_assert (SEAHORSE_IS_OBJECT (sobj));
    g_assert (SEAHORSE_IS_SET (skset));

    if (g_hash_table_lookup (skset->pv->objects, sobj))
        remove_object (sobj, NULL, skset);
}

static void
object_changed (SeahorseContext *sctx, SeahorseObject *sobj, SeahorseObjectChange change,
                SeahorseSet *skset)
{
    gpointer closure = g_hash_table_lookup (skset->pv->objects, sobj);

    g_assert (SEAHORSE_IS_OBJECT (sobj));
    g_assert (SEAHORSE_IS_SET (skset));

    if (closure) {
        
        /* See if needs to be removed, otherwise emit signal */
        if (!maybe_remove_object (skset, sobj)) {
            
            if (closure == GINT_TO_POINTER (TRUE))
                closure = NULL;
            
            g_signal_emit (skset, signals[CHANGED], 0, sobj, change, closure);
        }
        
    /* Not in our set yet */
    } else 
        maybe_add_object (skset, sobj);
}

static void
objects_to_list (SeahorseObject *sobj, gpointer *c, GList **l)
{
    *l = g_list_append (*l, sobj);
}

static void
objects_to_hash (SeahorseObject *sobj, gpointer *c, GHashTable *ht)
{
    g_hash_table_replace (ht, sobj, NULL);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_set_dispose (GObject *gobject)
{
    SeahorseSet *skset = SEAHORSE_SET (gobject);
    
    /* Release all our pointers and stuff */
    g_hash_table_foreach_remove (skset->pv->objects, (GHRFunc)remove_update, skset);
    skset->pv->pred = NULL;
    
    g_signal_handlers_disconnect_by_func (SCTX_APP (), object_added, skset);    
    g_signal_handlers_disconnect_by_func (SCTX_APP (), object_removed, skset);    
    g_signal_handlers_disconnect_by_func (SCTX_APP (), object_changed, skset);    
    
	G_OBJECT_CLASS (seahorse_set_parent_class)->dispose (gobject);
}

static void
seahorse_set_finalize (GObject *gobject)
{
    SeahorseSet *skset = SEAHORSE_SET (gobject);

    g_hash_table_destroy (skset->pv->objects);
    g_assert (skset->pv->pred == NULL);
    g_free (skset->pv);
    
	G_OBJECT_CLASS (seahorse_set_parent_class)->finalize (gobject);
}

static void
seahorse_set_set_property (GObject *object, guint prop_id, const GValue *value, 
                              GParamSpec *pspec)
{
	SeahorseSet *skset = SEAHORSE_SET (object);
	
    switch (prop_id) {
    case PROP_PREDICATE:
        skset->pv->pred = (SeahorseObjectPredicate*)g_value_get_pointer (value);        
        seahorse_set_refresh (skset);
        break;
    }
}

static void
seahorse_set_get_property (GObject *object, guint prop_id, GValue *value, 
                              GParamSpec *pspec)
{
    SeahorseSet *skset = SEAHORSE_SET (object);
	
    switch (prop_id) {
    case PROP_PREDICATE:
        g_value_set_pointer (value, skset->pv->pred);
        break;
    }
}

static void
seahorse_set_init (SeahorseSet *skset)
{
	/* init private vars */
	skset->pv = g_new0 (SeahorseSetPrivate, 1);

    skset->pv->objects = g_hash_table_new (g_direct_hash, g_direct_equal);
    
    g_signal_connect (SCTX_APP (), "added", G_CALLBACK (object_added), skset);
    g_signal_connect (SCTX_APP (), "removed", G_CALLBACK (object_removed), skset);
    g_signal_connect (SCTX_APP (), "changed", G_CALLBACK (object_changed), skset);
}

static void
seahorse_set_class_init (SeahorseSetClass *klass)
{
    GObjectClass *gobject_class;
    
    seahorse_set_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->dispose = seahorse_set_dispose;
    gobject_class->finalize = seahorse_set_finalize;
    gobject_class->set_property = seahorse_set_set_property;
    gobject_class->get_property = seahorse_set_get_property;
    
    g_object_class_install_property (gobject_class, PROP_PREDICATE,
        g_param_spec_pointer ("predicate", "Predicate", "Predicate for matching objects into this set.", 
                              G_PARAM_READWRITE));
    
    signals[ADDED] = g_signal_new ("added", SEAHORSE_TYPE_SET, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseSetClass, added),
                NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, SEAHORSE_TYPE_OBJECT);
    
    signals[REMOVED] = g_signal_new ("removed", SEAHORSE_TYPE_SET, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseSetClass, removed),
                NULL, NULL, seahorse_marshal_VOID__OBJECT_POINTER, G_TYPE_NONE, 2, SEAHORSE_TYPE_OBJECT, G_TYPE_POINTER);    
    
    signals[CHANGED] = g_signal_new ("changed", SEAHORSE_TYPE_SET, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseSetClass, changed),
                NULL, NULL, seahorse_marshal_VOID__OBJECT_UINT_POINTER, G_TYPE_NONE, 3, SEAHORSE_TYPE_OBJECT, G_TYPE_UINT, G_TYPE_POINTER);
                
    signals[SET_CHANGED] = g_signal_new ("set-changed", SEAHORSE_TYPE_SET,
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseSetClass, set_changed),
                NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseSet*
seahorse_set_new (GQuark ktype, SeahorseUsage usage, SeahorseLocation location,
                     guint flags, guint nflags)
{
    SeahorseSet *skset;
    SeahorseObjectPredicate *pred = g_new0(SeahorseObjectPredicate, 1);
    
    pred->location = location;
    pred->id = ktype;
    pred->usage = usage;
    pred->flags = flags;
    pred->nflags = nflags;
    
    skset = seahorse_set_new_full (pred);
    g_object_set_data_full (G_OBJECT (skset), "quick-predicate", pred, g_free);
    
    return skset;
}

SeahorseSet*     
seahorse_set_new_full (SeahorseObjectPredicate *pred)
{
    return g_object_new (SEAHORSE_TYPE_SET, "predicate", pred, NULL);
}

gboolean
seahorse_set_has_object (SeahorseSet *skset, SeahorseObject *sobj)
{
    if (g_hash_table_lookup (skset->pv->objects, sobj))
        return TRUE;
    
    /* 
     * This happens when the object has changed state, but we have 
     * not yet received the signal. 
     */
    if (maybe_add_object (skset, sobj))
        return TRUE;
    
    return FALSE;
}

GList*             
seahorse_set_get_objects (SeahorseSet *skset)
{
    GList *objs = NULL;
    g_hash_table_foreach (skset->pv->objects, (GHFunc)objects_to_list, &objs);
    return objs;
}

guint
seahorse_set_get_count (SeahorseSet *skset)
{
    return g_hash_table_size (skset->pv->objects);
}

void
seahorse_set_refresh (SeahorseSet *skset)
{
    GHashTable *check = g_hash_table_new (g_direct_hash, g_direct_equal);
    GList *l, *objects = NULL;
    SeahorseObject *sobj;
    
    /* Make note of all the objects we had prior to refresh */
    g_hash_table_foreach (skset->pv->objects, (GHFunc)objects_to_hash, check);
    
    if (skset->pv->pred)
        objects = seahorse_context_find_objects_full (SCTX_APP (), skset->pv->pred);
    
    for (l = objects; l; l = g_list_next (l)) {
        sobj = SEAHORSE_OBJECT (l->data);
        
        /* Make note that we've seen this object */
        g_hash_table_remove (check, sobj);

        /* This will add to set */
        maybe_add_object (skset, sobj);
    }
    
    g_list_free (objects);
    
    g_hash_table_foreach (check, (GHFunc)remove_object, skset);
    g_hash_table_destroy (check);
}

gpointer
seahorse_set_get_closure (SeahorseSet *skset, SeahorseObject *sobj)
{
    gpointer closure = g_hash_table_lookup (skset->pv->objects, sobj);
    g_return_val_if_fail (closure != NULL, NULL);

    /* |TRUE| means no closure has been set */
    if (closure == GINT_TO_POINTER (TRUE))
        return NULL;
    
    return closure;
}

void
seahorse_set_set_closure (SeahorseSet *skset, SeahorseObject *sobj, 
                             gpointer closure)
{
    /* Make sure we have the object */
    g_return_if_fail (g_hash_table_lookup (skset->pv->objects, sobj) != NULL);
    
    /* |TRUE| means no closure has been set */
    if (closure == NULL)
        closure = GINT_TO_POINTER (TRUE);

    g_hash_table_insert (skset->pv->objects, sobj, closure);    
}
