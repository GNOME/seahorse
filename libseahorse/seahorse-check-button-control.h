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

/** 
 * SeahorseCheckButtonControl: A checkbox widget which is tied to a 
 * boolean gconf key. 
 *
 * - Derived from GtkCheckButton.
 *
 * Properties:
 *   gconf-key: (gchar*) The GConf key to set and monitor.
 */
 
#ifndef __SEAHORSE_CHECK_BUTTON_CONTROL_H__
#define __SEAHORSE_CHECK_BUTTON_CONTROL_H__

#include <gtk/gtk.h>

#define SEAHORSE_TYPE_CHECK_BUTTON_CONTROL		(seahorse_check_button_control_get_type ())
#define SEAHORSE_CHECK_BUTTON_CONTROL(obj)		(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_CHECK_BUTTON_CONTROL, SeahorseCheckButtonControl))
#define SEAHORSE_CHECK_BUTTON_CONTROL_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_CHECK_BUTTON_CONTROL, SeahorseCheckButtonControlClass))
#define SEAHORSE_IS_CHECK_BUTTON_CONTROL(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_CHECK_BUTTON_CONTROL))
#define SEAHORSE_IS_CHECK_BUTTON_CONTROL_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_CHECK_BUTTON_CONTROL))
#define SEAHORSE_CHECK_BUTTON_CONTROL_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_CHECK_BUTTON_CONTROL, SeahorseCheckButtonControlClass))

typedef struct _SeahorseCheckButtonControl SeahorseCheckButtonControl;
typedef struct _SeahorseCheckButtonControlClass SeahorseCheckButtonControlClass;

struct _SeahorseCheckButtonControl
{
	GtkCheckButton		parent;
	
	gchar			*gconf_key;
	guint			notify_id;
};

struct _SeahorseCheckButtonControlClass
{
	GtkCheckButtonClass	parent_class;
};

GtkWidget*	seahorse_check_button_control_new	(const gchar	*label,
							 const gchar	*gconf_key);

#endif /* __SEAHORSE_CHECK_BUTTON_CONTROL_H__ */
