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
#include "seahorse-ops-key.h"
#include "seahorse-key-pair.h"

void
seahorse_delete_new (SeahorseContext *sctx, SeahorseKey *skey, const guint index)
{
	GtkWidget *warning;
	gint response;
	const gchar *message, *keyid;
	
	g_return_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx));
	g_return_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey));
	g_return_if_fail (index <= seahorse_key_get_num_subkeys (skey));
	
	if (index == 0)
		message = _("Are you sure you want to permanently delete key %s, %s?");
	else
		message = _("Are you sure you want to permanently delete subkey %s for %s?");
	
	warning = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
		GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, g_strdup_printf (
		message, seahorse_key_get_keyid (skey, index),
		seahorse_key_get_userid (skey, 0)));
	
	response = gtk_dialog_run (GTK_DIALOG (warning));
	gtk_widget_destroy (warning);
	
	if (response != GTK_RESPONSE_YES)
		return;
	/* make sure if pair */
	else if (SEAHORSE_IS_KEY_PAIR (skey)) {
		warning = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
			GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
			_("This key has a secret key! Do you really want to delete it?"));
		response = gtk_dialog_run (GTK_DIALOG (warning));
		gtk_widget_destroy (warning);
		
		if (response != GTK_RESPONSE_YES)
			return;
	}
	
	if (index == 0)
		seahorse_ops_key_delete (sctx, skey);
	else
		seahorse_ops_key_del_subkey (sctx, skey, index);
}
