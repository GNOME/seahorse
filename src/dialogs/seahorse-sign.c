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

#define EXPIRES "expires"

static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	SeahorseKeyWidget *skwidget;
	SeahorseSignCheck check;
	SeahorseSignOptions options = 0;
	GtkWidget *dialog;
	gchar *info;
	
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
	/* if sign successful */
	if (seahorse_ops_key_sign (swidget->sctx, skwidget->skey, skwidget->index, check, options)) {
		seahorse_widget_destroy (swidget);
		/* show status */
		if (skwidget->index == 0)
			info = g_strdup_printf (_("Successfully signed all user IDs for key %s"),
				seahorse_key_get_keyid (skwidget->skey, 0));
		else
			info = g_strdup_printf (_("Successfully signed user ID %s for key %s"),
				seahorse_key_get_userid (skwidget->skey, skwidget->index-1),
				seahorse_key_get_keyid (skwidget->skey, 0));
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
			GTK_MESSAGE_INFO, GTK_BUTTONS_OK, info);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
	else
		seahorse_util_show_error (GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)),
			_("Signing failed\n"));
}

/**
 * seahorse_sign_new:
 * @sctx: Current #SeahorseContext
 * @skey: #SeahorseKey to sign
 * @index: Index of user ID in @skey to sign, 0 being all user IDs
 *
 * Asks user if sure about signing user ID(s),
 * then presents a dialog for signing the key.
 **/
void
seahorse_sign_new (SeahorseContext *sctx, SeahorseKey *skey, const guint index)
{
	SeahorseWidget *swidget;
	GtkWidget *dialog;
	gchar *question;
	gint response;
	
	/* ask question */
	if (index == 0)
		question = g_strdup_printf (_("Are you sure you want to sign all user IDs for key %s?"),
			seahorse_key_get_keyid (skey, 0));
	else
		question = g_strdup_printf (_("Are you sure you want to sign user ID %s for key %s?"),
			seahorse_key_get_userid (skey, index - 1), seahorse_key_get_keyid (skey, 0));
	
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
		GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, question);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	
	if (response != GTK_RESPONSE_YES)
		return;
	/* do widget */
	swidget = seahorse_key_widget_new_with_index ("sign", sctx, skey, index);
	g_return_if_fail (swidget != NULL);
	
	gtk_label_set_text (GTK_LABEL (glade_xml_get_widget (swidget->xml, "key")),
		g_strdup_printf ("%s?", seahorse_key_get_userid (skey, 0)));
	
	if (gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_EXPIRE, NULL, 0))
		gtk_widget_show (glade_xml_get_widget (swidget->xml, EXPIRES));
	
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		G_CALLBACK (ok_clicked), swidget);
}
