#ifndef SEAHORSEREGISTRY_H_
#define SEAHORSEREGISTRY_H_

#include <glib-object.h>

G_BEGIN_DECLS

typedef GType (*SeaRegisterType) (void);

#define SEA_TYPE_REGISTRY             (sea_registry_get_type())
#define SEA_REGISTRY(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), SEA_TYPE_REGISTRY, SeaRegistry))
#define SEA_REGISTRY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), SEA_TYPE_REGISTRY, GObject))
#define SEA_IS_REGISTRY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), SEA_TYPE_REGISTRY))
#define SEA_IS_REGISTRY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), SEA_TYPE_REGISTRY))
#define SEA_REGISTRY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), SEA_TYPE_REGISTRY, SeaRegistryClass))

typedef struct _SeaRegistry      SeaRegistry;
typedef struct _SeaRegistryClass SeaRegistryClass;

struct _SeaRegistry {
	 GObject parent;
};

struct _SeaRegistryClass {
	GObjectClass parent_class;
};

/* member functions */
GType                sea_registry_get_type        (void) G_GNUC_CONST;

SeaRegistry*         sea_registry_get             (void);

void                 sea_registry_load_types      (SeaRegistry *registry, 
                                                   const SeaRegisterType *types);

void                 sea_registry_register_type   (SeaRegistry *registry, 
                                                   GType type, const gchar *category, 
                                                   ...) G_GNUC_NULL_TERMINATED;

GType                sea_registry_find_type       (SeaRegistry *registry, 
                                                   const gchar *category,
                                                   ...) G_GNUC_NULL_TERMINATED;

GList*               sea_registry_find_types      (SeaRegistry *registry, 
                                                   const gchar *category,
                                                   ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS

#endif /*SEAHORSEREGISTRY_H_*/
