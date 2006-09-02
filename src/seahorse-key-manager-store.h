/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2006 Nate Nielsen
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

#include <gtk/gtk.h>

#include "seahorse-context.h"
#include "seahorse-keyset.h"
#include "seahorse-key-model.h"

#define SEAHORSE_TYPE_KEY_MANAGER_STORE             (seahorse_key_manager_store_get_type ())
#define SEAHORSE_KEY_MANAGER_STORE(obj)             (GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_KEY_MANAGER_STORE, SeahorseKeyManagerStore))
#define SEAHORSE_KEY_MANAGER_STORE_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEY_MANAGER_STORE, SeahorseKeyManagerStoreClass))
#define SEAHORSE_IS_KEY_MANAGER_STORE(obj)          (GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_KEY_MANAGER_STORE))
#define SEAHORSE_IS_KEY_MANAGER_STORE_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEY_MANAGER_STORE))
#define SEAHORSE_KEY_MANAGER_STORE_GET_CLASS(obj)   (GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_KEY_MANAGER_STORE, SeahorseKeyManagerStoreClass))

typedef struct _SeahorseKeyManagerStore SeahorseKeyManagerStore;
typedef struct _SeahorseKeyManagerStorePriv SeahorseKeyManagerStorePriv;
typedef struct _SeahorseKeyManagerStoreClass SeahorseKeyManagerStoreClass;

typedef enum _SeahorseKeyManagerStoreMode {
    KEY_STORE_MODE_ALL,
    KEY_STORE_MODE_FILTERED
} SeahorseKeyManagerStoreMode;

struct _SeahorseKeyManagerStore {
    SeahorseKeyModel               parent;
 
    /*< public >*/
    SeahorseKeyset                 *skset;
    
    /*< private >*/
    SeahorseKeyManagerStorePriv    *priv;
};

struct _SeahorseKeyManagerStoreClass {
    SeahorseKeyModelClass           parent_class;
};

SeahorseKeyManagerStore*   seahorse_key_manager_store_new       (SeahorseKeyset *skset,
                                                                 GtkTreeView *view);

SeahorseKey*        seahorse_key_manager_store_get_key_from_path (GtkTreeView *view,
                                                                  GtkTreePath *path,
                                                                  guint *uid);

GList*              seahorse_key_manager_store_get_all_keys      (GtkTreeView *view);

void                seahorse_key_manager_store_set_selected_keys (GtkTreeView *view,
                                                                  GList* keys);
                                                             
GList*              seahorse_key_manager_store_get_selected_keys (GtkTreeView *view);

SeahorseKey*        seahorse_key_manager_store_get_selected_key  (GtkTreeView *view,
                                                                  guint *uid);

/* -----------------------------------------------------------------------------
 * DRAG AND DROP 
 */

enum SeahorseTargetTypes {
    TEXT_PLAIN,
    TEXT_URIS
};

/* Drag and drop targent entries */
extern const GtkTargetEntry seahorse_target_entries[];
extern guint seahorse_n_targets;

#endif /* __SEAHORSE_KEY_MANAGER_STORE_H__ */
