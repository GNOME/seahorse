/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
 * 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *  
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */
   
#include "seahorse-cleanup.h"
#include "seahorse-registry.h"

typedef struct _SeahorseRegistryPrivate SeahorseRegistryPrivate;

struct _SeahorseRegistryPrivate {
	GHashTable *types;
	GHashTable *objects;
	GHashTable *functions;
};

#define SEAHORSE_REGISTRY_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE((o), SEAHORSE_TYPE_REGISTRY, SeahorseRegistryPrivate))

G_DEFINE_TYPE(SeahorseRegistry, seahorse_registry, G_TYPE_OBJECT);

static SeahorseRegistry *registry_singleton = NULL; 

#define NO_VALUE (GUINT_TO_POINTER(1))

/* -----------------------------------------------------------------------------
 * HELPERS
 */

static void
cleanup_registry (void *unused)
{
	g_assert (registry_singleton);
	g_object_unref (registry_singleton);
	registry_singleton = NULL;
}

static void
keys_to_list (gpointer key, gpointer value, gpointer user_data)
{
	GList** list = user_data;
	*list = g_list_prepend (*list, key);
}

static void
register_value_for_category (GHashTable *table, GQuark category, 
                             gpointer value, GDestroyNotify destroy_func)
{
	GHashTable *set;
	
	g_return_if_fail (table);
	g_return_if_fail (category);
	
	set = g_hash_table_lookup (table, GUINT_TO_POINTER (category));
	if (!set) {
		set = g_hash_table_new_full (g_direct_hash, g_direct_equal, destroy_func, NULL);
		g_hash_table_replace (table, GUINT_TO_POINTER (category), set);
	}
	
	g_hash_table_replace (set, value, NO_VALUE);
}

static GList*
lookup_category_values (GHashTable *table, const gchar *category, va_list cats)
{
	GList *l, *value, *values = NULL;
	GHashTable *set;

	g_return_val_if_fail (table, NULL);
	g_return_val_if_fail (category, NULL);
	g_return_val_if_fail (category[0], NULL);
	
	/* Get the first category */
	set = g_hash_table_lookup (table, g_quark_try_string (category));
	if (!set)
		return NULL;
	
	/* Transfer all of that into the list */
	g_hash_table_foreach (set, keys_to_list, &values);
	
	/* 
	 * Go through the other values and remove any in results
	 * which we don't find.
	 */
	for (;;) {
		category = va_arg (cats, const gchar*);
		if (!category)
			break;
		g_return_val_if_fail (category[0], NULL);
		
		/* Lookup this category */
		set = g_hash_table_lookup (table, g_quark_try_string (category));
		
		/* No category, no matches */
		if (!set) {
			g_list_free (values);
			return NULL;
		}
		
		/* Go through each item in list and make sure it exists in this cat */
		for (l = values; l; l = l ? g_list_next (l) : values) {
			value = l;
			if (!g_hash_table_lookup (set, values->data)) {
				l = g_list_previous (l);
				values = g_list_delete_link (values, value);
			}
		}
	}
	
	return values;
}

static gpointer
lookup_category_value (GHashTable *table, const gchar *category, va_list cats)
{
	gpointer value = NULL;
	GList *values;
	
	values = lookup_category_values (table, category, cats);
	if (values)
		value = values->data;
	g_list_free (values);
	return value;
}

static gint
object_has_type (gconstpointer object, gconstpointer type)
{
	if (G_OBJECT_TYPE (object) == GPOINTER_TO_UINT (type))
		return 0;
	return -1;
}

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static void
seahorse_registry_init (SeahorseRegistry *obj)
{
	SeahorseRegistryPrivate *pv = SEAHORSE_REGISTRY_GET_PRIVATE (obj);
	pv->types = g_hash_table_new_full (g_direct_hash, g_direct_equal, 
	                                   NULL, (GDestroyNotify)g_hash_table_destroy);
	pv->objects = g_hash_table_new_full (g_direct_hash, g_direct_equal, 
	                                     NULL, (GDestroyNotify)g_hash_table_destroy);
	pv->functions = g_hash_table_new_full (g_direct_hash, g_direct_equal, 
	                                       NULL, (GDestroyNotify)g_hash_table_destroy);
}

