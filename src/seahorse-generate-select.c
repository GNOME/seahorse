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

#include "seahorse-widget.h"
#include "seahorse-util.h"
#include "seahorse-key-op.h"

#define DRUID "radio_druid"
#define ADV "radio_adv"
void
on_method_ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	GtkToggleButton *druid, *adv;
	GtkWidget *widget;
	
	druid = glade_xml_get_widget (swidget->xml, DRUID);
	adv = glade_xml_get_widget (swidget->xml, ADV);
	
	widget = glade_xml_get_widget (swidget->xml, swidget->name);
	gtk_widget_hide (widget);
	
	if(druid->active){
		seahorse_generate_druid_show (swidget->sctx);
	}else if(adv->active){
		seahorse_generate_adv_show (swidget->sctx);
	}
}


void
seahorse_generate_select_show (SeahorseContext *sctx)
{	
	SeahorseWidget *swidget;
	
	swidget = seahorse_widget_new ("generate-select", sctx);
	g_return_if_fail (swidget != NULL);
	
              
	glade_xml_signal_connect_data(swidget->xml, "on_method_ok_clicked",
		G_CALLBACK (on_method_ok_clicked), swidget);
}
