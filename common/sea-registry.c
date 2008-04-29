
#include "sea-cleanup.h"
#include "sea-registry.h"

typedef struct _SeaRegistryPrivate SeaRegistryPrivate;

struct _SeaRegistryPrivate {
	GHashTable *categories;
};

#define SEA_REGISTRY_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE((o), SEA_TYPE_REGISTRY, SeaRegistryPrivate))

G_DEFINE_TYPE(SeaRegistry, sea_registry, G_TYPE_OBJECT);

static SeaRegistry *registry_singleton = NULL; 

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
register_type_for_category (SeaRegistry *registry, const gchar *category, GType type)
{
	SeaRegistryPrivate *pv = SEA_REGISTRY_GET_PRIVATE (registry);
	GHashTable *set;
	
	g_return_if_fail (SEA_IS_REGISTRY (registry));
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
lookup_types (SeaRegistry *registry, const gchar *category, va_list cats)
{
	SeaRegistryPrivate *pv = SEA_REGISTRY_GET_PRIVATE (registry);
	GList *l, *type, *types = NULL;
	GHashTable *set;
	
	g_return_val_if_fail (SEA_IS_REGISTRY (registry), NULL);
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
sea_registry_init (SeaRegistry *obj)
{
	SeaRegistryPrivate *pv = SEA_REGISTRY_GET_PRIVATE (obj);
	pv->categories = g_hash_table_new_full (g_str_hash, g_str_equal, 
	                                        g_free, (GDestroyNotify)g_hash_table_destroy);
}

static void
sea_registry_dispose (GObject *obj)
{
	SeaRegistryPrivate *pv = SEA_REGISTRY_GET_PRIVATE (obj);
	g_hash_table_remove_all (pv->categories);
	G_OBJECT_CLASS (sea_registry_parent_class)->dispose (obj);
}

static void
sea_registry_finalize (GObject *obj)
{
	SeaRegistryPrivate *pv = SEA_REGISTRY_GET_PRIVATE (obj);
	g_hash_table_destroy (pv->categories);
	G_OBJECT_CLASS (sea_registry_parent_class)->finalize (obj);
}

static void
sea_registry_class_init (SeaRegistryClass *klass)
{
	GObjectClass *gobject_class;
	gobject_class = (GObjectClass*)klass;

	sea_registry_parent_class = g_type_class_peek_parent (klass);
	gobject_class->dispose = sea_registry_dispose;
	gobject_class->finalize = sea_registry_finalize;

	g_type_class_add_private (gobject_class, sizeof (SeaRegistryPrivate));
}

/* -------------------------------------------------------------------------------
 * PUBLIC 
 */

SeaRegistry*
sea_registry_get (void)
{
	if (!registry_singleton) {
		registry_singleton = g_object_new (SEA_TYPE_REGISTRY, NULL);
		sea_cleanup_register (cleanup_registry, NULL);
	}
	
	return registry_singleton;
}

void
sea_registry_load_types (SeaRegistry *registry, const SeaRegisterType *types)
{
	GType type;
	gpointer klass;
	
	while (*types) {
		type = (*types) ();
		g_return_if_fail (type);
		
		klass = g_type_class_ref (type);
		g_type_class_unref (klass);
		
		++types;
	}
}

void
sea_registry_register_type (SeaRegistry *registry, GType type, 
                            const gchar *category, ...)
{
	va_list cats;

	if (!registry)
		registry = sea_registry_get ();
	
	g_return_if_fail (SEA_IS_REGISTRY (registry));
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
sea_registry_find_type (SeaRegistry *registry, const gchar *category, ...)
{
	va_list cats;
	GList *types;
	GType type;
	
	if (!registry)
		registry = sea_registry_get ();
	
	g_return_val_if_fail (SEA_IS_REGISTRY (registry), 0);

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
sea_registry_find_types (SeaRegistry *registry, const gchar *category, ...)
{
	va_list cats;
	GList *types;
	
	if (!registry)
		registry = sea_registry_get ();
	
	g_return_val_if_fail (SEA_IS_REGISTRY (registry), NULL);

	va_start (cats, category);
	types = lookup_types (registry, category, cats);
	va_end (cats);
	
	return types;
}
