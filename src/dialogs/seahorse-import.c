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
//#define IMPORT_SERVER "server"
#define KEYID "keyid"
#define OK "ok"

static void
file_changed (GtkEditable *editable, SeahorseWidget *swidget)
{
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, OK),
		(gnome_file_entry_get_full_path (GNOME_FILE_ENTRY (editable), TRUE) != NULL));
}

/* Import from file toggled */
static void
file_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	gboolean active;
	GtkWidget *widget;
	
	active = gtk_toggle_button_get_active (togglebutton);
	widget = glade_xml_get_widget (swidget->xml, IMPORT_FILE);
	gtk_widget_set_sensitive (widget, active);
	
	if (active)
		gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, OK),
			(gnome_file_entry_get_full_path (GNOME_FILE_ENTRY (widget), TRUE) != NULL));
}

/* Import from server toggled *
static void
server_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, IMPORT_SERVER),
		gtk_toggle_button_get_active (togglebutton));
	
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, KEYID),
		gtk_toggle_button_get_active (togglebutton));
}*/

static void
text_changed (GtkTextBuffer *buffer, SeahorseWidget *swidget)
{
	GtkTextIter start, end;
	
	gtk_text_buffer_get_bounds (buffer, &start, &end);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, OK),
		strlen (gtk_text_buffer_get_text (buffer, &start, &end, FALSE)) > 0);
}

/* Import from text toggled */
static void
text_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	gboolean active;
	GtkTextView *view;
	
	active = gtk_toggle_button_get_active (togglebutton);
	view = GTK_TEXT_VIEW (glade_xml_get_widget (swidget->xml, IMPORT_TEXT));
	
	gtk_text_view_set_editable (view, active);
	
	if (active)
		gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, OK),
			strlen (seahorse_util_get_text_view_text (view)) > 0);
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
	GtkTextBuffer *buffer;
	
	swidget = seahorse_widget_new ("import", sctx);
	g_return_if_fail (swidget != NULL);
	
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (glade_xml_get_widget (
		swidget->xml, IMPORT_TEXT)));
	g_signal_connect_after (buffer, "changed", G_CALLBACK (text_changed), swidget);
	
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		G_CALLBACK (ok_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "file_toggled",
		G_CALLBACK (file_toggled), swidget);
	//glade_xml_signal_connect_data (swidget->xml, "server_toggled", G_CALLBACK (server_toggled), swidget);
	glade_xml_signal_connect_data (swidget->xml, "text_toggled",
		G_CALLBACK (text_toggled), swidget);
	glade_xml_signal_connect_data (swidget->xml, "file_changed",
		G_CALLBACK (file_changed), swidget);
}
