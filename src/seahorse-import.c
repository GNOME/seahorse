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

#include "seahorse-windows.h"
#include "seahorse-widget.h"
#include "seahorse-op.h"
#include "seahorse-util.h"

#define IMPORT_FILE "file"
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

static void
clipboard_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, OK),
		gtk_toggle_button_get_active (togglebutton));
}

static void
clipboard_received (GtkClipboard *board, const gchar *text, SeahorseContext *sctx)
{
	gpgme_error_t err;
	gint keys;
	
	keys = seahorse_op_import_text (sctx, text, &err);
	
	if (!GPG_IS_OK (err))
		seahorse_util_handle_error (err);
	else if (keys > 0)
		seahorse_context_keys_added (sctx, keys);
}

/* Imports keys from specified data source */
static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	GtkWidget *file_toggle;
	gpgme_error_t err;
	gint keys;
	
	file_toggle = glade_xml_get_widget (swidget->xml, "file_toggle");
	
	/* Import from file */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (file_toggle))) {
		GnomeFileEntry *file;
		const gchar *file_string;
		
		file = GNOME_FILE_ENTRY (glade_xml_get_widget (swidget->xml, IMPORT_FILE));
		file_string = gnome_file_entry_get_full_path (file, TRUE);
		
		keys = seahorse_op_import_file (swidget->sctx, file_string, &err);
		if (!GPG_IS_OK (err))
			seahorse_util_handle_error (err);
		else {
			seahorse_widget_destroy (swidget);
			seahorse_context_keys_added (swidget->sctx, keys);
		}
	}
	/* Import from text */
	else {
		GdkAtom atom;
		GtkClipboard *board;
		
		atom = gdk_atom_intern ("CLIPBOARD", FALSE);
		board = gtk_clipboard_get (atom);
		gtk_clipboard_request_text (board,
			(GtkClipboardTextReceivedFunc)clipboard_received, swidget->sctx);
		seahorse_widget_destroy (swidget);
	}
}

void
seahorse_import_show (SeahorseContext *sctx)
{
	SeahorseWidget *swidget;
	
	swidget = seahorse_widget_new ("import", sctx);
	g_return_if_fail (swidget != NULL);
	
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		G_CALLBACK (ok_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "file_toggled",
		G_CALLBACK (file_toggled), swidget);
	glade_xml_signal_connect_data (swidget->xml, "clipboard_toggled",
		G_CALLBACK (clipboard_toggled), swidget);
	glade_xml_signal_connect_data (swidget->xml, "file_changed",
		G_CALLBACK (file_changed), swidget);
}
