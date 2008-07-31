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
	GHashTable *categories;
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
register_type_for_category (SeahorseRegistry *registry, const gchar *category, GType type)
{
	SeahorseRegistryPrivate *pv = SEAHORSE_REGISTRY_GET_PRIVATE (registry);
	GHashTable *set;
	
	g_return_if_fail (SEAHORSE_IS_REGISTRY (registry));
	g_return_if_fail (category);
	g_return_if_fail (category[0]);
	
	set = g_hash_table_lookup (pv->categories, category);
	if (!set) {
		set = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);
		g_hash_table_replace (pv->categories, g_strdup (category), set);
	}
	
	g_hash_table_replace (set, GUINT_TO_POINTER (type), NO_VALUE);
}

static GList*
lookup_types (SeahorseRegistry *registry, const gchar *category, va_list cats)
{
	SeahorseRegistryPrivate *pv = SEAHORSE_REGISTRY_GET_PRIVATE (registry);
	GList *l, *type, *types = NULL;
	GHashTable *set;
	
	g_return_val_if_fail (SEAHORSE_IS_REGISTRY (registry), NULL);
	g_return_val_if_fail (category, NULL);
	g_return_val_if_fail (category[0], NULL);
	
	/* Get the first category */
	set = g_hash_table_lookup (pv->categories, category);
	if (!set)
		return NULL;
	
	/* Transfer all of that into the list */
	g_hash_table_foreach (set, keys_to_list, &types);
	
	/* 
	 * Go through the other categories and remove any in results
	 * which we don't find.
	 */
	for (;;) {
		category = va_arg (cats, const gchar*);
		if (!category)
			break;
		g_return_val_if_fail (category[0], NULL);
		
		/* Lookup this category */
		set = g_hash_table_lookup (pv->categories, category);
		
		/* No category, no matches */
		if (!set) {
			g_list_free (types);
			return NULL;
		}
		
		/* Go through each item in list and make sure it exists in this cat */
		for (l = types; l; l = l ? g_list_next (l) : types) {
			type = l;
			if (!g_hash_table_lookup (set, type->data)) {
				l = g_list_previous (l);
				types = g_list_delete_link (types, type);
			}
		}
	}
	
	return types;
}

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static void
seahorse_registry_init (SeahorseRegistry *obj)
{
	SeahorseRegistryPrivate *pv = SEAHORSE_REGISTRY_GET_PRIVATE (obj);
	pv->categories = g_hash_table_new_full (g_str_hash, g_str_equal, 
	                                        g_free, (GDestroyNotify)g_hash_table_destroy);
}

static void
seahorse_registry_dispose (GObject *obj)
{
	SeahorseRegistryPrivate *pv = SEAHORSE_REGISTRY_GET_PRIVATE (obj);
	g_hash_table_remove_all (pv->categories);
	G_OBJECT_CLASS (seahorse_registry_parent_class)->dispose (obj);
}

static void
seahorse_registry_finalize (GObject *obj)
{
	SeahorseRegistryPrivate *pv = SEAHORSE_REGISTRY_GET_PRIVATE (obj);
	g_hash_table_destroy (pv->categories);
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
	va_list cats;

	if (!registry)
		registry = seahorse_registry_get ();
	
	g_return_if_fail (SEAHORSE_IS_REGISTRY (registry));
	g_return_if_fail (type);
	g_return_if_fail (category);

	register_type_for_category (registry, category, type);
	
	va_start (cats, category);
	for (;;) {
		category = va_arg (cats, const gchar*);
		if (!category)
			break;
		register_type_for_category (registry, category, type);
	}
	va_end (cats);
}

GType
seahorse_registry_find_type (SeahorseRegistry *registry, const gchar *category, ...)
{
	va_list cats;
	GList *types;
	GType type;
	
	if (!registry)
		registry = seahorse_registry_get ();
	
	g_return_val_if_fail (SEAHORSE_IS_REGISTRY (registry), 0);

	va_start (cats, category);
	types = lookup_types (registry, category, cats);
	va_end (cats);

	type = 0;
	if (types)
		type = GPOINTER_TO_UINT (types->data);
	g_list_free (types);
	return type;
}

GList*
seahorse_registry_find_types (SeahorseRegistry *registry, const gchar *category, ...)
{
	va_list cats;
	GList *types;
	
	if (!registry)
		registry = seahorse_registry_get ();
	
	g_return_val_if_fail (SEAHORSE_IS_REGISTRY (registry), NULL);

	va_start (cats, category);
	types = lookup_types (registry, category, cats);
	va_end (cats);
	
	return types;
}
