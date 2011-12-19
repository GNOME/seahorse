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

#include "config.h"

#include "seahorse-collection.h"
#include "seahorse-marshal.h"

enum {
	PROP_0,
	PROP_BASE,
	PROP_PREDICATE
};

struct _SeahorseCollectionPrivate {
	GcrCollection *base;
	GHashTable *objects;
	SeahorsePredicate *pred;
	GDestroyNotify destroy_func;
};

static void      seahorse_collection_iface_init     (GcrCollectionIface *iface);

static gboolean  maybe_add_object                   (SeahorseCollection *self,
                                                     GObject *obj);

static gboolean  maybe_remove_object                (SeahorseCollection *self,
                                                     GObject *obj);

G_DEFINE_TYPE_WITH_CODE (SeahorseCollection, seahorse_collection, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_COLLECTION, seahorse_collection_iface_init);
);

static gboolean
remove_update (GObject *object,
               gpointer unused,
               SeahorseCollection *self)
{
	gcr_collection_emit_removed (GCR_COLLECTION (self), object);
	return TRUE;
}


static void
on_object_changed (GObject *obj,
                   GParamSpec *spec,
                   gpointer user_data)
{
	SeahorseCollection *self = SEAHORSE_COLLECTION (user_data);
	if (g_hash_table_lookup (self->pv->objects, obj))
		maybe_remove_object (self, obj);
	else
		maybe_add_object (self, obj);
}

static void
remove_object (SeahorseCollection *self,
               GObject *object)
{
	g_hash_table_remove (self->pv->objects, object);
	remove_update (object, NULL, self);
}

static gboolean
maybe_add_object (SeahorseCollection *self,
                  GObject *obj)
{
	if (g_hash_table_lookup (self->pv->objects, obj))
		return FALSE;

	if (!self->pv->pred || !seahorse_predicate_match (self->pv->pred, obj))
		return FALSE;

	g_hash_table_replace (self->pv->objects, obj, GINT_TO_POINTER (TRUE));
	gcr_collection_emit_added (GCR_COLLECTION (self), obj);
	return TRUE;
}

static gboolean
maybe_remove_object (SeahorseCollection *self,
                     GObject *obj)
{
	if (!g_hash_table_lookup (self->pv->objects, obj))
		return FALSE;

	if (self->pv->pred && seahorse_predicate_match (self->pv->pred, obj))
		return FALSE;

	remove_object (self, obj);
	return TRUE;
}

static void
on_base_added (GcrCollection *base,
               GObject *obj,
               gpointer user_data)
{
	SeahorseCollection *self = SEAHORSE_COLLECTION (user_data);

	g_signal_connect (obj, "notify", G_CALLBACK (on_object_changed), self);
	maybe_add_object (self, obj);
}

static void
on_base_removed (GcrCollection *base,
                 GObject *object,
                 gpointer user_data)
{
	SeahorseCollection *self = SEAHORSE_COLLECTION (user_data);

	g_signal_handlers_disconnect_by_func (object, on_object_changed, self);

	if (g_hash_table_lookup (self->pv->objects, object))
		remove_object (self, object);
}

static void
objects_to_list (GObject *obj, gpointer *c, GList **l)
{
	*l = g_list_append (*l, obj);
}

static void
objects_to_hash (GObject *obj, gpointer *c, GHashTable *ht)
{
	g_hash_table_replace (ht, obj, NULL);
}

static void
seahorse_collection_init (SeahorseCollection *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_COLLECTION,
	                                        SeahorseCollectionPrivate);
	self->pv->objects = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
seahorse_collection_constructed (GObject *obj)
{
	SeahorseCollection *self = SEAHORSE_COLLECTION (obj);

	g_return_if_fail (self->pv->base);

	G_OBJECT_CLASS (seahorse_collection_parent_class)->constructed (obj);

	g_signal_connect (self->pv->base, "added", G_CALLBACK (on_base_added), self);
	g_signal_connect (self->pv->base, "removed", G_CALLBACK (on_base_removed), self);
}

static void
seahorse_collection_dispose (GObject *obj)
{
	SeahorseCollection *self = SEAHORSE_COLLECTION (obj);

	g_signal_handlers_disconnect_by_func (self->pv->base, on_base_added, self);
	g_signal_handlers_disconnect_by_func (self->pv->base, on_base_removed, self);

	/* Release all our pointers and stuff */
	g_hash_table_foreach_remove (self->pv->objects, (GHRFunc)remove_update, self);

	G_OBJECT_CLASS (seahorse_collection_parent_class)->dispose (obj);
}

static void
seahorse_collection_finalize (GObject *obj)
{
	SeahorseCollection *self = SEAHORSE_COLLECTION (obj);

	g_clear_object (&self->pv->base);
	g_hash_table_destroy (self->pv->objects);

	if (self->pv->destroy_func)
		(self->pv->destroy_func) (self->pv->pred);

	G_OBJECT_CLASS (seahorse_collection_parent_class)->finalize (obj);
}

