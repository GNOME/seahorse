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

#ifndef SEAHORSEREGISTRY_H_
#define SEAHORSEREGISTRY_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define SEAHORSE_TYPE_REGISTRY             (seahorse_registry_get_type())
#define SEAHORSE_REGISTRY(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), SEAHORSE_TYPE_REGISTRY, SeahorseRegistry))
#define SEAHORSE_REGISTRY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), SEAHORSE_TYPE_REGISTRY, GObject))
#define SEAHORSE_IS_REGISTRY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), SEAHORSE_TYPE_REGISTRY))
#define SEAHORSE_IS_REGISTRY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), SEAHORSE_TYPE_REGISTRY))
#define SEAHORSE_REGISTRY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), SEAHORSE_TYPE_REGISTRY, SeahorseRegistryClass))

typedef struct _SeahorseRegistry      SeahorseRegistry;
typedef struct _SeahorseRegistryClass SeahorseRegistryClass;

struct _SeahorseRegistry {
	 GObject parent;
};

struct _SeahorseRegistryClass {
	GObjectClass parent_class;
};

/* member functions */
GType                seahorse_registry_get_type          (void) G_GNUC_CONST;

SeahorseRegistry*    seahorse_registry_get               (void);

void                 seahorse_registry_register_type     (SeahorseRegistry *registry, 
                                                          GType type, const gchar *category, 
                                                          ...) G_GNUC_NULL_TERMINATED;

void                 seahorse_registry_register_object   (SeahorseRegistry *registry, 
                                                          GObject *object, const gchar *category, 
                                                          ...) G_GNUC_NULL_TERMINATED;

GType                seahorse_registry_object_type       (SeahorseRegistry *registry, 
                                                          const gchar *category,
                                                          ...) G_GNUC_NULL_TERMINATED;

GList*               seahorse_registry_object_types      (SeahorseRegistry *registry, 
                                                          const gchar *category,
                                                          ...) G_GNUC_NULL_TERMINATED;

GObject*             seahorse_registry_object_instance   (SeahorseRegistry *registry, 
                                                          const gchar *category,
                                                          ...) G_GNUC_NULL_TERMINATED;

GList*               seahorse_registry_object_instances  (SeahorseRegistry *registry, 
                                                          const gchar *category,
                                                          ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS

#endif /*SEAHORSEREGISTRY_H_*/
