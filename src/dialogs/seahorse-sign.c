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

#include "seahorse-sign.h"
#include "seahorse-key-widget.h"
#include "seahorse-ops-key.h"

#define EXPIRES "expires"

static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	SeahorseKeyWidget *skwidget;
	SeahorseSignCheck check;
	SeahorseSignOptions options = 0;
	GpgmeError err;
	
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
	
	err = seahorse_ops_key_sign (swidget->sctx, skwidget->skey, skwidget->index, check, options);
	g_print ("%s\n", gpgme_strerror (err));
	seahorse_widget_destroy (swidget);
}

void
seahorse_sign_new (SeahorseContext *sctx, SeahorseKey *skey, const guint index)
{
	SeahorseWidget *swidget;
	
	swidget = seahorse_key_widget_new_with_index ("sign", sctx, skey, index);
	g_return_if_fail (swidget != NULL);
	
	gtk_label_set_text (GTK_LABEL (glade_xml_get_widget (swidget->xml, "key")),
		g_strdup_printf ("%s?", seahorse_key_get_userid (skey, 0)));
	
	if (gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_EXPIRE, NULL, 0))
		gtk_widget_show (glade_xml_get_widget (swidget->xml, EXPIRES));
	
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		G_CALLBACK (ok_clicked), swidget);
}
