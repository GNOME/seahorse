/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#include "cryptui-key-combo.h"

G_DEFINE_TYPE (CryptUIKeyCombo, cryptui_key_combo, GTK_TYPE_COMBO_BOX);

/* -----------------------------------------------------------------------------
 * OBJECT 
 */


static void
cryptui_key_combo_init (CryptUIKeyCombo *combo)
{
    
}

static void
cryptui_key_combo_class_init (CryptUIKeyComboClass *klass)
{
    cryptui_key_combo_parent_class = g_type_class_peek_parent (klass);
}

CryptUIKeyCombo*  
cryptui_key_combo_new (CryptUIKeyStore *ckstore)
{
    GtkCellRenderer *cell;
    CryptUIKeyCombo *combo;

    combo = g_object_new (CRYPTUI_TYPE_KEY_COMBO, "model", ckstore, NULL);
    
    cell = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell, "text", 0, NULL);
    
    return combo;
}

void
cryptui_key_combo_set_key (CryptUIKeyCombo *combo, const gchar *key)
{
    GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
    CryptUIKeyStore *ckstore;
    GtkTreeIter iter;
    
    g_return_if_fail (CRYPTUI_IS_KEY_STORE (model));
    ckstore = CRYPTUI_KEY_STORE (model);
    
    if (cryptui_key_store_get_iter_from_key (ckstore, key, &iter))
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo), &iter);
}

const gchar*
cryptui_key_combo_get_key (CryptUIKeyCombo *combo)
{
    GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
    CryptUIKeyStore *ckstore;
    GtkTreeIter iter;
    
    g_return_val_if_fail (CRYPTUI_IS_KEY_STORE (model), NULL);
    ckstore = CRYPTUI_KEY_STORE (model);
    
    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter))
        return cryptui_key_store_get_key_from_iter (ckstore, &iter);
    
    return NULL;
}

