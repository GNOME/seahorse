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

#include "seahorse-windows.h"
#include "seahorse-key-op.h"
#include "seahorse-util.h"

static gboolean
ask_key_pair (SeahorseKeyPair *skpair)
{
	GtkWidget *warning;
	gint response;
	
	warning = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
		GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
		_("%s is a key pair! Do you still want to delete it?"),
		seahorse_key_get_userid (SEAHORSE_KEY (skpair), 0));
	response = gtk_dialog_run (GTK_DIALOG (warning));
	gtk_widget_destroy (warning);
	return (response == GTK_RESPONSE_YES);
}

static gboolean
ask_key (SeahorseKey *skey)
{
	GtkWidget *question;
	gint response;
	
	question = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
		GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
		_("Are you sure you want to permanently delete %s?"),
		seahorse_key_get_userid (skey, 0));
	response = gtk_dialog_run (GTK_DIALOG (question));
	gtk_widget_destroy (question);
	
	if (response == GTK_RESPONSE_YES) {
		if (SEAHORSE_IS_KEY_PAIR (skey))
			return ask_key_pair (SEAHORSE_KEY_PAIR (skey));
		else
			return TRUE;
	}
	else
		return FALSE;
}

void
seahorse_delete_show (SeahorseContext *sctx, GList *keys)
{
	GtkWidget *warning;
	const gchar *message;
	gint length;
	SeahorseKey *skey;
	GpgmeError err;
	GList *list = NULL;
	
	length = g_list_length (keys);
	g_return_if_fail (length > 0);
	
	if (length == 1) {
		skey = keys->data;
		if (ask_key (skey)) {
			if (SEAHORSE_IS_KEY_PAIR (skey))
				err = seahorse_key_pair_op_delete (sctx, SEAHORSE_KEY_PAIR (skey));
			else
				err = seahorse_key_op_delete (sctx, skey);
		}
	}
	else {
		for (list = keys; list != NULL; list = g_list_next (list)) {
			skey = list->data;
			if (ask_key (skey)) {
				if (SEAHORSE_IS_KEY_PAIR (skey))
					err = seahorse_key_pair_op_delete (sctx, SEAHORSE_KEY_PAIR (skey));
				else
					err = seahorse_key_op_delete (sctx, skey);
			}
			else
				break;
		}
	}
	
	g_list_free (keys);
	if (err != GPGME_No_Error)
		seahorse_util_handle_error (err);
}

void
seahorse_delete_subkey_new (SeahorseContext *sctx, SeahorseKey *skey, const guint index)
{
	GtkWidget *question;
	gint response;
	GpgmeError err;
	
	question = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
		GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
		_("Are you sure you want to permanently delete subkey %d of %s?"),
		index, seahorse_key_get_userid (skey, 0));
	response = gtk_dialog_run (GTK_DIALOG (question));
	gtk_widget_destroy (question);
	
	if (response != GTK_RESPONSE_YES)
		return;
	
	err = seahorse_key_op_del_subkey (sctx, skey, index);
	if (err != GPGME_No_Error)
		seahorse_util_handle_error (err);
}