static void
seahorse_collection_set_property (GObject *obj,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	SeahorseCollection *self = SEAHORSE_COLLECTION (obj);

	switch (prop_id) {
	case PROP_BASE:
		g_return_if_fail (self->pv->base == NULL);
		self->pv->base = g_value_dup_object (value);
		g_return_if_fail (self->pv->base != NULL);
		if (self->pv->pred)
			seahorse_collection_refresh (self);
		break;
	case PROP_PREDICATE:
		g_return_if_fail (self->pv->pred == NULL);
		self->pv->pred = g_value_get_pointer (value);
		g_return_if_fail (self->pv->pred != NULL);
		if (self->pv->base)
			seahorse_collection_refresh (self);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_collection_get_property (GObject *obj,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	SeahorseCollection *self = SEAHORSE_COLLECTION (obj);

	switch (prop_id) {
	case PROP_BASE:
		g_value_set_object (value, self->pv->base);
		break;
	case PROP_PREDICATE:
		g_value_set_pointer (value, self->pv->pred);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_collection_class_init (SeahorseCollectionClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->constructed = seahorse_collection_constructed;
	gobject_class->dispose = seahorse_collection_dispose;
	gobject_class->finalize = seahorse_collection_finalize;
	gobject_class->set_property = seahorse_collection_set_property;
	gobject_class->get_property = seahorse_collection_get_property;

	g_object_class_install_property (gobject_class, PROP_BASE,
	          g_param_spec_object ("base", "Base", "Base collection",
	                               GCR_TYPE_COLLECTION, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));


	g_object_class_install_property (gobject_class, PROP_PREDICATE,
	          g_param_spec_pointer ("predicate", "Predicate", "Predicate for matching objects into this set.",
	                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (klass, sizeof (SeahorseCollectionPrivate));
}

static guint
seahorse_collection_get_length (GcrCollection *collection)
{
	SeahorseCollection *self = SEAHORSE_COLLECTION (collection);
	return g_hash_table_size (self->pv->objects);
}

static GList *
seahorse_collection_get_objects (GcrCollection *collection)
{
	SeahorseCollection *self = SEAHORSE_COLLECTION (collection);
	GList *objs = NULL;

	g_hash_table_foreach (self->pv->objects, (GHFunc)objects_to_list, &objs);

	return objs;
}

static gboolean
seahorse_collection_contains (GcrCollection *collection,
                              GObject *object)
{
	SeahorseCollection *self = SEAHORSE_COLLECTION (collection);
	return g_hash_table_lookup (self->pv->objects, object) ? TRUE : FALSE;
}

static void
seahorse_collection_iface_init (GcrCollectionIface *iface)
{
	iface->get_length = seahorse_collection_get_length;
	iface->get_objects = seahorse_collection_get_objects;
	iface->contains = seahorse_collection_contains;
}

SeahorseCollection *
seahorse_collection_new_for_predicate (GcrCollection *base,
                                       SeahorsePredicate *pred,
                                       GDestroyNotify destroy_func)
{
	SeahorseCollection *collection;

	g_return_val_if_fail (GCR_IS_COLLECTION (base), NULL);

	collection = g_object_new (SEAHORSE_TYPE_COLLECTION,
	                           "base", base,
	                           "predicate", pred,
	                           NULL);

	collection->pv->destroy_func = destroy_func;
	return collection;
}

void
seahorse_collection_refresh (SeahorseCollection *self)
{
	GHashTable *check = g_hash_table_new (g_direct_hash, g_direct_equal);
	GList *l, *objects = NULL;
	GHashTableIter iter;
	GObject *obj;

	g_return_if_fail (SEAHORSE_IS_COLLECTION (self));

	/* Make note of all the objects we had prior to refresh */
	g_hash_table_foreach (self->pv->objects, (GHFunc)objects_to_hash, check);

	objects = gcr_collection_get_objects (self->pv->base);
	for (l = objects; l != NULL; l = g_list_next (l)) {

		/* Make note that we've seen this object */
		g_hash_table_remove (check, l->data);

		/* This will add to set */
		if (!maybe_remove_object (self, l->data)) {
			if (maybe_add_object (self, l->data))
				g_signal_connect (l->data, "notify", G_CALLBACK (on_object_changed), self);
		}
	}
	g_list_free (objects);

	g_hash_table_iter_init (&iter, check);
	while (g_hash_table_iter_next (&iter, (gpointer *)&obj, NULL)) {
		g_signal_handlers_disconnect_by_func (obj, on_object_changed, self);
		remove_object (self, obj);
	}

	g_hash_table_destroy (check);
}

SeahorsePredicate *
seahorse_collection_get_predicate (SeahorseCollection *self)
{
	g_return_val_if_fail (SEAHORSE_IS_COLLECTION (self), NULL);
	return self->pv->pred;
}
