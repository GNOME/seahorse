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
#include "seahorse-ops-key.h"
#include "seahorse-util.h"

static void
name_changed (GtkEditable *editable, SeahorseWidget *swidget)
{
	gchar *text;
	
	text = gtk_editable_get_chars (editable, 0, -1);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "ok"),
		strlen (text) >= 5);
}

static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	const gchar *name, *email, *comment;
	//GtkWindow *parent;
	
	//parent = GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name));
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	
	name = gtk_entry_get_text (GTK_ENTRY (
		glade_xml_get_widget (swidget->xml, "name")));
	email = gtk_entry_get_text (GTK_ENTRY (
		glade_xml_get_widget (swidget->xml, "email")));
	comment = gtk_entry_get_text (GTK_ENTRY (
		glade_xml_get_widget (swidget->xml, "comment")));
	
	/* Check entries *
	if (strlen (name) < 5) {
		seahorse_util_show_error (parent,
			_("Name must be at least 5 characters long"));
		return;
	}
	if (!seahorse_ops_key_check_email (email)) {
		seahorse_util_show_error (parent,
			_("You must enter a valid email address"));
		return;
	}*/
	
	seahorse_ops_key_add_uid (swidget->sctx, SEAHORSE_KEY_WIDGET (swidget)->skey,
		name, email, comment);
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
}
