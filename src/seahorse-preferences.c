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
#include "seahorse-check-button-control.h"
#include "seahorse-default-key-control.h"

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
	GtkWidget *widget, *child;
	gchar *key_style;
	gint history = 0;
	
	g_return_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx));
	
	swidget = seahorse_widget_new ("preferences", sctx);
	g_return_if_fail (swidget != NULL);
	
	widget = gtk_table_new (2, 1, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (widget), 12);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 12);
	
	gtk_table_attach (GTK_TABLE (widget), seahorse_check_button_control_new (
		_("_Ascii Armor"), ARMOR_KEY), 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (widget), seahorse_default_key_control_new (swidget->sctx),
		0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	
	gtk_notebook_prepend_page (GTK_NOTEBOOK (glade_xml_get_widget (swidget->xml, "notebook")),
		widget, gtk_label_new (_("PGP")));
	
	widget = gtk_hbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 12);
	gtk_container_add (GTK_CONTAINER (glade_xml_get_widget (swidget->xml, "frame")), widget);
	
	gtk_container_add (GTK_CONTAINER (widget), seahorse_check_button_control_new (
		_("_Validity"), SHOW_VALIDITY_KEY));
	gtk_container_add (GTK_CONTAINER (widget), seahorse_check_button_control_new (
		_("_Expires"), SHOW_EXPIRES_KEY));
	gtk_container_add (GTK_CONTAINER (widget), seahorse_check_button_control_new (
		_("_Trust"), SHOW_TRUST_KEY));
	gtk_container_add (GTK_CONTAINER (widget), seahorse_check_button_control_new (
		_("_Length"), SHOW_LENGTH_KEY));
	gtk_container_add (GTK_CONTAINER (widget), seahorse_check_button_control_new (
		_("T_ype"), SHOW_TYPE_KEY));
	
	gtk_widget_show_all (glade_xml_get_widget (swidget->xml, swidget->name));
}
