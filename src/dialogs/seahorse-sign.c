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

#define INDEX "index"

static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	guint index;
	SeahorseSignCheck check;
	SeahorseSignOptions options = 0;
	
	index = (guint)g_object_steal_data (G_OBJECT (swidget), INDEX);
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
	
	seahorse_ops_key_sign (swidget->sctx, SEAHORSE_KEY_WIDGET (swidget)->skey,
		index, check, options);
	seahorse_widget_destroy (swidget);
}

void
seahorse_sign_new (SeahorseContext *sctx, SeahorseKey *skey, const guint index)
{
	SeahorseWidget *swidget;
	
	swidget = seahorse_key_widget_new ("sign", sctx, skey);
	g_return_if_fail (swidget != NULL);
	
	g_object_set_data (G_OBJECT (swidget), INDEX, (gpointer)index);
	
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		G_CALLBACK (ok_clicked), swidget);
}
