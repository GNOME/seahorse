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
#include <eel/eel.h>

#include "seahorse-preferences.h"
#include "seahorse-widget.h"
#include "seahorse-signer-menu.h"

static void
button_toggled (GtkToggleButton *togglebutton, gchar *key)
{
	eel_gconf_set_boolean (key, gtk_toggle_button_get_active (togglebutton));
}

static void
key_toolbar_changed (GtkOptionMenu *option, SeahorseWidget *swidget)
{
	gchar *str;
	
	switch (gtk_option_menu_get_history (option)) {
		case 1:
			str = TOOLBAR_BOTH;
			break;
		case 2:
			str = TOOLBAR_BOTH_HORIZ;
			break;
		case 3:
			str = TOOLBAR_ICONS;
			break;
		case 4:
			str = TOOLBAR_TEXT;
			break;
		default:
			str = TOOLBAR_DEFAULT;
			break;
	}
	
	eel_gconf_set_string (TOOLBAR_STYLE_KEY, str);
}

/**
 * seahorse_preferences_show:
 * @sctx: Current #SeahorseContext
 *
 * Creates a new or shows the current preferences dialog.
 **/
void
seahorse_preferences_show (SeahorseContext *sctx)
{	
	SeahorseWidget *swidget;
	GtkWidget *widget;
	gchar *key_style;
	gint history = 0;
	
	g_return_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx));
	
	swidget = seahorse_widget_new ("preferences", sctx);
	g_return_if_fail (swidget != NULL);
	
	/* Do initial states */
	widget = glade_xml_get_widget (swidget->xml, "ascii_armor");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
		gpgme_get_armor (sctx->ctx));
	
	widget = glade_xml_get_widget (swidget->xml, "text_mode");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
		gpgme_get_textmode (sctx->ctx));
	
	seahorse_signer_menu_new (sctx, GTK_OPTION_MENU (
		glade_xml_get_widget (swidget->xml, "default_key")));
	
	key_style = eel_gconf_get_string (TOOLBAR_STYLE_KEY);
	if (g_str_equal (key_style, TOOLBAR_BOTH))
		history = 1;
	else if (g_str_equal (key_style, TOOLBAR_BOTH_HORIZ))
		history = 2;
	else if (g_str_equal (key_style, TOOLBAR_ICONS))
		history = 3;
	else if (g_str_equal (key_style, TOOLBAR_TEXT))
		history = 4;
	gtk_option_menu_set_history (GTK_OPTION_MENU (glade_xml_get_widget (
		swidget->xml, "key_toolbar")), history);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (glade_xml_get_widget (
		swidget->xml, "show_validity")), eel_gconf_get_boolean (SHOW_VALIDITY_KEY));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (glade_xml_get_widget (
		swidget->xml, "show_expires")), eel_gconf_get_boolean (SHOW_EXPIRES_KEY));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (glade_xml_get_widget (
		swidget->xml, "show_trust")), eel_gconf_get_boolean (SHOW_TRUST_KEY));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (glade_xml_get_widget (
		swidget->xml, "show_length")), eel_gconf_get_boolean (SHOW_LENGTH_KEY));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (glade_xml_get_widget (
		swidget->xml, "show_type")), eel_gconf_get_boolean (SHOW_TYPE_KEY));
	
	glade_xml_signal_connect_data (swidget->xml, "armor_toggled",
		G_CALLBACK (button_toggled), ARMOR_KEY);
	glade_xml_signal_connect_data (swidget->xml, "text_mode_toggled",
		G_CALLBACK (button_toggled), TEXTMODE_KEY);
	glade_xml_signal_connect_data (swidget->xml, "key_toolbar_changed",
		G_CALLBACK (key_toolbar_changed), swidget);
	glade_xml_signal_connect_data (swidget->xml, "show_validity_toggled",
		G_CALLBACK (button_toggled), SHOW_VALIDITY_KEY);
	glade_xml_signal_connect_data (swidget->xml, "show_expires_toggled",
		G_CALLBACK (button_toggled), SHOW_EXPIRES_KEY);
	glade_xml_signal_connect_data (swidget->xml, "show_trust_toggled",
		G_CALLBACK (button_toggled), SHOW_TRUST_KEY);
	glade_xml_signal_connect_data (swidget->xml, "show_length_toggled",
		G_CALLBACK (button_toggled), SHOW_LENGTH_KEY);
	glade_xml_signal_connect_data (swidget->xml, "show_type_toggled",
		G_CALLBACK (button_toggled), SHOW_TYPE_KEY);
}
