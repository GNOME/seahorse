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

#include "seahorse-gkr-dialogs.h"
#include "seahorse-gkr-keyring.h"
#include "seahorse-gkr-source.h"

#include "seahorse-widget.h"
#include "seahorse-util.h"

#include <glib/gi18n.h>

/**
 * SECTION:seahorse-gkr-add-keyring
 * @short_description: Contains the functions to add a new gnome-keyring keyring
 *
 **/

/**
 * keyring_add_done:
 * @result:GNOME_KEYRING_RESULT_CANCELLED or GNOME_KEYRING_RESULT_OK
 * @data: the swidget
 *
 * This callback is called when the new keyring was created. It updates the
 * internal seahorse list of keyrings
 */
static void
keyring_add_done (GnomeKeyringResult result, gpointer data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (data);
	SeahorseOperation *op;

	g_return_if_fail (swidget);

	/* Clear the operation without cancelling it since it is complete */
	seahorse_gkr_dialog_complete_request (swidget, FALSE);
    
	/* Successful. Update the listings and stuff. */
	if (result == GNOME_KEYRING_RESULT_OK) {
		
		op = seahorse_source_load (SEAHORSE_SOURCE (seahorse_gkr_source_default ()));
		
		/* 
		 * HACK: Major hack alert. This whole area needs some serious refactoring,
		 * so for now we're just going to let any viewers listen in on this
		 * operation like so:
		 */
		g_signal_emit_by_name (seahorse_context_for_app (), "refreshing", op);
		g_object_unref (op);

	/* Setting the default keyring failed */
	} else if (result != GNOME_KEYRING_RESULT_CANCELLED) {     
		seahorse_util_show_error (seahorse_widget_get_toplevel (swidget),
		                          _("Couldn't add keyring"),
		                          gnome_keyring_result_to_message (result));
	}
	
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
on_add_keyring_name_changed (GtkEntry *entry, SeahorseWidget *swidget)
{
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
on_add_keyring_properties_response (GtkDialog *dialog, int response, SeahorseWidget *swidget)
{
	GtkEntry *entry;
	const gchar *keyring;
	gpointer request;
	
	if (response == GTK_RESPONSE_HELP) {
		seahorse_widget_show_help (swidget);
		
	} else if (response == GTK_RESPONSE_ACCEPT) {
	    
		entry = GTK_ENTRY (seahorse_widget_get_widget (swidget, "keyring-name"));
		g_return_if_fail (entry); 

		keyring = gtk_entry_get_text (entry);
		g_return_if_fail (keyring && keyring[0]);
	    
		request = gnome_keyring_create (keyring, NULL, keyring_add_done, g_object_ref (swidget), g_object_unref);
		g_return_if_fail (request);
		seahorse_gkr_dialog_begin_request (swidget, request);
		
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
