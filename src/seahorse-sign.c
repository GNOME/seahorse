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
#include "seahorse-key-widget.h"
#include "seahorse-key-op.h"
#include "seahorse-util.h"

static gboolean
ok_clicked (SeahorseWidget *swidget)
{
	SeahorseKeyWidget *skwidget;
	SeahorseSignCheck check;
	SeahorseSignOptions options = 0;
	gpgme_error_t err;
	
	skwidget = SEAHORSE_KEY_WIDGET (swidget);
	
	check = gtk_option_menu_get_history (GTK_OPTION_MENU (
		glade_xml_get_widget (swidget->xml, "checked")));
	/* get local option */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (
	glade_xml_get_widget (swidget->xml, "local"))))
		options = options | SIGN_LOCAL;
	/* get revoke option */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (
	glade_xml_get_widget (swidget->xml, "revocable"))))
		options = options | SIGN_NO_REVOKE;
	/* get expires option */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (
	glade_xml_get_widget (swidget->xml, "expires"))))
		options = options | SIGN_EXPIRES;
	
	err = seahorse_key_op_sign (skwidget->skey, skwidget->index, check, options);
	if (!GPG_IS_OK (err)) {
		seahorse_util_handle_error (err, _("Couldn't sign key"));
		return FALSE;
	}
	else {
		seahorse_widget_destroy (swidget);
		return TRUE;
	}
}

void
seahorse_sign_show (SeahorseContext *sctx, GList *keys)
{
	GtkWidget *question;
	gint response;
	GList *list = NULL;
	SeahorseKey *skey;
	SeahorseWidget *swidget;
	gboolean do_sign = TRUE;
	
	for (list = keys; list != NULL; list = g_list_next (list)) {
		skey = list->data;
		
		question = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
			_("Are you sure you want to sign all user IDs for %s?"),
			seahorse_key_get_keyid (skey, 0));
		response = gtk_dialog_run (GTK_DIALOG (question));
		gtk_widget_destroy (question);
		
		if (response != GTK_RESPONSE_YES)
			break;
		
		swidget = seahorse_key_widget_new_with_index ("sign", sctx, skey, 0);
		g_return_if_fail (swidget != NULL);
		
		while (do_sign) {
			response = gtk_dialog_run (GTK_DIALOG (
				glade_xml_get_widget (swidget->xml, swidget->name)));\
			switch (response) {
				case GTK_RESPONSE_HELP:
					break;
				case GTK_RESPONSE_OK:
					do_sign = !ok_clicked (swidget);
					break;
				default:
					do_sign = FALSE;
					seahorse_widget_destroy (swidget);
					break;
			}
		}
		do_sign = TRUE;
	}
}

void
seahorse_sign_uid_show (SeahorseContext *sctx, SeahorseKey *skey, const guint uid)
{
    GtkWidget *question;
    gint response;
    SeahorseWidget *swidget;
    gboolean do_sign = TRUE;
    gchar *userid;
    
    /* UIDs are one based ... */
    g_return_if_fail (uid > 0);
   
    /* ... Except for when calling this, which is messed up */
    userid = seahorse_key_get_userid (skey, uid - 1);     
    question = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
                        GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
                        _("Are you sure you want to sign the '%s' user ID?"),
                        userid);
    g_free (userid);

    response = gtk_dialog_run (GTK_DIALOG (question));
    gtk_widget_destroy (question);
     
    if (response != GTK_RESPONSE_YES)
        return;
     
    swidget = seahorse_key_widget_new_with_index ("sign", sctx, skey, uid);
    g_return_if_fail (swidget != NULL);
        
    while (do_sign) {
        response = gtk_dialog_run (GTK_DIALOG (
                glade_xml_get_widget (swidget->xml, swidget->name)));\
        switch (response) {
        case GTK_RESPONSE_HELP:
            break;
        case GTK_RESPONSE_OK:
            do_sign = !ok_clicked (swidget);
            break;
        default:
            do_sign = FALSE;
            seahorse_widget_destroy (swidget);
            break;
        }
    }
}
