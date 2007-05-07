/*
 * Seahorse
 *
 * Copyright (C) 2004-2006 Nate Nielsen
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

#include "seahorse-default-key-control.h"
#include "seahorse-key.h"
#include "seahorse-gconf.h"

/* TODO: This file should be renamed to seahorse-combo-keys.c once we're using SVN */

enum {
  COMBO_STRING,
  COMBO_POINTER,
  N_COLUMNS
};

/* -----------------------------------------------------------------------------
 * HELPERS 
 */

static void
key_added (SeahorseKeyset *skset, SeahorseKey *skey, GtkComboBox *combo)
{
    GtkListStore *model;
    GtkTreeIter iter;
    gchar *userid;
    
    g_return_if_fail (skey != NULL);
    g_return_if_fail (combo != NULL);
    
    model = GTK_LIST_STORE (gtk_combo_box_get_model (combo));
    
    userid = seahorse_key_get_display_name (skey);

    gtk_list_store_append (model, &iter);
    gtk_list_store_set (model, &iter,
                        COMBO_STRING, userid,
                        COMBO_POINTER, skey,
                        -1);

    g_free (userid);
    
    seahorse_keyset_set_closure (skset, skey, GINT_TO_POINTER (TRUE));
}

static void
key_changed (SeahorseKeyset *skset, SeahorseKey *skey, SeahorseKeyChange change, 
             GtkWidget *closure, GtkComboBox *combo)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean valid;
    gchar *userid;
    gpointer pntr;
    SeahorseKey *skeyfrommodel;
    
    g_return_if_fail (skey != NULL);

    model = gtk_combo_box_get_model (combo);
    valid = gtk_tree_model_get_iter_first (model, &iter);
    
    while (valid) {
        gtk_tree_model_get (model, &iter,
                            COMBO_POINTER, pntr,
                            -1);
                            
        skeyfrommodel = SEAHORSE_KEY (pntr);
        
        if (skeyfrommodel == skey) {
        userid = seahorse_key_get_display_name (skey);
            gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                COMBO_STRING, userid,
                                -1);
                                
        g_free (userid);
            break;
    }
    
        valid = gtk_tree_model_iter_next (model, &iter);
    }
}

static void
key_removed (SeahorseKeyset *skset, SeahorseKey *skey, GtkWidget *closure, 
             GtkComboBox *combo)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gpointer pntr;
    gboolean valid;
    SeahorseKey *skeyfrommodel;
    
    g_return_if_fail (skey != NULL);
    g_return_if_fail (combo != NULL);

    model = gtk_combo_box_get_model (combo);
    
    while (valid) {
        gtk_tree_model_get (model, &iter,
                            COMBO_POINTER, pntr,
                            -1);
                            
        skeyfrommodel = SEAHORSE_KEY (pntr);
        
        if (skeyfrommodel == skey) {
            gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
                                
            break;
        }
        
        valid = gtk_tree_model_iter_next (model, &iter);
    }
}

static void
combo_destroyed (GtkComboBox *combo, SeahorseKeyset *skset)
{
    g_signal_handlers_disconnect_by_func (skset, key_added, combo);
    g_signal_handlers_disconnect_by_func (skset, key_changed, combo);
    g_signal_handlers_disconnect_by_func (skset, key_removed, combo);
}

/* -----------------------------------------------------------------------------
 * PUBLIC CALLS
 */

void 
seahorse_combo_keys_attach (GtkComboBox *combo, SeahorseKeyset *skset,
                            const gchar *none_option)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkCellRenderer *renderer;
    SeahorseKey *skey;
    GList *l, *keys;

    /* Setup the None Option */
    model = gtk_combo_box_get_model (combo);
    if (!model) {
        model = GTK_TREE_MODEL (gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER));
        gtk_combo_box_set_model (combo, model);
        
        gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo));
        renderer = gtk_cell_renderer_text_new ();
        
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), renderer,
                                       "text", COMBO_STRING);                            
    }

    /* Setup the key list */
    keys = seahorse_keyset_get_keys (skset);  
    for (l = keys; l != NULL; l = g_list_next (l)) {
        skey = SEAHORSE_KEY (l->data);
        key_added (skset, skey, combo);
    }
    g_list_free (keys);

    g_signal_connect_after (skset, "added", G_CALLBACK (key_added), combo);
    g_signal_connect_after (skset, "changed", G_CALLBACK (key_changed), combo);
    g_signal_connect_after (skset, "removed", G_CALLBACK (key_removed), combo);

    if (none_option) {
        gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                            COMBO_STRING, none_option,
                            COMBO_POINTER, NULL,
                            -1);    
    }

    gtk_tree_model_get_iter_first (model, &iter);
    
    gtk_combo_box_set_active_iter (combo, &iter);
    
    /* Cleanup */
    g_object_ref (skset);
    g_object_set_data_full (G_OBJECT (combo), "skset", skset, g_object_unref);
    g_signal_connect (combo, "destroy", G_CALLBACK (combo_destroyed), skset);
}

void
seahorse_combo_keys_set_active_id (GtkComboBox *combo, GQuark keyid)
{
    SeahorseKey *skey;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean valid;
    gpointer pointer;
    guint i;
    
    g_return_if_fail (GTK_IS_COMBO_BOX (combo));

    model = gtk_combo_box_get_model (combo);
    g_return_if_fail (model != NULL);
    
    valid = gtk_tree_model_get_iter_first (model, &iter);
    i = 0;
    
    while (valid) {
        gtk_tree_model_get (model, &iter,
                            COMBO_POINTER, &pointer,
                            -1);
                            
        skey = SEAHORSE_KEY (pointer);
        
        if (!keyid) {
            if (!skey) {
                gtk_combo_box_set_active_iter (combo, &iter);
                break;
            }
        } else if (skey != NULL) {
            if (keyid == seahorse_key_get_keyid (skey)) {
                gtk_combo_box_set_active_iter (combo, &iter);
                break;
            }
        }

        valid = gtk_tree_model_iter_next (model, &iter);
        i++;
    }
}

void 
seahorse_combo_keys_set_active (GtkComboBox *combo, SeahorseKey *skey)
{
    seahorse_combo_keys_set_active_id (combo, 
                skey == NULL ? 0 : seahorse_key_get_keyid (skey));
}

SeahorseKey* 
seahorse_combo_keys_get_active (GtkComboBox *combo)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gpointer pointer;
    
    g_return_val_if_fail (GTK_IS_COMBO_BOX (combo), NULL);
    
    model = gtk_combo_box_get_model (combo);
    g_return_val_if_fail (model != NULL, NULL);
    
    gtk_combo_box_get_active_iter(combo, &iter);
    
    gtk_tree_model_get (model, &iter,
                        COMBO_POINTER, &pointer,
                        -1);

    return SEAHORSE_KEY (pointer);
}

GQuark 
seahorse_combo_keys_get_active_id (GtkComboBox *combo)
{
    SeahorseKey *skey = seahorse_combo_keys_get_active (combo);
    return skey == NULL ? 0 : seahorse_key_get_keyid (skey);
}
