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

#ifndef __SEAHORSE_VIEW_H__
#define __SEAHORSE_VIEW_H__

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "seahorse-object.h"
#include "seahorse-predicate.h"

typedef struct _SeahorseCommands SeahorseCommands;

#define SEAHORSE_TYPE_VIEW                 (seahorse_view_get_type ())
#define SEAHORSE_VIEW(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_VIEW, SeahorseView))
#define SEAHORSE_IS_VIEW(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_VIEW))
#define SEAHORSE_VIEW_GET_INTERFACE(obj)   (G_TYPE_INSTANCE_GET_INTERFACE ((obj), SEAHORSE_TYPE_VIEW, SeahorseViewIface))

typedef struct _SeahorseView SeahorseView;
typedef struct _SeahorseViewIface SeahorseViewIface;

struct _SeahorseViewIface {
	GTypeInterface parent_iface;

	/* virtual metdods */
	GList*          (*get_selected_objects)   (SeahorseView *self);
	
	void            (*set_selected_objects)   (SeahorseView *self, 
	                                           GList *objects);
	
	GList*          (*get_selected_matching)  (SeahorseView *self, 
	                                           SeahorsePredicate *pred);
	
	SeahorseObject* (*get_selected)           (SeahorseView *self);
	
	void            (*set_selected)           (SeahorseView *self, 
	                                           SeahorseObject *value);

	GtkWindow*      (*get_window)             (SeahorseView *self);
	
	void            (*register_commands)      (SeahorseView *self, 
	                                           SeahorsePredicate *pred,
	                                           SeahorseCommands *commands);
	
	void            (*register_ui)            (SeahorseView *self, 
	                                           SeahorsePredicate *pred,
	                                           const gchar *ui_definition, 
	                                           GtkActionGroup *actions);
};


GType             seahorse_view_get_type                    (void);

GList*            seahorse_view_get_selected_objects        (SeahorseView *self);

void              seahorse_view_set_selected_objects        (SeahorseView *self, 
                                                             GList *objects);

GList*            seahorse_view_get_selected_matching       (SeahorseView *self, 
                                                             SeahorsePredicate *pred);

SeahorseObject*   seahorse_view_get_selected                (SeahorseView *self);

void              seahorse_view_set_selected                (SeahorseView *self, 
                                                             SeahorseObject *value);

GtkWindow*        seahorse_view_get_window                  (SeahorseView *self);

void              seahorse_view_register_ui                 (SeahorseView *self, 
                                                             SeahorsePredicate *pred,
                                                             const gchar *ui_definition,
                                                             GtkActionGroup *actions);

void              seahorse_view_register_commands           (SeahorseView *self, 
                                                             SeahorsePredicate *pred,
                                                             SeahorseCommands *commands);

#endif
