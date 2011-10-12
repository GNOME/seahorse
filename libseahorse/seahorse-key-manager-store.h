/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2006 Stefan Walter
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

#ifndef __SEAHORSE_KEY_MANAGER_STORE_H__
#define __SEAHORSE_KEY_MANAGER_STORE_H__

#include "seahorse-collection.h"
#include "seahorse-object.h"

#include <gtk/gtk.h>
#include <gcr/gcr.h>

#define SEAHORSE_TYPE_KEY_MANAGER_STORE             (seahorse_key_manager_store_get_type ())
#define SEAHORSE_KEY_MANAGER_STORE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_KEY_MANAGER_STORE, SeahorseKeyManagerStore))
#define SEAHORSE_KEY_MANAGER_STORE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEY_MANAGER_STORE, SeahorseKeyManagerStoreClass))
#define SEAHORSE_IS_KEY_MANAGER_STORE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_KEY_MANAGER_STORE))
#define SEAHORSE_IS_KEY_MANAGER_STORE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEY_MANAGER_STORE))
#define SEAHORSE_KEY_MANAGER_STORE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_KEY_MANAGER_STORE, SeahorseKeyManagerStoreClass))

typedef struct _SeahorseKeyManagerStore SeahorseKeyManagerStore;
typedef struct _SeahorseKeyManagerStorePriv SeahorseKeyManagerStorePriv;
typedef struct _SeahorseKeyManagerStoreClass SeahorseKeyManagerStoreClass;

typedef enum _SeahorseKeyManagerStoreMode {
	KEY_STORE_MODE_ALL,
	KEY_STORE_MODE_FILTERED
} SeahorseKeyManagerStoreMode;

struct _SeahorseKeyManagerStore {
	GcrCollectionModel parent;

	/*< private >*/
	SeahorseKeyManagerStorePriv *priv;
};

struct _SeahorseKeyManagerStoreClass {
	GcrCollectionModelClass parent_class;
};

GType                      seahorse_key_manager_store_get_type               (void) G_GNUC_CONST;

SeahorseKeyManagerStore*   seahorse_key_manager_store_new                    (GcrCollection *collection,
                                                                              GtkTreeView *view,
                                                                              GSettings *settings);

GObject *                  seahorse_key_manager_store_get_object_from_path   (GtkTreeView *view,
                                                                              GtkTreePath *path);

GList*                     seahorse_key_manager_store_get_all_objects        (GtkTreeView *view);

void                       seahorse_key_manager_store_set_selected_objects   (GtkTreeView *view,
                                                                              GList* objects);

GList*                     seahorse_key_manager_store_get_selected_objects   (GtkTreeView *view);

GObject *                  seahorse_key_manager_store_get_selected_object    (GtkTreeView *view);

#endif /* __SEAHORSE_KEY_MANAGER_STORE_H__ */
