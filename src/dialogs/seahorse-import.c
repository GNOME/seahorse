/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#include "seahorse-import.h"
#include "seahorse-widget.h"
#include "seahorse-ops-file.h"
#include "seahorse-ops-text.h"
#include "seahorse-util.h"

#define IMPORT_FILE "file"
#define IMPORT_TEXT "text"
#define IMPORT_SERVER "server"
#define KEYID "keyid"

/* Import from file toggled */
static void
file_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, IMPORT_FILE),
		gtk_toggle_button_get_active (togglebutton));
}

/* Import from server toggled */
static void
server_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, IMPORT_SERVER),
		gtk_toggle_button_get_active (togglebutton));
	
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, KEYID),
		gtk_toggle_button_get_active (togglebutton));
}

/* Import from text toggled */
static void
text_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	gtk_text_view_set_editable (GTK_TEXT_VIEW (glade_xml_get_widget (swidget->xml, IMPORT_TEXT)),
		gtk_toggle_button_get_active (togglebutton));
}

/* Imports keys from specified data source */
static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	GtkWidget *file_toggle;
	GtkWidget *text_toggle;
	
	file_toggle = glade_xml_get_widget (swidget->xml, "file_toggle");
	text_toggle = glade_xml_get_widget (swidget->xml, "text_toggle");
	
	/* Import from file */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (file_toggle))) {
		GnomeFileEntry *file;
		const gchar *file_string;
		
		file = GNOME_FILE_ENTRY (glade_xml_get_widget (swidget->xml, IMPORT_FILE));
		file_string = gnome_file_entry_get_full_path (file, TRUE);
		
		if (seahorse_ops_file_import (swidget->sctx, file_string))
			seahorse_widget_destroy (swidget);
	}
	/* Import from text */
	else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (text_toggle))) {
		GtkTextView *text;
		const gchar *text_string;
		
		text = GTK_TEXT_VIEW (glade_xml_get_widget (swidget->xml, IMPORT_TEXT));
		text_string = seahorse_util_get_text_view_text (text);
		
		if (seahorse_ops_text_import (swidget->sctx, text_string))
			seahorse_widget_destroy (swidget);
	}
}

void
seahorse_import_show (SeahorseContext *sctx)
{
	SeahorseWidget *swidget;
	
	swidget = seahorse_widget_new ("import", sctx);
	g_return_if_fail (swidget != NULL);
	
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked", G_CALLBACK (ok_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "file_toggled", G_CALLBACK (file_toggled), swidget);
	glade_xml_signal_connect_data (swidget->xml, "server_toggled", G_CALLBACK (server_toggled), swidget);
	glade_xml_signal_connect_data (swidget->xml, "text_toggled", G_CALLBACK (text_toggled), swidget);
}
