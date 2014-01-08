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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef __SEAHORSE_OBJECT_WIDGET_H__
#define __SEAHORSE_OBJECT_WIDGET_H__

#include <glib.h>

#include "seahorse-object.h"
#include "seahorse-widget.h"

#define SEAHORSE_TYPE_OBJECT_WIDGET		(seahorse_object_widget_get_type ())
#define SEAHORSE_OBJECT_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_OBJECT_WIDGET, SeahorseObjectWidget))
#define SEAHORSE_OBJECT_WIDGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_OBJECT_WIDGET, SeahorseObjectWidgetClass))
#define SEAHORSE_IS_OBJECT_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_OBJECT_WIDGET))
#define SEAHORSE_IS_OBJECT_WIDGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_OBJECT_WIDGET))
#define SEAHORSE_OBJECT_WIDGET_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_OBJECT_WIDGET, SeahorseObjectWidgetClass))

typedef struct _SeahorseObjectWidget SeahorseObjectWidget;
typedef struct _SeahorseObjectWidgetClass SeahorseObjectWidgetClass;

struct _SeahorseObjectWidget {
	SeahorseWidget parent;
	GObject *object;
};

struct _SeahorseObjectWidgetClass {
	SeahorseWidgetClass	parent_class;
};

GType             seahorse_object_widget_get_type    (void);

SeahorseWidget *  seahorse_object_widget_new         (gchar *name,
                                                      GtkWindow *parent,
                                                      GObject *object);

#endif /* __SEAHORSE_OBJECT_WIDGET_H__ */
