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
#include <gpgme.h>

#include "seahorse-text-editor.h"
#include "seahorse-ops-text.h"
#include "seahorse-widget.h"
#include "seahorse-preferences.h"
#include "seahorse-util.h"
#include "seahorse-signatures.h"
#include "seahorse-recipients.h"

#define TEXT_VIEW "text"

/* Available ops in text_op */
enum {
	EXPORT,
	SIGN,
	CLEAR_SIGN,
	ENCRYPT,
	DECRYPT,
	VERIFY,
	DECRYPT_VERIFY
};

static void
preferences_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_preferences_show (swidget->sctx);
}

/* Import keys in text */
static void
import_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	GtkWidget *text_view;
	const gchar *text;
	
	text_view = glade_xml_get_widget (swidget->xml, TEXT_VIEW);
	text = seahorse_util_get_text_view_text (GTK_TEXT_VIEW (text_view));
	
	seahorse_ops_text_import (swidget->sctx, text);
}

/* Gets the text view & text it contains, calls the appropriate ops,
 * then replaces the text if necessary */
static void
text_op (gint op, gpointer data, SeahorseWidget *swidget)
{
	GtkTextView *text_view;
	gchar *text;
	GString *string;
        gboolean success = FALSE;
        GpgmeSigStat status;
	
	text_view = GTK_TEXT_VIEW (glade_xml_get_widget (swidget->xml, TEXT_VIEW));
	text = seahorse_util_get_text_view_text (text_view);
	
	if (op == EXPORT)
		string = g_string_new ("");
	else
		string = g_string_new (text);
	
	switch (op) {
		case EXPORT:
			success = seahorse_ops_text_export_multiple (swidget->sctx, string, data);
			break;
		case SIGN:
			success = seahorse_ops_text_sign (swidget->sctx, string);
			break;
		case CLEAR_SIGN:
			success = seahorse_ops_text_clear_sign (swidget->sctx, string);
			break;
		case ENCRYPT:
			success = seahorse_ops_text_encrypt (swidget->sctx, string, data);
			break;
		case DECRYPT:
			success = seahorse_ops_text_decrypt (swidget->sctx, string);
			break;
		case VERIFY:
			success = seahorse_ops_text_verify (swidget->sctx, string, &status);
			seahorse_signatures_new (swidget->sctx, status);
			break;
		case DECRYPT_VERIFY:
			success = seahorse_ops_text_decrypt_verify (swidget->sctx, string, &status);
			seahorse_signatures_new (swidget->sctx, status);
			break;
		default:
			break;
	}
	
	if (success)
		seahorse_util_set_text_view_string (text_view, string);
}

/* Exports (possibly) multiple keys to text */
static void
export_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseWidget *srecips;

	srecips = seahorse_export_recipients_new (swidget->sctx);
	text_op (EXPORT, seahorse_recipients_run (SEAHORSE_RECIPIENTS (srecips)), swidget);
	seahorse_widget_destroy (srecips);
}

/* Signs current text & replaces with signed text */
static void
sign_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	text_op (SIGN, NULL, swidget);
}

/* Same as above, but clear sign */
static void
clear_sign_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	text_op (CLEAR_SIGN, NULL, swidget);
}

/* Encrypts current text to (possibly) multiple recipients,
 * then replaces with encrypted text */
static void
encrypt_activate (GtkWidget *widget, SeahorseWidget *swidget)
{	
	SeahorseWidget *srecips;

	srecips = seahorse_encrypt_recipients_new (swidget->sctx);
	text_op (ENCRYPT, seahorse_recipients_run (SEAHORSE_RECIPIENTS (srecips)), swidget);
	seahorse_widget_destroy (srecips);
}

/* Decrypts current text, replaces with decrypted text */
static void
decrypt_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	text_op (DECRYPT, NULL, swidget);
}

/* Verifies sigs in text, shows status of sig */
static void
verify_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	text_op (VERIFY, NULL, swidget);
}

/* Decrypts & verifies text, replaces with decrypted text & shows status of sigs */
static void
decrypt_verify_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	text_op (DECRYPT_VERIFY, NULL, swidget);
}

/* Populate the text view popup menu with op items */
static void
populate_popup (GtkWidget *widget, GtkMenu *menu, SeahorseWidget *swidget)
{
	GtkWidget *item;
	
	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	
	item = gtk_menu_item_new_with_mnemonic (_("Decrypt and Verify"));
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (decrypt_verify_activate), swidget);
	
	item = gtk_menu_item_new_with_mnemonic (_("_Verify"));
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (verify_activate), swidget);
	
	item = gtk_menu_item_new_with_mnemonic (_("_Decrypt"));
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (decrypt_activate), swidget);
	
	item = gtk_menu_item_new_with_mnemonic (_("_Encrypt"));
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (encrypt_activate), swidget);
	
	item = gtk_menu_item_new_with_mnemonic (_("C_lear Text Sign"));
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (clear_sign_activate), swidget);
	
	item = gtk_menu_item_new_with_mnemonic (_("_Sign"));
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (sign_activate), swidget);
	
	item = gtk_menu_item_new_with_mnemonic (_("_Import"));
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (import_activate), swidget);
	
	gtk_widget_show_all (GTK_WIDGET (menu));
}

void
seahorse_text_editor_show (SeahorseContext *sctx)
{
	SeahorseWidget *swidget;
	
	swidget = seahorse_widget_new ("text-editor", sctx);
	g_return_if_fail (swidget != NULL);
	
	glade_xml_signal_connect_data (swidget->xml, "preferences_activate",
		G_CALLBACK (preferences_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "import_activate",
		G_CALLBACK (import_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "export_activate",
		G_CALLBACK (export_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "sign_activate",
		G_CALLBACK (sign_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "clear_sign_activate",
		G_CALLBACK (clear_sign_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "encrypt_activate",
		G_CALLBACK (encrypt_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "decrypt_activate",
		G_CALLBACK (decrypt_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "verify_activate",
		G_CALLBACK (verify_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "decrypt_verify_activate",
		G_CALLBACK (decrypt_verify_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "populate_popup",
		G_CALLBACK (populate_popup), swidget);
}
