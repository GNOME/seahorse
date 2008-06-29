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

#include "config.h"

#include <glib/gi18n.h>

#include "seahorse-util.h"

#include "pgp/seahorse-pgp-dialogs.h"
#include "pgp/seahorse-pgp-key-op.h"

static gboolean
ask_key_pair (SeahorseKey *skey)
{
	GtkWidget *warning, *delete_button, *cancel_button;
	gint response;
    gchar *userid;
	
    userid = seahorse_key_get_name (skey, 0);
	warning = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
		GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
		_("%s is a private key. Make sure you have a backup. Do you still want to delete it?"), userid);
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
ask_keys (GList* keys)
{
    GtkWidget *question, *delete_button, *cancel_button;
    gint response = GTK_RESPONSE_ACCEPT;
    gchar *userid;
    guint nkeys;

    // create widgets	
    nkeys = g_list_length (keys);
    if (nkeys == 1) {
        userid = seahorse_key_get_name (SEAHORSE_KEY (keys->data), 0);
        question = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
    	                                   GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                           _("Are you sure you want to permanently delete %s?"), 
                                           userid);
        g_free(userid);
    } else {
        question = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
    	                                   GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                           ngettext ("Are you sure you want to permanently delete %d key?",
                                        	     "Are you sure you want to permanently delete %d keys?", nkeys),
                                           nkeys);
    }

    // add widgets to action area
    delete_button = gtk_button_new_from_stock (GTK_STOCK_DELETE);
    cancel_button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);	
    gtk_dialog_add_action_widget (GTK_DIALOG (question), GTK_WIDGET (cancel_button), GTK_RESPONSE_REJECT);
    gtk_dialog_add_action_widget (GTK_DIALOG (question), GTK_WIDGET (delete_button), GTK_RESPONSE_ACCEPT);
    gtk_widget_show (delete_button); 
    gtk_widget_show (cancel_button);

    // run dialog
    response = gtk_dialog_run (GTK_DIALOG (question));
    gtk_widget_destroy (question);

    return (response == GTK_RESPONSE_ACCEPT);
}

void
seahorse_delete_show (GList *keys)
{
    SeahorseKeySource *sksrc;
    SeahorseKey *skey;
    GError *error = NULL;
    GList *list = NULL;

    g_return_if_fail (g_list_length (keys) > 0);

    if (!ask_keys (keys))
        return;
    
    for (list = keys; list != NULL; list = g_list_next (list)) {
        skey = SEAHORSE_KEY (list->data);

        if (seahorse_key_get_etype (skey) == SKEY_PRIVATE && 
            !ask_key_pair (skey))
            break;

        sksrc = seahorse_key_get_source (skey);
        g_return_if_fail (sksrc != NULL);
            
        if (!seahorse_key_source_remove (sksrc, skey, 0, &error)) {
            seahorse_util_handle_error (error, _("Couldn't delete key"));
            g_clear_error (&error);
        }
    }
}

void
seahorse_delete_subkey_new (SeahorsePGPKey *pkey, guint index)
{
	GtkWidget *question, *delete_button, *cancel_button;
	gint response;
	gpgme_error_t err;
    gchar *userid;
	
    userid = seahorse_key_get_name (SEAHORSE_KEY (pkey), 0);
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
	
	err = seahorse_pgp_key_op_del_subkey (pkey, index);
	if (!GPG_IS_OK (err))
		seahorse_pgp_handle_gpgme_error (err, _("Couldn't delete subkey"));
}

void
seahorse_delete_userid_show (SeahorseKey *skey, guint index)
{
    GtkWidget *question, *delete_button, *cancel_button;
    SeahorseKeySource *sksrc;
    gint response;
    GError *error = NULL;
    gchar *userid;
  
    /* UIDs are one based ... */
    g_return_if_fail (index > 0);
   
    /* ... Except for when calling this, which is messed up */
    userid = seahorse_key_get_name (skey, index - 1);
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
    
    sksrc = seahorse_key_get_source (skey);
    g_return_if_fail (sksrc != NULL);
    
    if (!seahorse_key_source_remove (sksrc, skey, index, &error)) {
        seahorse_util_handle_error (error, _("Couldn't delete user id"));
        g_clear_error (&error);
    }
}
