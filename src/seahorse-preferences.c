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

#include "seahorse-prefs.h"
#include "seahorse-gconf.h"
#include "seahorse-preferences.h"
#include "seahorse-widget.h"
#include "seahorse-check-button-control.h"

/**
 * seahorse_preferences_show:
 * @sctx: Current #SeahorseContext
 *
 * Creates a new or shows the current preferences dialog.
 **/
void
seahorse_preferences_show (SeahorseContext *sctx, guint ntab)
{	
	SeahorseWidget *swidget;
    GtkWidget *label;
    GtkWidget *tab;
    GtkWidget *box;
	GtkWidget *widget;
	
	g_return_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx));
	
    swidget = seahorse_prefs_new (sctx);
    
    label = gtk_label_new (_("Key Manager"));

    tab = gtk_vbox_new (FALSE, 12); 
    gtk_container_set_border_width (GTK_CONTAINER (tab), 12);
    
    widget = gtk_frame_new (_("Visible Columns:"));
    gtk_container_add (GTK_CONTAINER (tab), widget);
    gtk_box_set_child_packing (GTK_BOX (tab), widget, FALSE, TRUE, 0, GTK_PACK_START);
    
    box = gtk_hbox_new (FALSE, 12);
    gtk_container_set_border_width (GTK_CONTAINER (box), 12);
    gtk_container_add (GTK_CONTAINER (widget), box);    
    
	gtk_container_add (GTK_CONTAINER (box), 
            seahorse_check_button_control_new (_("_Validity"), SHOW_VALIDITY_KEY));
	gtk_container_add (GTK_CONTAINER (box), 
            seahorse_check_button_control_new (_("_Expires"), SHOW_EXPIRES_KEY));
	gtk_container_add (GTK_CONTAINER (box), 
            seahorse_check_button_control_new (_("_Trust"), SHOW_TRUST_KEY));
	gtk_container_add (GTK_CONTAINER (box), 
            seahorse_check_button_control_new (_("T_ype"), SHOW_TYPE_KEY));
	
	gtk_widget_show_all (tab);
    seahorse_prefs_add_tab (swidget, label, tab);
    
    seahorse_prefs_select_tab (swidget, ntab);
}
