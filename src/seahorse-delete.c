/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins, Adam Schreiber
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

#include "seahorse-gpgmex.h"
#include "seahorse-windows.h"
#include "seahorse-key-op.h"
#include "seahorse-util.h"

static gboolean
ask_key_pair (SeahorseKeyPair *skpair)
{
	GtkWidget *warning, *delete_button, *cancel_button;
	gint response;
    gchar *userid;
	
    userid = seahorse_key_get_userid (SEAHORSE_KEY (skpair), 0);
	warning = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
		GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
		_("%s is a key pair! Do you still want to delete it?"), userid);
    g_free (userid);
    
	delete_button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	cancel_button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	
	//add widgets to action area
	gtk_dialog_add_action_widget(GTK_DIALOG(warning), GTK_WIDGET (cancel_button), GTK_RESPONSE_REJECT);
	gtk_dialog_add_action_widget(GTK_DIALOG(warning), GTK_WIDGET (delete_button), GTK_RESPONSE_ACCEPT);
	
	//show widgets
	gtk_widget_show (delete_button);
	gtk_widget_show (cancel_button);
	
	response = gtk_dialog_run (GTK_DIALOG (warning));
	gtk_widget_destroy (warning);
	
	return (response == GTK_RESPONSE_ACCEPT);
}

static gboolean
ask_key (SeahorseKey *skey)
{
	GtkWidget *question, *delete_button, *cancel_button;
	gint response;
    gchar *userid;

	//create widgets	
    userid = seahorse_key_get_userid (skey, 0);
	question = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
		GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
		_("Are you sure you want to permanently delete %s?"), userid);
    g_free(userid);
	delete_button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	cancel_button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	
	
	//add widgets to action area
	gtk_dialog_add_action_widget(GTK_DIALOG(question), GTK_WIDGET (cancel_button), GTK_RESPONSE_REJECT);
	gtk_dialog_add_action_widget(GTK_DIALOG(question), GTK_WIDGET (delete_button), GTK_RESPONSE_ACCEPT);
	
	//show widgets
	gtk_widget_show (delete_button);
	gtk_widget_show (cancel_button);

	//run dialog
	response = gtk_dialog_run (GTK_DIALOG (question));
	gtk_widget_destroy (question);

	if (response == GTK_RESPONSE_ACCEPT) {
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
	SeahorseKey *skey;
	gpgme_error_t err;
	GList *list = NULL;
	
	g_return_if_fail (g_list_length (keys) > 0);
	
	for (list = keys; list != NULL; list = g_list_next (list)) {
		skey = list->data;
		if (ask_key (skey)) {
			if (SEAHORSE_IS_KEY_PAIR (skey))
				err = seahorse_key_pair_op_delete (SEAHORSE_KEY_PAIR (skey));
			else
				err = seahorse_key_op_delete (skey);
			
			if (!GPG_IS_OK (err))
				seahorse_util_handle_gpgme (err, _("Couldn't delete key"));
		}
		else
			break;
	}
}

void
seahorse_delete_subkey_new (SeahorseContext *sctx, SeahorseKey *skey, const guint index)
{
	GtkWidget *question, *delete_button, *cancel_button;
	gint response;
	gpgme_error_t err;
    gchar *userid;
	
    userid = seahorse_key_get_userid (skey, 0);
	question = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
		GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
		_("Are you sure you want to permanently delete subkey %d of %s?"),
		index, userid);
    g_free (userid);
	delete_button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	cancel_button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);

	//add widgets to action area
	gtk_dialog_add_action_widget(GTK_DIALOG(question), GTK_WIDGET (cancel_button), GTK_RESPONSE_REJECT);
	gtk_dialog_add_action_widget(GTK_DIALOG(question), GTK_WIDGET (delete_button), GTK_RESPONSE_ACCEPT);
	
	//show widgets
	gtk_widget_show (delete_button);
	gtk_widget_show (cancel_button);
		
	response = gtk_dialog_run (GTK_DIALOG (question));
	gtk_widget_destroy (question);
	
	if (response != GTK_RESPONSE_ACCEPT)
		return;
	
	err = seahorse_key_op_del_subkey (skey, index);
	if (!GPG_IS_OK (err))
		seahorse_util_handle_gpgme (err, _("Couldn't delete subkey"));
}

void
seahorse_delete_userid_show (SeahorseContext *sctx, SeahorseKey *skey, const guint index)
{
    GtkWidget *question, *delete_button, *cancel_button;
    gint response;
    gpgme_error_t err;
    gchar *userid;
  
    /* UIDs are one based ... */
    g_return_if_fail (index > 0);
   
    /* ... Except for when calling this, which is messed up */
    userid = seahorse_key_get_userid (skey, index - 1);
    question = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
                        GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                        _("Are you sure you want to permanently delete the '%s' user ID?"),
                        userid);
    g_free (userid);
    
    delete_button = gtk_button_new_from_stock (GTK_STOCK_DELETE);
    cancel_button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);

    //add widgets to action area
    gtk_dialog_add_action_widget(GTK_DIALOG(question), GTK_WIDGET (cancel_button), GTK_RESPONSE_REJECT);
    gtk_dialog_add_action_widget(GTK_DIALOG(question), GTK_WIDGET (delete_button), GTK_RESPONSE_ACCEPT);
   
    //show widgets
    gtk_widget_show (delete_button);
    gtk_widget_show (cancel_button);
       
    response = gtk_dialog_run (GTK_DIALOG (question));
    gtk_widget_destroy (question);
 
    if (response != GTK_RESPONSE_ACCEPT)
       return;
    
    err = seahorse_key_op_del_uid (skey, index);
    if (!GPG_IS_OK (err))
        seahorse_util_handle_gpgme (err, _("Couldn't delete user id"));
}
