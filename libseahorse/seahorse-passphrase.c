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
#include <glade/glade-xml.h>

#include "seahorse-gpgmex.h"
#include "seahorse-libdialogs.h"
#include "seahorse-widget.h"
#include "seahorse-util.h"

static void
pass_changed (GtkEditable *editable, SeahorseWidget *swidget)
{
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "ok"),
		strlen (gtk_editable_get_chars (editable, 0, -1)) > 0);
}

gpgme_error_t
seahorse_passphrase_get (SeahorseContext *sctx, const gchar *passphrase_hint, 
                            const char* passphrase_info, int flags, int fd)
{
	SeahorseWidget *swidget;
    GtkWidget *widget;
	gint response;
    gpgme_error_t err;
	gchar *pass, **split_uid, *label;
	
	swidget = seahorse_widget_new ("passphrase", sctx);
	g_return_val_if_fail (swidget != NULL, GPG_E (GPG_ERR_GENERAL));
  
	glade_xml_signal_connect_data (swidget->xml, "pass_changed",
		G_CALLBACK (pass_changed), swidget);
     
    if (passphrase_info && strlen(passphrase_info) < 16)
        flags |= SEAHORSE_PASS_NEW;        

	split_uid = g_strsplit (passphrase_hint, " ", 2);

    if (flags & SEAHORSE_PASS_BAD) {
        widget = glade_xml_get_widget (swidget->xml, "image");
		gtk_image_set_from_stock (GTK_IMAGE (widget), GTK_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_DIALOG);
		label = g_strdup_printf (_("Bad passphrase! Try again for '%s'"), split_uid[1]);
    } else if (flags & SEAHORSE_PASS_NEW) {
        label = g_strdup_printf (_("Enter new passphrase for '%s'"), split_uid[1]);
    } else {
        label = g_strdup_printf (_("Enter passphrase for '%s'"), split_uid[1]);
    }   
		
    widget = glade_xml_get_widget (swidget->xml, "description");
	gtk_label_set_text (GTK_LABEL (widget), label);
    g_free (label);     
		
    widget = glade_xml_get_widget (swidget->xml, swidget->name);
	response = gtk_dialog_run (GTK_DIALOG (widget));

    widget = glade_xml_get_widget (swidget->xml, "pass");
    pass = g_strdup (gtk_entry_get_text (GTK_ENTRY (widget)));
    seahorse_widget_destroy (swidget);
    
    if (response == GTK_RESPONSE_OK) {
        seahorse_util_printf_fd (fd, "%s\n", pass);
        err = GPG_OK;
    } else {
        err = GPG_E (GPG_ERR_CANCELED);
    }
    
    g_free (pass);
    return err;
}
