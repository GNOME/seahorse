/*
 * Seahorse
 *
 * Copyright (C) 2004-2006 Stefan Walter
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "seahorse-combo-keys.h"

#include "libseahorse/seahorse-object.h"

enum {
  COMBO_LABEL,
  COMBO_MARKUP,
  COMBO_POINTER,
  N_COLUMNS
};

typedef struct {
	GHashTable *labels;
	gboolean collision;
} ComboClosure;

static void
combo_closure_free (gpointer data)
{
	ComboClosure *closure = data;
	g_hash_table_destroy (closure->labels);
	g_slice_free (ComboClosure, closure);
}

static ComboClosure *
combo_closure_new (void)
{
	ComboClosure *closure = g_slice_new0 (ComboClosure);
	closure->labels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	return closure;
}

static void
refresh_all_markup_in_combo (GtkComboBox *combo)
{
	GtkTreeModel *model;
	gpointer object;
	GtkTreeIter iter;
	gboolean valid;

	model = gtk_combo_box_get_model (combo);
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		gtk_tree_model_get (model, &iter, COMBO_POINTER, &object, -1);
		g_object_notify (object, "label");
		valid = gtk_tree_model_iter_next (model, &iter);
	}
}

static gchar *
calculate_markup_for_object (GtkComboBox *combo,
                             const gchar *label,
                             SeahorseObject *object)
{
	ComboClosure *closure;
	const gchar *keyid;
	gchar *ident;
	gchar *markup;

	closure = g_object_get_data (G_OBJECT (combo), "combo-keys-closure");

	if (!closure->collision) {
		if (g_hash_table_lookup (closure->labels, label)) {
			closure->collision = TRUE;
			refresh_all_markup_in_combo (combo);
		} else {
			g_hash_table_insert (closure->labels, g_strdup (label), "X");
		}
	}

	if (closure->collision && SEAHORSE_IS_PGP_KEY (object)) {
		keyid = seahorse_pgp_key_get_keyid (SEAHORSE_PGP_KEY (object));
		ident = seahorse_pgp_key_calc_identifier (keyid);
		markup = g_markup_printf_escaped ("%s <span size='small'>[%s]</span>", label, ident);
		g_free (ident);
	} else {
		markup = g_markup_escape_text (label, -1);
	}

	return markup;
}

static void
on_label_changed (GObject *obj,
                  GParamSpec *param,
                  gpointer user_data)
{
	SeahorseObject *object = SEAHORSE_OBJECT (obj);
	GtkComboBox *combo = GTK_COMBO_BOX (user_data);
	ComboClosure *closure;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;
	gchar *previous;
	gchar *markup;
	gpointer pntr;
	const gchar *label;

	closure = g_object_get_data (G_OBJECT (combo), "combo-keys-closure");
	model = gtk_combo_box_get_model (combo);
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		gtk_tree_model_get (model, &iter, COMBO_POINTER, &pntr, COMBO_LABEL, &previous, -1);
		if (SEAHORSE_OBJECT (pntr) == object) {

			/* Remove this from label collision checks */
			g_hash_table_remove (closure->labels, previous);

			/* Calculate markup taking into account label collisions */
			label = seahorse_object_get_label (object);
			markup = calculate_markup_for_object (combo, label, object);
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			                    COMBO_LABEL, label,
			                    COMBO_MARKUP, markup,
			                    -1);
			g_free (markup);
			break;
		}

		g_free (previous);
		valid = gtk_tree_model_iter_next (model, &iter);
	}
}

static void
on_collection_added (GcrCollection *collection,
                     GObject *obj,
                     gpointer user_data)
{
	SeahorseObject *object = SEAHORSE_OBJECT (obj);
	GtkComboBox *combo = GTK_COMBO_BOX (user_data);
	GtkListStore *model;
	GtkTreeIter iter;
	const gchar *label;
	gchar *markup;

	model = GTK_LIST_STORE (gtk_combo_box_get_model (combo));

	label = seahorse_object_get_label (object);
	markup = calculate_markup_for_object (combo, label, object);

	gtk_list_store_append (model, &iter);
	gtk_list_store_set (model, &iter,
	                    COMBO_LABEL, label,
	                    COMBO_MARKUP, markup,
	                    COMBO_POINTER, object,
	                    -1);

	g_free (markup);

	g_signal_connect (object, "notify::label", G_CALLBACK (on_label_changed), combo);
}

