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
#include <glade/glade-xml.h>

#include "seahorse-file-manager.h"
#include "seahorse-widget.h"
#include "seahorse-preferences.h"
#include "seahorse-ops-file.h"
#include "seahorse-recipients.h"
#include "seahorse-signatures.h"

#define ENTRY "file"

/* Show preferences */
static void
preferences_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_preferences_show (swidget->sctx);
}

/* Returns the current file name */
static gchar*
get_file (SeahorseWidget *swidget, gboolean exists)
{
	GtkWidget *entry;
	
	entry = glade_xml_get_widget (swidget->xml, ENTRY);
	return gnome_file_entry_get_full_path (GNOME_FILE_ENTRY (entry), exists);
}

/* Imports keys in file */
static void
import_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_ops_file_import (swidget->sctx, get_file (swidget, TRUE));
}

/* Exports recipient's keys to file */
static void
export_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseWidget *srecips;

	srecips = seahorse_recipients_new (swidget->sctx, FALSE);
	seahorse_ops_file_export_multiple (swidget->sctx, get_file (swidget, FALSE), seahorse_recipients_run (SEAHORSE_RECIPIENTS (srecips)));
	seahorse_widget_destroy (srecips);
}

/* Signs file */
static void
sign_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_ops_file_sign (swidget->sctx, get_file (swidget, TRUE));
}

/* Encrypt file to recipients */
static void
encrypt_activate (GtkWidget *widget, SeahorseWidget *swidget)
{	
	SeahorseWidget *srecips;

	srecips = seahorse_recipients_new (swidget->sctx, TRUE);
	seahorse_ops_file_encrypt (swidget->sctx, get_file (swidget, TRUE), seahorse_recipients_run (SEAHORSE_RECIPIENTS (srecips)));
	seahorse_widget_destroy (srecips);
}

/* Shows an error dialog about file suffixes */
static void
show_suffix_error (SeahorseWidget *swidget)
{
	seahorse_util_show_error (GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)),
		_("Unknown suffix or bad file name.\nFile must exist and end with '.asc' or '.gpg'."));
}

/* Decrypts file */
static void
decrypt_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	gchar *file;
	
	file = get_file (swidget, TRUE);
	
	if (seahorse_ops_file_check_suffix (file, CRYPT_FILE))
		seahorse_ops_file_decrypt (swidget->sctx, file);
	else
		show_suffix_error (swidget);
}

/* Verifies file */
static void
verify_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	GpgmeSigStat status;
	gchar *file;
	
	file = get_file (swidget, TRUE);
	
	if (seahorse_ops_file_check_suffix (file, SIG_FILE)) {
		seahorse_ops_file_verify (swidget->sctx, file, &status);
		seahorse_signatures_new (swidget->sctx, status, TRUE);
	}
	else
		show_suffix_error (swidget);
}

/* Decrypts and verifies file */
static void
decrypt_verify_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	GpgmeSigStat status;
	gchar *file;
	
	file = get_file (swidget, TRUE);
	
	if (seahorse_ops_file_check_suffix (file, CRYPT_FILE)) {
		seahorse_ops_file_decrypt_verify (swidget->sctx, file, &status);
		seahorse_signatures_new (swidget->sctx, status, TRUE);
	}
	else
		show_suffix_error (swidget);
}

void
seahorse_file_manager_show (SeahorseContext *sctx)
{
	static SeahorseWidget *swidget = NULL;
	
	if (swidget != NULL) {
		gtk_window_present (GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)));
		return;
	}
	
	swidget = seahorse_widget_new_component ("file-manager", sctx);
	
	g_object_add_weak_pointer (G_OBJECT (swidget), (gpointer)&swidget);
	
	glade_xml_signal_connect_data (swidget->xml, "preferences_activate",
		G_CALLBACK (preferences_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "import_activate",
		G_CALLBACK (import_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "export_activate",
		G_CALLBACK (export_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "sign_activate",
		G_CALLBACK (sign_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "encrypt_activate",
		G_CALLBACK (encrypt_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "decrypt_activate",
		G_CALLBACK (decrypt_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "verify_activate",
		G_CALLBACK (verify_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "decrypt_verify_activate",
		G_CALLBACK (decrypt_verify_activate), swidget);
}
