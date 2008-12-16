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

#include "config.h" 

#include <string.h>

#include <glib/gi18n.h>
 
#include "seahorse-object-widget.h"
#include "seahorse-util.h"

#include "seahorse-gpgme-dialogs.h"
#include "seahorse-gpgme-key-op.h"

#define NAME "name"
#define EMAIL "email"

static void
check_ok (SeahorseWidget *swidget)
{
	const gchar *name, *email;
	
	/* must be at least 5 characters */
	name = gtk_entry_get_text (GTK_ENTRY (
		glade_xml_get_widget (swidget->xml, NAME)));
	/* must be empty or be *@* */
	email = gtk_entry_get_text (GTK_ENTRY (
		glade_xml_get_widget (swidget->xml, EMAIL)));
	
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "ok"),
		strlen (name) >= 5 && (strlen (email) == 0  ||
		(g_pattern_match_simple ("?*@?*", email))));
}

static void
name_changed (GtkEditable *editable, SeahorseWidget *swidget)
{
	check_ok (swidget);
}

static void
email_changed (GtkEditable *editable, SeahorseWidget *swidget)
{
	check_ok (swidget);
}

static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	SeahorseObject *object;
	const gchar *name, *email, *comment;
	gpgme_error_t err;
	
	object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
	
	name = gtk_entry_get_text (GTK_ENTRY (
		glade_xml_get_widget (swidget->xml, NAME)));
	email = gtk_entry_get_text (GTK_ENTRY (
		glade_xml_get_widget (swidget->xml, EMAIL)));
	comment = gtk_entry_get_text (GTK_ENTRY (
		glade_xml_get_widget (swidget->xml, "comment")));
	
	err = seahorse_gpgme_key_op_add_uid (SEAHORSE_GPGME_KEY (object),
	                                     name, email, comment);
	if (!GPG_IS_OK (err))
		seahorse_gpgme_handle_error (err, _("Couldn't add user id"));
	else
		seahorse_widget_destroy (swidget);
}

/**
 * seahorse_add_uid_new:
 * @skey: #SeahorseKey
 *
 * Creates a new #SeahorseKeyWidget dialog for adding a user ID to @skey.
 **/
void
seahorse_gpgme_add_uid_new (SeahorseGpgmeKey *pkey, GtkWindow *parent)
{
	SeahorseWidget *swidget;
	const gchar *userid;
	
	swidget = seahorse_object_widget_new ("add-uid", parent, SEAHORSE_OBJECT (pkey));
	g_return_if_fail (swidget != NULL);
	
	userid = seahorse_object_get_label (SEAHORSE_OBJECT (pkey));
	gtk_window_set_title (GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)),
		g_strdup_printf (_("Add user ID to %s"), userid));
  
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		G_CALLBACK (ok_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "name_changed",
		G_CALLBACK (name_changed), swidget);
	glade_xml_signal_connect_data (swidget->xml, "email_changed",
		G_CALLBACK (email_changed), swidget);
}
