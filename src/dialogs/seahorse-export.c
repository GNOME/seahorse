/*
 * Seahorse
 *
 * Copyright (C) 2002 Jacob Perkins
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

#include <gnome.h>

#include "seahorse-export.h"
#include "seahorse-key-widget.h"
#include "seahorse-ops-file.h"
#include "seahorse-ops-text.h"
#include "seahorse-util.h"

#define EXPORT_FILE "file"
#define EXPORT_SERVER "server"

/* Export to file toggled */
static void
file_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, EXPORT_FILE),
		gtk_toggle_button_get_active (togglebutton));
}

/* Export to server toggled */
static void
server_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, EXPORT_SERVER),
		gtk_toggle_button_get_active (togglebutton));
}

/* Exports key to data source */
static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	SeahorseKeyWidget *skwidget;
	GtkToggleButton *file_toggle;
	GtkToggleButton *server_toggle;
	gboolean success;
	GtkWidget *export_file;
	const gchar *path;
	GtkWindow *window;
	GString *string;
	GtkTextView *text;
	
	skwidget = SEAHORSE_KEY_WIDGET (swidget);
	
	file_toggle = GTK_TOGGLE_BUTTON (
		glade_xml_get_widget (swidget->xml, "file_toggle"));
	server_toggle = GTK_TOGGLE_BUTTON (
		glade_xml_get_widget (swidget->xml, "server_toggle"));
	
	/* Export to file */
	if (gtk_toggle_button_get_active (file_toggle)) {
		export_file = glade_xml_get_widget (swidget->xml, EXPORT_FILE);
		path = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY (export_file), FALSE);
		
		if (seahorse_ops_file_export (swidget->sctx, path, skwidget->skey))
			seahorse_widget_destroy (swidget);
		else {
			window = GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name));
			
			seahorse_util_show_error (window,
				g_strdup_printf (_("Cannot export to \n%s"), path));
		}
	}
	else if (gtk_toggle_button_get_active (server_toggle)) {
		//export to server
	}
	/* Export to text */
	else {
		string = g_string_new ("");
		
		success = seahorse_ops_text_export (swidget->sctx, string, skwidget->skey);
		if (success) {
			text = GTK_TEXT_VIEW (glade_xml_get_widget (swidget->xml, "text"));
			seahorse_util_set_text_view_string (text, string);
		}
	}
}

void
seahorse_export_new (SeahorseContext *sctx, SeahorseKey *skey)
{
	SeahorseWidget *swidget;
	GtkWindow *export_dialog;
	
	swidget = seahorse_key_widget_new ("export", sctx, skey);
	
	glade_xml_signal_connect_data (swidget->xml, "file_toggled", G_CALLBACK (file_toggled), swidget);
	glade_xml_signal_connect_data (swidget->xml, "server_toggled", G_CALLBACK (server_toggled), swidget);	
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked", G_CALLBACK (ok_clicked), swidget);
	
	export_dialog = GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name));
	gtk_window_set_title (export_dialog, g_strdup_printf (_("Export Key: %s"), 
		seahorse_key_get_userid (skey, 0)));
}
