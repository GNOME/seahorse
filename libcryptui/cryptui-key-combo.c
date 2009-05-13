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

#include "config.h"
#include <gtk/gtk.h>

#include "cryptui-key-combo.h"

/* -----------------------------------------------------------------------------
 * INTERNAL
 */

static gboolean
is_row_separator (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    gboolean ret;
    gtk_tree_model_get (model, iter, CRYPTUI_KEY_STORE_SEPARATOR, &ret, -1);
    return ret;
}

/* -----------------------------------------------------------------------------
 * PUBLIC
 */

/**
 * cryptui_key_combo_new:
 * @ckstore: key store containing keys to be listed in the combo
 *
 * Creates a combobox containing the keys in ckstore
 *
 * Returns: the new CryptUIKeyCombo
 */
GtkComboBox*
cryptui_key_combo_new (CryptUIKeyStore *ckstore)
{
    GtkComboBox *ckcombo = g_object_new (GTK_TYPE_COMBO_BOX, "model", ckstore, NULL);
    cryptui_key_combo_setup (ckcombo, ckstore);
    return ckcombo;
}

/**
 * cryptui_key_combo_setup:
 * @combo: a GtkComboBox
 * @ckstore: key store containing keys to be listed in the combo
 *
 * Populates an existing GtkComboBox with keys from a CryptUIKeyStore
 */
void
cryptui_key_combo_setup (GtkComboBox *combo, CryptUIKeyStore *ckstore)
{
    GtkCellRenderer *cell;

    gtk_combo_box_set_model (combo, GTK_TREE_MODEL (ckstore));

    cell = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell, "text", 0, NULL);
    g_object_set (G_OBJECT (cell), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    gtk_combo_box_set_row_separator_func (combo, is_row_separator, NULL, NULL);
    cryptui_key_combo_set_key (combo, NULL);
}

/**
 * cryptui_key_combo_get_key_store:
 * @ckcombo: a CryptUICombo
 *
 * Gets the key store from a CryptUiCombo
 *
 * Returns: the key store
 */
CryptUIKeyStore*
cryptui_key_combo_get_key_store (GtkComboBox *ckcombo)
{
    GtkTreeModel *model = gtk_combo_box_get_model (ckcombo);
    g_return_val_if_fail (CRYPTUI_KEY_STORE (model), NULL);
    return CRYPTUI_KEY_STORE (model);
}

/**
 * cryptui_key_combo_get_keyset:
 * @ckcombo: a CryptUICombo
 *
 * Gets the keyset stored in the combo's key store.
 *
 * Returns: a CryptuiKeyset
 */
CryptUIKeyset*
cryptui_key_combo_get_keyset (GtkComboBox *ckcombo)
{
    CryptUIKeyStore *ckstore = cryptui_key_combo_get_key_store (ckcombo);
    return ckstore ? cryptui_key_store_get_keyset (ckstore) : NULL;
}

/**
 * cryptui_key_combo_set_key:
 * @ckcombo: a CryptUICombo
 * @key: a CryptUI Key
 *
 * Sets the combo's selection to the indicated key
 */
void
cryptui_key_combo_set_key (GtkComboBox *ckcombo, const gchar *key)
{
    GtkTreeModel *model = gtk_combo_box_get_model (ckcombo);
    CryptUIKeyStore *ckstore;
    GtkTreeIter iter;

    g_return_if_fail (CRYPTUI_IS_KEY_STORE (model));
    ckstore = CRYPTUI_KEY_STORE (model);

    if (cryptui_key_store_get_iter_from_key (ckstore, key, &iter))
        gtk_combo_box_set_active_iter (ckcombo, &iter);
}

/**
 * cryptui_key_combo_get_key:
 * @ckcombo: a CryptUICombo
 *
 * Gets the first selected key from the combo
 *
 * Returns: the first selected key
 */
const gchar*
cryptui_key_combo_get_key (GtkComboBox *ckcombo)
{
    GtkTreeModel *model = gtk_combo_box_get_model (ckcombo);
    CryptUIKeyStore *ckstore;
    GtkTreeIter iter;

    g_return_val_if_fail (CRYPTUI_IS_KEY_STORE (model), NULL);
    ckstore = CRYPTUI_KEY_STORE (model);

    if (gtk_combo_box_get_active_iter (ckcombo, &iter))
        return cryptui_key_store_get_key_from_iter (ckstore, &iter);

    return NULL;
}

