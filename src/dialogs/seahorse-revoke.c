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

static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	SeahorseKeyWidget *skwidget;
	guint index;
	SeahorseRevokeReason reason;
	const gchar *description;
	
	skwidget = SEAHORSE_KEY_WIDGET (swidget);
	
	index = skwidget->index;
	
	reason = gtk_option_menu_get_history (GTK_OPTION_MENU (
		glade_xml_get_widget (swidget->xml, "reason")));
	description = gtk_entry_get_text (GTK_ENTRY (
		glade_xml_get_widget (swidget->xml, "description")));
	
	if (skwidget->index != 0)
		seahorse_ops_key_revoke_subkey (swidget->sctx, skwidget->skey,
			skwidget->index, reason, description);
	seahorse_widget_destroy (swidget);
}

void
seahorse_revoke_new (SeahorseContext *sctx, SeahorseKey *skey, const guint index)
{
	SeahorseWidget *swidget;
	gchar *title;
	
	g_return_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx));
	g_return_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey));
	g_return_if_fail (index <= seahorse_key_get_num_subkeys (skey));
	
	swidget = seahorse_key_widget_new_with_index ("revoke", sctx, skey, index);
	g_return_if_fail (swidget != NULL);
	
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		G_CALLBACK (ok_clicked), swidget);
	
	if (index)
		title = g_strdup_printf (_("Revoke Subkey %d of %s"), index,
			seahorse_key_get_userid (skey, 0));
	else
		title = g_strdup_printf (_("Revoke %s"), seahorse_key_get_userid (skey, 0));
	
	gtk_window_set_title (GTK_WINDOW (glade_xml_get_widget (swidget->xml,
		swidget->name)), title);
}

void
seahorse_add_revoker_new (SeahorseContext *sctx, SeahorseKey *skey)
{
	SeahorseKeyPair *skpair;
	GtkWidget *dialog;
	gint response;
	gchar *message;
	
	g_return_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx));
	g_return_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey));
	
	skpair = seahorse_context_get_default_key (sctx);
	g_return_if_fail (skpair != NULL);
	
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
		GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
		_("You are about to add %s as a revoker for %s."
		" This operation cannot be undone! Are you sure you want to continue?"),
		seahorse_key_get_userid (SEAHORSE_KEY (skpair), 0),
		seahorse_key_get_userid (skey, 0));
	
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	
	if (response != GTK_RESPONSE_YES)
		return;
	
	if (seahorse_ops_key_add_revoker (sctx, skey))
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO, GTK_BUTTONS_OK, _("%s can now revoke %s."),
			seahorse_key_get_userid (SEAHORSE_KEY (skpair), 0),
			seahorse_key_get_userid (skey, 0));
	else
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
			_("Could not add %s as a revoker for %s"),
			seahorse_key_get_userid (SEAHORSE_KEY (skpair), 0),
			seahorse_key_get_userid (skey, 0));
	
	g_signal_connect_swapped (GTK_OBJECT (dialog), "response",
		G_CALLBACK (gtk_widget_destroy), GTK_OBJECT (dialog));
	gtk_widget_show (dialog);
}
