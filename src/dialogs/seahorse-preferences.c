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
#include <gconf/gconf-client.h>
#include <gpgme.h>

#include "seahorse-preferences.h"
#include "seahorse-widget.h"
#include "seahorse-signer-menu.h"

#define DEFAULT_OPTION "default_key"

/* Toggles armor setting */
static void
armor_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	seahorse_context_set_ascii_armor (swidget->sctx, gtk_toggle_button_get_active (togglebutton));
}

/* Toggles text mode setting */
static void
text_mode_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	seahorse_context_set_text_mode (swidget->sctx, gtk_toggle_button_get_active (togglebutton));
}

void
seahorse_preferences_show (SeahorseContext *sctx)
{	
	SeahorseWidget *swidget;
	GtkWidget *widget;
	
	g_return_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx));
	
	swidget = seahorse_widget_new ("preferences", sctx);
	g_return_if_fail (swidget != NULL);
	
	widget = glade_xml_get_widget (swidget->xml, "ascii_armor");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
		gpgme_get_armor (sctx->ctx));
	
	widget = glade_xml_get_widget (swidget->xml, "text_mode");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
		gpgme_get_textmode (sctx->ctx));
	
	seahorse_signer_menu_new (sctx, GTK_OPTION_MENU (
		glade_xml_get_widget (swidget->xml, "default_key")));
	
	glade_xml_signal_connect_data (swidget->xml, "armor_toggled",
		G_CALLBACK (armor_toggled), swidget);
	glade_xml_signal_connect_data (swidget->xml, "text_mode_toggled",
		G_CALLBACK (text_mode_toggled), swidget);
}
