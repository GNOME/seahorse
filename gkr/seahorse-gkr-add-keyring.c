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

static void
update_wait_cursor (GtkWidget *dialog, gpointer unused)
{
	GdkCursor *cursor;
    
	g_return_if_fail (dialog->window);
    
	/* No request active? */
	if (!g_object_get_data (G_OBJECT (dialog), "keyring-request")) {
		gdk_window_set_cursor (dialog->window, NULL);
		return;
	}
    
	/* 
	 * Get the wait cursor. Create a new one and cache it on the widget 
	 * if first time.
	 */
	cursor = (GdkCursor*)g_object_get_data (G_OBJECT (dialog), "wait-cursor");
	if (!cursor) {
		cursor = gdk_cursor_new (GDK_WATCH);
		g_object_set_data_full (G_OBJECT (dialog), "wait-cursor", cursor, 
		                        (GDestroyNotify)gdk_cursor_unref);
	}
    
	/* Indicate that we're loading stuff */
	gdk_window_set_cursor (dialog->window, cursor);
}

static void
clear_request (SeahorseWidget *swidget, gboolean cancel)
{
	GtkWidget *dialog;
	gpointer request;
    
	dialog = seahorse_widget_get_toplevel (swidget);
	g_return_if_fail (GTK_IS_WIDGET (dialog));

	request = g_object_steal_data (G_OBJECT (dialog), "keyring-request");
	if (request && cancel)
		gnome_keyring_cancel_request (request);
        
	if (GTK_WIDGET_REALIZED (dialog))
		update_wait_cursor (dialog, NULL);
	gtk_widget_set_sensitive (dialog, TRUE);
}

static void
setup_request (SeahorseWidget *swidget, gpointer request)
{
	GtkWidget *dialog;
	
	g_return_if_fail (request);
    
	dialog = seahorse_widget_get_toplevel (swidget);
	g_return_if_fail (GTK_IS_WIDGET (dialog));
    
	/* Cancel any old operation going on */
	clear_request (swidget, TRUE);
    
	/* 
	 * Start the operation and tie it to the widget so that it will get 
	 * cancelled if the widget is destroyed before the operation is complete
	 */ 
	g_object_set_data_full (G_OBJECT (dialog), "keyring-request", request, 
	                        gnome_keyring_cancel_request); 
    
	if (GTK_WIDGET_REALIZED (dialog))
		update_wait_cursor (dialog, NULL);
	else
		g_signal_connect (dialog, "realize", G_CALLBACK (update_wait_cursor), dialog);
    
	gtk_widget_set_sensitive (dialog, FALSE);    
}

static void
keyring_add_done (GnomeKeyringResult result, gpointer data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (data);
	SeahorseOperation *op;

	g_return_if_fail (swidget);

	/* Clear the operation without cancelling it since it is complete */
	clear_request (swidget, FALSE);
    
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

static void
keyring_name_changed (GtkEntry *entry, SeahorseWidget *swidget)
{
	const gchar *keyring = gtk_entry_get_text (entry);
	seahorse_widget_set_sensitive (swidget, "ok", keyring && keyring[0]);
}

static void
properties_response (GtkDialog *dialog, int response, SeahorseWidget *swidget)
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
		setup_request (swidget, request);
		
	} else {
		seahorse_widget_destroy (swidget);
	}
}

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

	glade_xml_signal_connect_data (swidget->xml, "keyring_name_changed", 
	                               G_CALLBACK (keyring_name_changed), swidget);
	keyring_name_changed (entry, swidget);

	widget = seahorse_widget_get_toplevel (swidget);
	g_signal_connect (widget, "response", G_CALLBACK (properties_response), swidget);
	
	gtk_widget_show (widget);
	gtk_window_present (GTK_WINDOW (widget));
}
