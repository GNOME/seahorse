/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

/**
 * SECTION:seahorse-gkr-add-keyring
 * @short_description: Contains the functions to add a new gnome-keyring keyring
 *
 **/

void             on_add_keyring_name_changed         (GtkEntry *entry,
                                                      gpointer user_data);

void             on_add_keyring_properties_response  (GtkDialog *dialog,
                                                      int response,
                                                      gpointer user_data);

/**
 * keyring_add_done:
 * @result:GNOME_KEYRING_RESULT_CANCELLED or GNOME_KEYRING_RESULT_OK
 * @data: the swidget
 *
 * This callback is called when the new keyring was created. It updates the
 * internal seahorse list of keyrings
 */
static void
on_keyring_create (GObject *source,
                   GAsyncResult *result,
                   gpointer data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (data);
	GError *error = NULL;
	GtkWidget *dialog;

	g_return_if_fail (swidget);

	dialog = seahorse_widget_get_toplevel (swidget);

	/* Clear the operation without cancelling it since it is complete */
	seahorse_gkr_dialog_complete_request (dialog, FALSE);

	secret_collection_create_finish (result, &error);

	/* Successful. Update the listings and stuff. */
	if (error != NULL) {
		seahorse_util_handle_error (&error, seahorse_widget_get_toplevel (swidget),
		                            _("Couldn't add keyring"));
	}

	g_object_unref (swidget);
	seahorse_widget_destroy (swidget);
}

/**
 * on_add_keyring_name_changed:
 * @entry: the entry widget
 * @swidget: the seahorse widget
 *
 * Sets the widget named "ok" in the @swidget to sensitive as soon as the
 * entry @entry contains text
 */
G_MODULE_EXPORT void
on_add_keyring_name_changed (GtkEntry *entry,
                             gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	const gchar *keyring = gtk_entry_get_text (entry);
	seahorse_widget_set_sensitive (swidget, "ok", keyring && keyring[0]);
}

/**
 * on_add_keyring_properties_response:
 * @dialog: ignored
 * @response: the response of the choose-keyring-name dialog
 * @swidget: the seahorse widget
 *
 * Requests a password using #gnome_keyring_create - the name of the keyring
 * was already entered by the user. The password will be requested in a window
 * provided by gnome-keyring
 */
G_MODULE_EXPORT void
on_add_keyring_properties_response (GtkDialog *dialog,
                                    int response,
                                    gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	GCancellable *cancellable;
	GtkEntry *entry;
	const gchar *keyring;
	GtkWidget *widget;

	if (response == GTK_RESPONSE_HELP) {
		seahorse_widget_show_help (swidget);
		
	} else if (response == GTK_RESPONSE_ACCEPT) {
	    
		entry = GTK_ENTRY (seahorse_widget_get_widget (swidget, "keyring-name"));
		g_return_if_fail (entry); 

		keyring = gtk_entry_get_text (entry);
		g_return_if_fail (keyring && keyring[0]);

		widget = seahorse_widget_get_toplevel (swidget);
		cancellable = seahorse_gkr_dialog_begin_request (widget);

		secret_collection_create (seahorse_gkr_backend_get_service (NULL),
		                          keyring, NULL, SECRET_COLLECTION_NONE,
		                          cancellable, on_keyring_create,
		                          g_object_ref (swidget));
		g_object_unref (cancellable);

	} else {
		seahorse_widget_destroy (swidget);
	}
}

/**
 * seahorse_gkr_add_keyring_show:
 * @parent: the parent widget of the new window
 *
 * Displays the window requesting the keyring name. Starts the whole
 * "create a new keyring" process.
 */
void
seahorse_gkr_add_keyring_show (GtkWindow *parent)
{
	SeahorseWidget *swidget = NULL;
	GtkWidget *widget;
	GtkEntry *entry;

	swidget = seahorse_widget_new_allow_multiple ("add-keyring", parent);
	g_return_if_fail (swidget);

	entry = GTK_ENTRY (seahorse_widget_get_widget (swidget, "keyring-name"));
	g_return_if_fail (entry); 
	on_add_keyring_name_changed (entry, swidget);

	widget = seahorse_widget_get_toplevel (swidget);
	
	gtk_widget_show (widget);
	gtk_window_present (GTK_WINDOW (widget));
}
