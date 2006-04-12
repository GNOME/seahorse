/*
 * Seahorse
 *
 * Copyright (C) 2004-2005 Nate Nielsen
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
#include <gtk/gtk.h>

#include "cryptui-key-list.h"

/* -----------------------------------------------------------------------------
 * INTERNAL
 */
 
GtkTreeViewColumn*
append_text_column (GtkTreeView *view, const gchar *label, const gint index)
{
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (label, renderer, "text", index, NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_append_column (view, column);

    return column;
}

static void
check_toggled (GtkCellRendererToggle *cellrenderertoggle, gchar *path, GtkTreeView *view)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    model = gtk_tree_view_get_model (view);
    g_return_if_fail (CRYPTUI_IS_KEY_STORE (model));
    
    g_assert (path != NULL);
    
    if (gtk_tree_model_get_iter_from_string (model, &iter, path))
        cryptui_key_store_check_toggled (CRYPTUI_KEY_STORE (model), view, &iter);
}
    
static void
row_activated (GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *arg2, 
               CryptUIKeyStore *ckstore)
{
    GtkTreeIter iter;

    g_assert (path != NULL);
    
    if (gtk_tree_model_get_iter (GTK_TREE_MODEL (ckstore), &iter, path))
        cryptui_key_store_check_toggled (ckstore, treeview, &iter);
}


/* -----------------------------------------------------------------------------
 * PUBLIC
 */

GtkTreeView*      
cryptui_key_list_new (CryptUIKeyStore *ckstore, guint flags)
{
    GtkTreeView *view = GTK_TREE_VIEW (gtk_tree_view_new ());
    cryptui_key_list_setup (view, ckstore, flags);
    return view;
}

void
cryptui_key_list_setup (GtkTreeView *view, CryptUIKeyStore *ckstore,
                        guint flags)
{
    GtkTreeViewColumn *col;
    GtkTreeSelection *sel;
    GtkCellRenderer *renderer;

    gtk_tree_view_set_model (view, GTK_TREE_MODEL (ckstore));
    sel = gtk_tree_view_get_selection (view);
 
    if (flags & CRYPTUI_KEY_LIST_CHECKS) {
        g_object_set (ckstore, "use-checks", TRUE, NULL);
        
        renderer = gtk_cell_renderer_toggle_new ();
        g_signal_connect (renderer, "toggled", G_CALLBACK (check_toggled), view);
        
        col = gtk_tree_view_column_new_with_attributes ("", renderer, "active", CRYPTUI_KEY_STORE_CHECK, NULL);
        gtk_tree_view_column_set_resizable (col, FALSE);
        gtk_tree_view_append_column (view, col);
        
        g_signal_connect (view, "row_activated", G_CALLBACK (row_activated), ckstore);
        
        gtk_tree_selection_set_mode (sel, GTK_SELECTION_BROWSE);
        
    /* No checks, allow multiple selection */
    } else {
        gtk_tree_selection_set_mode (sel, GTK_SELECTION_MULTIPLE);
    }
    
    /* TODO: Icons */
 
    /* TODO: Translatable */
    col = append_text_column (view, "Name", CRYPTUI_KEY_STORE_NAME);
    append_text_column (view, "Key ID", CRYPTUI_KEY_STORE_KEYID);
    gtk_tree_view_column_set_sort_column_id (col, CRYPTUI_KEY_STORE_NAME);
    
    gtk_tree_view_set_rules_hint (view, TRUE);
    gtk_widget_set_size_request (GTK_WIDGET (view), 500, 250);
}

GList*
cryptui_key_list_get_selected_keys (GtkTreeView *view)
{
    GtkTreeModel *model;
    
    model = gtk_tree_view_get_model (view);
    g_return_val_if_fail (CRYPTUI_IS_KEY_STORE (model), NULL);
    
    return cryptui_key_store_get_selected_keys (CRYPTUI_KEY_STORE (model), view);
}

void
cryptui_key_list_set_selected_keys (GtkTreeView *view, GList *keys)
{
    GtkTreeModel *model;
    
    model = gtk_tree_view_get_model (view);
    g_return_if_fail (CRYPTUI_IS_KEY_STORE (model));
    
    cryptui_key_store_set_selected_keys (CRYPTUI_KEY_STORE (model), view, keys);
}

const gchar*
cryptui_key_list_get_selected_key (GtkTreeView *view)
{
    GtkTreeModel *model;
    
    model = gtk_tree_view_get_model (view);
    g_return_val_if_fail (CRYPTUI_IS_KEY_STORE (model), NULL);
    
    return cryptui_key_store_get_selected_key (CRYPTUI_KEY_STORE (model), view);
}

void
cryptui_key_list_set_selected_key (GtkTreeView *view, const gchar *key)
{
    GtkTreeModel *model;
    
    model = gtk_tree_view_get_model (view);
    g_return_if_fail (CRYPTUI_IS_KEY_STORE (model));
    
    cryptui_key_store_set_selected_key (CRYPTUI_KEY_STORE (model), view, key);
}
