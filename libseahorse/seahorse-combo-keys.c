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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "seahorse-combo-keys.h"

#include "seahorse-object.h"

enum {
  COMBO_STRING,
  COMBO_POINTER,
  N_COLUMNS
};

/* -----------------------------------------------------------------------------
 * HELPERS 
 */

static void
on_label_changed (GObject *obj,
                  GParamSpec *param,
                  gpointer user_data)
{
	SeahorseObject *object = SEAHORSE_OBJECT (obj);
	GtkComboBox *combo = GTK_COMBO_BOX (user_data);
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;
	const gchar *userid;
	gpointer pntr;
	SeahorseObject *frommodel;

	model = gtk_combo_box_get_model (combo);
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		gtk_tree_model_get (model, &iter,
		                    COMBO_POINTER, &pntr,
		                    -1);

		frommodel = SEAHORSE_OBJECT (pntr);
		if (frommodel == object) {
			userid = seahorse_object_get_label (object);
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			                    COMBO_STRING, userid,
			                    -1);
			break;
		}

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
	const gchar *userid;

	model = GTK_LIST_STORE (gtk_combo_box_get_model (combo));
	userid = seahorse_object_get_label (object);

	gtk_list_store_append (model, &iter);
	gtk_list_store_set (model, &iter,
	                    COMBO_STRING, userid,
	                    COMBO_POINTER, object,
	                    -1);

	g_signal_connect (object, "notify::label", G_CALLBACK (on_label_changed), combo);
}

static void
on_collection_removed (GcrCollection *collection,
                       GObject *obj,
                       gpointer user_data)
{
	SeahorseObject *object = SEAHORSE_OBJECT (obj);
	GtkComboBox *combo = GTK_COMBO_BOX (user_data);
	GtkTreeModel *model;
	GtkTreeIter iter;
	gpointer pntr;
	gboolean valid;
	SeahorseObject *frommodel;

	model = gtk_combo_box_get_model (combo);
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		gtk_tree_model_get (model, &iter,
		                    COMBO_POINTER, &pntr,
		                    -1);

		frommodel = SEAHORSE_OBJECT (pntr);
		if (frommodel == object) {
			gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
			break;
		}

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
		                    COMBO_STRING, none_option,
		                    COMBO_POINTER, NULL,
		                    -1);
	}

	gtk_tree_model_get_iter_first (model, &iter);
	gtk_combo_box_set_active_iter (combo, &iter);

	g_signal_connect_data (combo, "destroy", G_CALLBACK (on_combo_destroy),
	                       g_object_ref (collection), (GClosureNotify)g_object_unref, 0);
}

void
seahorse_combo_keys_set_active_id (GtkComboBox *combo, GQuark id)
{
    SeahorseObject *object;
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
                            
        object = SEAHORSE_OBJECT (pointer);
        
        if (!id) {
            if (!object) {
                gtk_combo_box_set_active_iter (combo, &iter);
                break;
            }
        } else if (object != NULL) {
            if (id == seahorse_object_get_id (object)) {
                gtk_combo_box_set_active_iter (combo, &iter);
                break;
            }
        }

        valid = gtk_tree_model_iter_next (model, &iter);
        i++;
    }
}

void 
seahorse_combo_keys_set_active (GtkComboBox *combo, SeahorseObject *object)
{
    seahorse_combo_keys_set_active_id (combo, 
                object == NULL ? 0 : seahorse_object_get_id (object));
}

SeahorseObject* 
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

    return SEAHORSE_OBJECT (pointer);
}

GQuark 
seahorse_combo_keys_get_active_id (GtkComboBox *combo)
{
    SeahorseObject *object = seahorse_combo_keys_get_active (combo);
    return object == NULL ? 0 : seahorse_object_get_id (object);
}
