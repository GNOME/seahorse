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
 
#include "seahorse-key-dialogs.h"
#include "seahorse-key-widget.h"
#include "seahorse-key-op.h"
#include "seahorse-util.h"

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
	SeahorseKey *skey;
	const gchar *name, *email, *comment;
	GpgmeError err;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	
	name = gtk_entry_get_text (GTK_ENTRY (
		glade_xml_get_widget (swidget->xml, NAME)));
	email = gtk_entry_get_text (GTK_ENTRY (
		glade_xml_get_widget (swidget->xml, EMAIL)));
	comment = gtk_entry_get_text (GTK_ENTRY (
		glade_xml_get_widget (swidget->xml, "comment")));
	
	err = seahorse_key_pair_op_add_uid (swidget->sctx, SEAHORSE_KEY_PAIR (skey),
		name, email, comment);
	if (err != GPGME_No_Error)
		seahorse_util_handle_error (err);
	else
		seahorse_widget_destroy (swidget);
}

/**
 * seahorse_add_uid_new:
 * @sctx: Current context
 * @skey: #SeahorseKey
 *
 * Creates a new #SeahorseKeyWidget dialog for adding a user ID to @skey.
 **/
void
seahorse_add_uid_new (SeahorseContext *sctx, SeahorseKey *skey)
{
	SeahorseWidget *swidget;
	
	swidget = seahorse_key_widget_new ("add-uid", sctx, skey);
	g_return_if_fail (swidget != NULL);
	
	gtk_window_set_title (GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)),
		g_strdup_printf (_("Add user ID to %s"), seahorse_key_get_userid (skey, 0)));
	
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		G_CALLBACK (ok_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "name_changed",
		G_CALLBACK (name_changed), swidget);
	glade_xml_signal_connect_data (swidget->xml, "email_changed",
		G_CALLBACK (email_changed), swidget);
}
