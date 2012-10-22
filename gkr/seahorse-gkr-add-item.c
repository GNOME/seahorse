/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

#include "seahorse-gkr-backend.h"
#include "seahorse-gkr-dialogs.h"
#include "seahorse-gkr-keyring.h"

#include "seahorse-widget.h"
#include "seahorse-util.h"

#include <glib/gi18n.h>

void             on_add_item_label_changed                (GtkEntry *entry,
                                                           gpointer user_data);

void             on_add_item_password_toggled             (GtkToggleButton *button,
                                                           gpointer user_data);

void             on_add_item_response                     (GtkDialog *dialog,
                                                           int response,
                                                           gpointer user_data);

static void
on_item_stored (GObject *source,
                GAsyncResult *result,
                gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	GError *error = NULL;
	SecretItem *item;

	/* Clear the operation without cancelling it since it is complete */
	seahorse_gkr_dialog_complete_request (swidget, FALSE);

	item = secret_item_create_finish (result, &error);

	if (error == NULL) {
		g_object_unref (item);
	} else {
		seahorse_util_handle_error (&error, seahorse_widget_get_toplevel (swidget),
		                            _("Couldn't add item"));
	}

	g_object_unref (swidget);
	seahorse_widget_destroy (swidget);
}

G_MODULE_EXPORT void
on_add_item_label_changed (GtkEntry *entry,
                           gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	const gchar *keyring = gtk_entry_get_text (entry);
	seahorse_widget_set_sensitive (swidget, "ok", keyring && keyring[0]);
}

G_MODULE_EXPORT void 
on_add_item_password_toggled (GtkToggleButton *button,
                              gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	GtkWidget *widget= g_object_get_data (G_OBJECT (swidget), "gkr-secure-entry");
	gtk_entry_set_visibility (GTK_ENTRY (widget), gtk_toggle_button_get_active (button));
}

G_MODULE_EXPORT void
on_add_item_response (GtkDialog *dialog,
                      int response,
                      gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SecretCollection *collection;
	GCancellable *cancellable;
	GHashTable *attributes;
	GtkWidget *widget;
	SecretValue *secret;
	const gchar *label;
	GtkTreeIter iter;

	if (response == GTK_RESPONSE_HELP) {
		seahorse_widget_show_help (swidget);
		
	} else if (response == GTK_RESPONSE_ACCEPT) {
		widget = seahorse_widget_get_widget (swidget, "item-keyring");
		if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter))
			return;
		gtk_tree_model_get (gtk_combo_box_get_model (GTK_COMBO_BOX (widget)),
		                    &iter, 1, &collection, -1);

		widget = seahorse_widget_get_widget (swidget, "item-label");
		label = gtk_entry_get_text (GTK_ENTRY (widget));
		g_return_if_fail (label && label[0]);
		
		widget = g_object_get_data (G_OBJECT (swidget), "gkr-secure-entry");
		secret = secret_value_new (gtk_entry_get_text (GTK_ENTRY (widget)),
		                           -1, "text/plain");

		cancellable = seahorse_gkr_dialog_begin_request (swidget);
		attributes = g_hash_table_new (g_str_hash, g_str_equal);
		secret_item_create (collection, SECRET_SCHEMA_NOTE, attributes,
		                    label, secret, SECRET_ITEM_CREATE_NONE,
		                    cancellable, on_item_stored, g_object_ref (swidget));
		g_hash_table_unref (attributes);
		g_object_unref (cancellable);

	} else {
		seahorse_widget_destroy (swidget);
	}
}

void
seahorse_gkr_add_item_show (GtkWindow *parent)
{
	SeahorseWidget *swidget = NULL;
	GtkEntryBuffer *buffer;
	GtkWidget *entry, *widget;
	GList *keyrings, *l;
	GtkCellRenderer *cell;
	GtkListStore *store;
	GtkTreeIter iter;

	swidget = seahorse_widget_new_allow_multiple ("gkr-add-item", parent);
	g_return_if_fail (swidget);
	
	/* Load up a list of all the keyrings, and select the default */
	widget = seahorse_widget_get_widget (swidget, "item-keyring");
	store = gtk_list_store_new (2, G_TYPE_STRING, SECRET_TYPE_COLLECTION);
	gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (store));
	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), cell, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (widget), cell, "text", 0);

	keyrings = seahorse_gkr_backend_get_keyrings (NULL);
	for (l = keyrings; l; l = g_list_next (l)) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
		                    0, secret_collection_get_label (l->data),
		                    1, l->data,
		                    -1);
		if (seahorse_gkr_keyring_get_is_default (l->data))
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &iter);
	}
	g_list_free (keyrings);

	g_object_unref (store);

	widget = seahorse_widget_get_widget (swidget, "item-label");
	g_return_if_fail (widget); 
	on_add_item_label_changed (GTK_ENTRY (widget), swidget);
	
	widget = seahorse_widget_get_widget (swidget, "password-area");
	g_return_if_fail (widget);
	buffer = gcr_secure_entry_buffer_new ();
	entry = gtk_entry_new_with_buffer (buffer);
	g_object_unref (buffer);
	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (entry));
	gtk_widget_show (GTK_WIDGET (entry));
	g_object_set_data (G_OBJECT (swidget), "gkr-secure-entry", entry);

	widget = seahorse_widget_get_widget (swidget, "show-password");
	on_add_item_password_toggled (GTK_TOGGLE_BUTTON (widget), swidget);

	widget = seahorse_widget_get_toplevel (swidget);
	gtk_widget_show (widget);
	gtk_window_present (GTK_WINDOW (widget));
}