static void
on_collection_removed (GcrCollection *collection,
                       GObject *obj,
                       gpointer user_data)
{
	ComboClosure *closure = g_object_get_data (user_data, "combo-keys-closure");
	SeahorseObject *object = SEAHORSE_OBJECT (obj);
	GtkComboBox *combo = GTK_COMBO_BOX (user_data);
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *previous;
	gpointer pntr;
	gboolean valid;

	model = gtk_combo_box_get_model (combo);
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		gtk_tree_model_get (model, &iter,
		                    COMBO_LABEL, &previous,
		                    COMBO_POINTER, &pntr,
		                    -1);

		if (SEAHORSE_OBJECT (pntr) == object) {
			g_hash_table_remove (closure->labels, previous);
			gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
			break;
		}

		g_free (previous);
		valid = gtk_tree_model_iter_next (model, &iter);
	}

	g_signal_handlers_disconnect_by_func (object, on_label_changed, combo);
}

static void
on_combo_destroy (GtkComboBox *combo,
                  gpointer user_data)
{
	GcrCollection *collection = GCR_COLLECTION (user_data);
	GList *objects, *l;

	objects = gcr_collection_get_objects (collection);
	for (l = objects; l != NULL; l = g_list_next (l))
		g_signal_handlers_disconnect_by_func (l->data, on_label_changed, combo);
	g_list_free (objects);
	g_signal_handlers_disconnect_by_func (collection, on_collection_added, combo);
	g_signal_handlers_disconnect_by_func (collection, on_collection_removed, combo);
}

/* -----------------------------------------------------------------------------
 * PUBLIC CALLS
 */

void
seahorse_combo_keys_attach (GtkComboBox *combo,
                            GcrCollection *collection,
                            const gchar *none_option)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkCellRenderer *renderer;
	GList *l, *objects;

	g_object_set_data_full (G_OBJECT (combo), "combo-keys-closure",
	                        combo_closure_new (), combo_closure_free);

	/* Setup the None Option */
	model = gtk_combo_box_get_model (combo);
	if (!model) {
		model = GTK_TREE_MODEL (gtk_list_store_new (N_COLUMNS, G_TYPE_STRING,
		                                            G_TYPE_STRING, G_TYPE_POINTER));
		gtk_combo_box_set_model (combo, model);

		gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo));
		renderer = gtk_cell_renderer_text_new ();

		gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
		gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), renderer,
		                               "markup", COMBO_MARKUP);
	}

	/* Setup the object list */
	objects = gcr_collection_get_objects (collection);
	for (l = objects; l != NULL; l = g_list_next (l))
		on_collection_added (collection, l->data, combo);
	g_list_free (objects);

	g_signal_connect_after (collection, "added", G_CALLBACK (on_collection_added), combo);
	g_signal_connect_after (collection, "removed", G_CALLBACK (on_collection_removed), combo);

	if (none_option) {
		gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		                    COMBO_LABEL, NULL,
		                    COMBO_MARKUP, none_option,
		                    COMBO_POINTER, NULL,
		                    -1);
	}

	gtk_tree_model_get_iter_first (model, &iter);
	gtk_combo_box_set_active_iter (combo, &iter);

	g_signal_connect_data (combo, "destroy", G_CALLBACK (on_combo_destroy),
	                       g_object_ref (collection), (GClosureNotify)g_object_unref, 0);
}

void
seahorse_combo_keys_set_active_id (GtkComboBox *combo,
                                   const gchar *keyid)
{
    SeahorsePgpKey *key;
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

        key = SEAHORSE_PGP_KEY (pointer);

        if (!keyid) {
            if (!key) {
                gtk_combo_box_set_active_iter (combo, &iter);
                break;
            }
        } else if (key != NULL) {
            if (g_strcmp0 (seahorse_pgp_key_get_keyid (key), keyid) == 0) {
                gtk_combo_box_set_active_iter (combo, &iter);
                break;
            }
        }

        valid = gtk_tree_model_iter_next (model, &iter);
        i++;
    }
}

void
seahorse_combo_keys_set_active (GtkComboBox *combo,
                                SeahorsePgpKey *key)
{
	seahorse_combo_keys_set_active_id (combo,
	          key == NULL ? NULL : seahorse_pgp_key_get_keyid (key));
}

SeahorsePgpKey *
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

    return SEAHORSE_PGP_KEY (pointer);
}

const gchar *
seahorse_combo_keys_get_active_id (GtkComboBox *combo)
{
	SeahorsePgpKey *key = seahorse_combo_keys_get_active (combo);
	return key == NULL ? 0 : seahorse_pgp_key_get_keyid (key);
}
