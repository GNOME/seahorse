/* 
 * Seahorse
 * 
 * Copyright (C) 2005 Stefan Walter
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

#ifndef __CRYPTUI_KEY_LIST_H__
#define __CRYPTUI_KEY_LIST_H__

#include <gtk/gtk.h>

#include "cryptui-key-store.h"

enum {
    CRYPTUI_KEY_LIST_CHECKS = 0x01,
    CRYPTUI_KEY_LIST_ICONS = 0x02
};

#define CRYPTUI_TYPE_KEY_LIST               GTK_TYPE_TREE_VIEW
#define CRYPTUI_KEY_LIST(obj)               GTK_TREE_VIEW(obj)
#define CRYPTUI_KEY_LIST_CLASS(klass)       GTK_TREE_VIEW_CLASS(klass)
#define CRYPTUI_IS_KEY_LIST(obj)            GTK_IS_TREE_VIEW(obj)
#define CRYPTUI_IS_KEY_LIST_CLASS(klass)    GTK_IS_TREE_VIEW_CLASS(obj)
#define CRYPTUI_KEY_LIST_GET_CLASS(obj)     GTK_TREE_VIEW_GET_CLASS(obj)

GtkTreeView*      cryptui_key_list_new                  (CryptUIKeyStore *ckstore,
                                                         guint flags);

void              cryptui_key_list_setup                (GtkTreeView *view, 
                                                         CryptUIKeyStore *ckstore,
                                                         guint flags);

CryptUIKeyStore*  cryptui_key_list_get_key_store        (GtkTreeView *list);

CryptUIKeyset*    cryptui_key_list_get_keyset           (GtkTreeView *list);

gboolean          cryptui_key_list_have_selected_keys   (GtkTreeView *list);

GList*            cryptui_key_list_get_selected_keys    (GtkTreeView *list);

void              cryptui_key_list_set_selected_keys    (GtkTreeView *list,
                                                         GList *keys);

const gchar*      cryptui_key_list_get_selected_key     (GtkTreeView *list);

void              cryptui_key_list_set_selected_key     (GtkTreeView *list, 
                                                         const gchar *key);

#endif /* __CRYPTUI_KEY_LIST_H__ */
