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

#include "seahorse-key-dialogs.h"
#include "seahorse-widget.h"
#include "seahorse-ops-key.h"

static void
pass_changed (GtkEditable *editable, SeahorseWidget *swidget)
{
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "ok"),
		strlen (gtk_editable_get_chars (editable, 0, -1)) > 0);
}

const gchar*
seahorse_passphrase_get (SeahorseContext *sctx, const gchar *desc, gpointer *data)
{
	SeahorseWidget *swidget;
	gint response;
	gchar *pass, **split_line, **split_uid, *label;
	
	if (desc) {
		swidget = seahorse_widget_new ("passphrase", sctx);
		
		if (swidget == NULL) {
			gpgme_cancel (sctx->ctx);
			return NULL;
		}
		
		glade_xml_signal_connect_data (swidget->xml, "pass_changed",
			G_CALLBACK (pass_changed), swidget);
		
		split_line = g_strsplit (desc, "\n", 3);
		split_uid = g_strsplit (split_line[1], " ", 2);
		if (g_str_equal (split_line[0], "ENTER"))
			label = g_strdup_printf (_("Enter passphrase for %s"), split_uid[1]);
		else {
			gtk_image_set_from_stock (GTK_IMAGE (glade_xml_get_widget (
				swidget->xml, "image")), GTK_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_DIALOG);
			label = g_strdup_printf (_("Bad passphrase! Try again for %s"), split_uid[1]);
		}
		
		gtk_label_set_text (GTK_LABEL (glade_xml_get_widget (swidget->xml,
			"description")), label);
		
		response = gtk_dialog_run (GTK_DIALOG (glade_xml_get_widget (
			swidget->xml, swidget->name)));
		pass = g_strdup (gtk_entry_get_text (GTK_ENTRY (
			glade_xml_get_widget (swidget->xml, "pass"))));

		seahorse_widget_destroy (swidget);
		g_free (label);
		g_free (split_uid);
		g_free (split_line);
		
		if (response != GTK_RESPONSE_OK) {
			gpgme_cancel (sctx->ctx);
			g_free (pass);
			return "";
		}
		
		*data = pass;
		return pass;
	}
	else if (*data) {
		g_free (*data);
		*data = NULL;
	}
	
	return NULL;
}

static void
entry_changed (GtkEditable *editable, SeahorseWidget *swidget)
{
	gchar *pass, *confirm;
	
	pass = gtk_editable_get_chars (GTK_EDITABLE (glade_xml_get_widget (
		swidget->xml, "pass")), 0, -1);
	confirm = gtk_editable_get_chars (GTK_EDITABLE (glade_xml_get_widget (
		swidget->xml, "confirm")), 0, -1);
	
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "ok"),
		strlen (pass) > 0 && g_str_equal (pass, confirm));
}

const gchar*
seahorse_change_passphrase_get (SeahorseContext *sctx, const gchar *desc, gpointer *data)
{
	SeahorseWidget *swidget;
	gint response;
	gchar *pass;
	
	if (desc) {
		swidget = seahorse_widget_new ("change-passphrase", sctx);
		
		if (swidget == NULL) {
			gpgme_cancel (sctx->ctx);
			return NULL;
		}
		
		glade_xml_signal_connect_data (swidget->xml, "entry_changed",
			G_CALLBACK (entry_changed), swidget);
		
		response = gtk_dialog_run (GTK_DIALOG (glade_xml_get_widget (
			swidget->xml, swidget->name)));
		pass = g_strdup (gtk_entry_get_text (GTK_ENTRY (glade_xml_get_widget (
			swidget->xml, "pass"))));
		seahorse_widget_destroy (swidget);
		
		if (response != GTK_RESPONSE_OK) {
			gpgme_cancel (sctx->ctx);
			g_free (pass);
			return "";
		}
		
		*data = pass;
		return pass;
	}
	else if (*data) {
		g_free (*data);
		*data = NULL;
	}
	
	return NULL;
}
