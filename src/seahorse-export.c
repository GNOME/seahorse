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

#define EXPORT_FILE "file"
#define OK "ok"

static GpgmeRecipients recipients = NULL;

/* Export to file toggled */
static void
file_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	gboolean active;
	GtkWidget *entry;
	
	active = gtk_toggle_button_get_active (togglebutton);
	entry = glade_xml_get_widget (swidget->xml, EXPORT_FILE);
	
	gtk_widget_set_sensitive (entry, active);
	
	/* Set OK sensitive if deselecting File or if selecting File and
	 * there is some kind of file name */
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, OK), !active ||
		(gnome_file_entry_get_full_path (GNOME_FILE_ENTRY (entry), FALSE) != NULL));
}

static void
file_changed (GtkEditable *editable, SeahorseWidget *swidget)
{
	/* Set OK sensitive if some kind of filename exists */
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, OK),
		gnome_file_entry_get_full_path (GNOME_FILE_ENTRY (
		glade_xml_get_widget (swidget->xml, EXPORT_FILE)), FALSE) != NULL);
}

static void
clipboard_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, OK),
		gtk_toggle_button_get_active (togglebutton));
}

/* Exports key to data source */
static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	GtkToggleButton *file_toggle;
	GpgmeError err;
	
	file_toggle = GTK_TOGGLE_BUTTON (glade_xml_get_widget (swidget->xml, "file_toggle"));
	
	/* Export to file */
	if (gtk_toggle_button_get_active (file_toggle)) {
		GtkWidget *export_file;
		const gchar *path;
		
		export_file = glade_xml_get_widget (swidget->xml, EXPORT_FILE);
		path = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY (export_file), FALSE);
		
		seahorse_op_export_file (swidget->sctx, path, recipients, &err);
	}
	/* Export to clipboard */
	else {
		GdkAtom atom;
		GtkClipboard *board;
		gchar *text;
		
		text = seahorse_op_export_text (swidget->sctx, recipients, &err);
		
		if (text != NULL) {
			atom = gdk_atom_intern ("CLIPBOARD", FALSE);
			board = gtk_clipboard_get (atom);
			gtk_clipboard_set_text (board, text, strlen (text));
			g_free (text);
		}
	}
	
	if (err != GPGME_No_Error)
		seahorse_util_handle_error (err);
	else
		seahorse_widget_destroy (swidget);
}

/**
 * seahorse_export_new:
 * @sctx: Current #SeahorseContext
 * @recips: Keys to export
 *
 * Creates a new #SeahorseKeyWidget dialog for exporting @recips.
 **/
void
seahorse_export_show (SeahorseContext *sctx, GpgmeRecipients recips)
{
	SeahorseWidget *swidget;
	
	swidget = seahorse_widget_new ("export", sctx);
	g_return_if_fail (swidget != NULL);
	
	recipients = recips;
	
	glade_xml_signal_connect_data (swidget->xml, "file_toggled",
		G_CALLBACK (file_toggled), swidget);
	glade_xml_signal_connect_data (swidget->xml, "clipboard_toggled",
		G_CALLBACK (clipboard_toggled), swidget);
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		G_CALLBACK (ok_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "file_changed",
		G_CALLBACK (file_changed), swidget);
}