static void
seahorse_registry_dispose (GObject *obj)
{
	SeahorseRegistryPrivate *pv = SEAHORSE_REGISTRY_GET_PRIVATE (obj);
	g_hash_table_remove_all (pv->types);
	G_OBJECT_CLASS (seahorse_registry_parent_class)->dispose (obj);
}

static void
seahorse_registry_finalize (GObject *obj)
{
	SeahorseRegistryPrivate *pv = SEAHORSE_REGISTRY_GET_PRIVATE (obj);
	g_hash_table_destroy (pv->types);
	G_OBJECT_CLASS (seahorse_registry_parent_class)->finalize (obj);
}

static void
seahorse_registry_class_init (SeahorseRegistryClass *klass)
{
	GObjectClass *gobject_class;
	gobject_class = (GObjectClass*)klass;

	seahorse_registry_parent_class = g_type_class_peek_parent (klass);
	gobject_class->dispose = seahorse_registry_dispose;
	gobject_class->finalize = seahorse_registry_finalize;

	g_type_class_add_private (gobject_class, sizeof (SeahorseRegistryPrivate));
}

/* -------------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseRegistry*
seahorse_registry_get (void)
{
	if (!registry_singleton) {
		registry_singleton = g_object_new (SEAHORSE_TYPE_REGISTRY, NULL);
		seahorse_cleanup_register (cleanup_registry, NULL);
	}
	
	return registry_singleton;
}

void
seahorse_registry_register_type (SeahorseRegistry *registry, GType type, 
                                 const gchar *category, ...)
{
	SeahorseRegistryPrivate *pv;
	va_list cats;

	if (!registry)
		registry = seahorse_registry_get ();
	
	g_return_if_fail (SEAHORSE_IS_REGISTRY (registry));
	g_return_if_fail (type);
	g_return_if_fail (category);

	pv = SEAHORSE_REGISTRY_GET_PRIVATE (registry);
	register_value_for_category (pv->types, g_quark_from_string (category), GUINT_TO_POINTER (type), NULL);
	
	va_start (cats, category);
	for (;;) {
		category = va_arg (cats, const gchar*);
		if (!category)
			break;
		register_value_for_category (pv->types, g_quark_from_string (category), GUINT_TO_POINTER (type), NULL);
	}
	va_end (cats);
}

void
seahorse_registry_register_object (SeahorseRegistry *registry, GObject *object, 
                                   const gchar *category, ...)
{
	SeahorseRegistryPrivate *pv;
	va_list cats;

	if (!registry)
		registry = seahorse_registry_get ();
	
	g_return_if_fail (SEAHORSE_IS_REGISTRY (registry));
	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (category);

	pv = SEAHORSE_REGISTRY_GET_PRIVATE (registry);
	register_value_for_category (pv->objects, g_quark_from_string (category), g_object_ref (object), g_object_unref);
	
	va_start (cats, category);
	for (;;) {
		category = va_arg (cats, const gchar*);
		if (!category)
			break;
		register_value_for_category (pv->objects, g_quark_from_string (category), g_object_ref (object), g_object_unref);
	}
	va_end (cats);
}

void
seahorse_registry_register_function (SeahorseRegistry *registry, gpointer func, 
                                     const gchar *category, ...)
{
	SeahorseRegistryPrivate *pv;
	va_list cats;

	if (!registry)
		registry = seahorse_registry_get ();
	
	g_return_if_fail (SEAHORSE_IS_REGISTRY (registry));
	g_return_if_fail (func);
	g_return_if_fail (category);

	pv = SEAHORSE_REGISTRY_GET_PRIVATE (registry);
	register_value_for_category (pv->functions, g_quark_from_string (category), func, NULL);
	
	va_start (cats, category);
	for (;;) {
		category = va_arg (cats, const gchar*);
		if (!category)
			break;
		register_value_for_category (pv->functions, g_quark_from_string (category), func, NULL);
	}
	va_end (cats);
}

GType
seahorse_registry_object_type (SeahorseRegistry *registry, const gchar *category, ...)
{
	SeahorseRegistryPrivate *pv;
	va_list cats;
	GType type;
	
	if (!registry)
		registry = seahorse_registry_get ();
	
	g_return_val_if_fail (SEAHORSE_IS_REGISTRY (registry), 0);
	pv = SEAHORSE_REGISTRY_GET_PRIVATE (registry);

	va_start (cats, category);
	type = GPOINTER_TO_UINT (lookup_category_value (pv->types, category, cats));
	va_end (cats);

	return type;
}

GList*
seahorse_registry_object_types (SeahorseRegistry *registry, const gchar *category, ...)
{
	SeahorseRegistryPrivate *pv;
	va_list cats;
	GList *types;
	
	if (!registry)
		registry = seahorse_registry_get ();
	
	g_return_val_if_fail (SEAHORSE_IS_REGISTRY (registry), NULL);
	pv = SEAHORSE_REGISTRY_GET_PRIVATE (registry);

	va_start (cats, category);
	types = lookup_category_values (pv->types, category, cats);
	va_end (cats);
	
	return types;
}

GObject*
seahorse_registry_object_instance (SeahorseRegistry *registry, const gchar *category, ...)
{
	SeahorseRegistryPrivate *pv;
	va_list cats;
	GObject *object;
	GType type;
	
	if (!registry)
		registry = seahorse_registry_get ();
	
	g_return_val_if_fail (SEAHORSE_IS_REGISTRY (registry), 0);
	pv = SEAHORSE_REGISTRY_GET_PRIVATE (registry);

	va_start (cats, category);
	object = lookup_category_value (pv->objects, category, cats);
	type = GPOINTER_TO_UINT (lookup_category_value (pv->types, category, cats));
	va_end (cats);

	if (object) 
		return g_object_ref (object);

	if (!type)
		return NULL;
	
	return g_object_new (type, NULL);
}

GList*
seahorse_registry_object_instances (SeahorseRegistry *registry, const gchar *category, ...)
{
	SeahorseRegistryPrivate *pv;
	va_list cats;
	GList *objects;
	GList *types, *l;
	
	if (!registry)
		registry = seahorse_registry_get ();
	
	g_return_val_if_fail (SEAHORSE_IS_REGISTRY (registry), 0);
	pv = SEAHORSE_REGISTRY_GET_PRIVATE (registry);

	va_start (cats, category);
	objects = lookup_category_values (pv->objects, category, cats);
	types = lookup_category_values (pv->types, category, cats);
	va_end (cats);

	for (l = objects; l; l = g_list_next (l))
		g_object_ref (l->data);
	
	/* Instantiate other types if nothing of that type exists */
	for (l = types; l; l = g_list_next (l)) {
		if (!g_list_find_custom (objects, l->data, object_has_type))
			objects = g_list_prepend (objects, g_object_new (GPOINTER_TO_UINT (l->data), NULL));
	}
	
	g_list_free (types);
	return objects;
}

gpointer
seahorse_registry_lookup_function (SeahorseRegistry *registry, const gchar *category, ...)
{
	SeahorseRegistryPrivate *pv;
	va_list cats;
	gpointer func;
	
	if (!registry)
		registry = seahorse_registry_get ();
	
	g_return_val_if_fail (SEAHORSE_IS_REGISTRY (registry), 0);
	pv = SEAHORSE_REGISTRY_GET_PRIVATE (registry);

	va_start (cats, category);
	func = GPOINTER_TO_UINT (lookup_category_value (pv->functions, category, cats));
	va_end (cats);

	return func;
}