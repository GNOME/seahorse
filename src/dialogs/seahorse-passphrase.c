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

#include "seahorse-passphrase.h"
#include "seahorse-key-widget.h"
#include "seahorse-ops-key.h"

#define SPACING 12

/* parts borrowed from gpa's passphrase_cb */
const gchar*
seahorse_passphrase_get (gpointer ctx, const gchar *desc, gpointer *r_hd)
{
	GtkWidget *dialog, *vbox, *description, *entry;
	gint response;
	
	/* If need pass */
	if (desc) {
		dialog = gtk_dialog_new_with_buttons (_("Enter Passphrase"), NULL, GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
			
		vbox = gtk_vbox_new (FALSE, 12);
		gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), vbox);
		gtk_container_set_border_width (GTK_CONTAINER (vbox), SPACING);
			
		description = gtk_label_new (desc);
		gtk_container_add (GTK_CONTAINER (vbox), description);
		
		/* Add pass entry */
		entry = gtk_entry_new ();
		gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);
		gtk_widget_grab_focus (entry);
		gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
		gtk_container_add (GTK_CONTAINER (vbox), entry);
		
		gtk_widget_show_all (dialog);
		
		response = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_hide (dialog);
		
		/* If entered pass */
		if (response == GTK_RESPONSE_OK)
			return (gtk_entry_get_text (GTK_ENTRY (entry)));
		/* Else cancel op */
		else {
			gpgme_cancel (ctx);
			return "";
		}
	}
	else
		return NULL;
}
