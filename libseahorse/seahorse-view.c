/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
 * 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *  
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include "seahorse-view.h"

GList* 
seahorse_view_get_selected_objects (SeahorseView* self) 
{
	g_return_val_if_fail (SEAHORSE_VIEW_GET_INTERFACE (self)->get_selected_objects, NULL);
	return SEAHORSE_VIEW_GET_INTERFACE (self)->get_selected_objects (self);
}


void 
seahorse_view_set_selected_objects (SeahorseView* self, GList* objects) 
{
	g_return_if_fail (SEAHORSE_VIEW_GET_INTERFACE (self)->set_selected_objects);
	SEAHORSE_VIEW_GET_INTERFACE (self)->set_selected_objects (self, objects);
}


GList*
seahorse_view_get_selected_matching (SeahorseView *self,
                                     SeahorsePredicate *pred)
{
	g_return_val_if_fail (SEAHORSE_VIEW_GET_INTERFACE (self)->get_selected_matching, NULL);
	return SEAHORSE_VIEW_GET_INTERFACE (self)->get_selected_matching (self, pred);
}

GObject *
seahorse_view_get_selected (SeahorseView* self) 
{
	g_return_val_if_fail (SEAHORSE_VIEW_GET_INTERFACE (self)->get_selected, NULL);
	return SEAHORSE_VIEW_GET_INTERFACE (self)->get_selected (self);
}


void 
seahorse_view_set_selected (SeahorseView* self,
                            GObject* value)
{
	g_return_if_fail (SEAHORSE_VIEW_GET_INTERFACE (self)->set_selected);
	SEAHORSE_VIEW_GET_INTERFACE (self)->set_selected (self, value);
}

GtkWindow* 
seahorse_view_get_window (SeahorseView* self) 
{
	g_return_val_if_fail (SEAHORSE_VIEW_GET_INTERFACE (self)->get_window, NULL);
	return SEAHORSE_VIEW_GET_INTERFACE (self)->get_window (self);
}

void
seahorse_view_register_commands (SeahorseView *self,
                                 SeahorsePredicate *pred,
                                 SeahorseCommands *commands)
{
	g_return_if_fail (SEAHORSE_VIEW_GET_INTERFACE (self)->register_commands);
	SEAHORSE_VIEW_GET_INTERFACE (self)->register_commands (self, pred, commands);
}

void
seahorse_view_register_ui (SeahorseView *self,
                           SeahorsePredicate *pred,
                           const gchar *ui_definition,
                           GtkActionGroup *actions)
{
	g_return_if_fail (SEAHORSE_VIEW_GET_INTERFACE (self)->register_ui);
	SEAHORSE_VIEW_GET_INTERFACE (self)->register_ui (self, pred, ui_definition, actions);
}

static void 
seahorse_view_base_init (SeahorseViewIface * iface) 
{
	static gboolean initialized = FALSE;
	if (!initialized) {
		initialized = TRUE;
		g_object_interface_install_property (iface, 
		         g_param_spec_object ("selected", "selected", "selected", 
		                              SEAHORSE_TYPE_OBJECT, G_PARAM_READWRITE));

		g_object_interface_install_property (iface, 
		         g_param_spec_object ("window", "window", "window", 
		                              GTK_TYPE_WINDOW, G_PARAM_READABLE));
		
		g_signal_new ("selection_changed", SEAHORSE_TYPE_VIEW, G_SIGNAL_RUN_LAST, 0, NULL, NULL, 
		              g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
	}
}

GType 
seahorse_view_get_type (void) 
{
	static GType seahorse_view_type_id = 0;
	
	if (seahorse_view_type_id == 0) {
		static const GTypeInfo type_info = { 
			sizeof (SeahorseViewIface), (GBaseInitFunc) seahorse_view_base_init, 
			(GBaseFinalizeFunc) NULL, (GClassInitFunc) NULL, 
			(GClassFinalizeFunc) NULL, NULL, 0, 0, (GInstanceInitFunc) NULL 
		};
		
		seahorse_view_type_id = g_type_register_static (G_TYPE_INTERFACE, "SeahorseView", &type_info, 0);
		g_type_interface_add_prerequisite (seahorse_view_type_id, G_TYPE_OBJECT);
	}
	
	return seahorse_view_type_id;
}

