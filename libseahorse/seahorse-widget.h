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

#ifndef __SEAHORSE_WIDGET_H__
#define __SEAHORSE_WIDGET_H__

#include <glib.h>
#include <glade/glade-xml.h>

#include "seahorse-context.h"

#define SEAHORSE_TYPE_WIDGET		(seahorse_widget_get_type ())
#define SEAHORSE_WIDGET(obj)		(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_WIDGET, SeahorseWidget))
#define SEAHORSE_WIDGET_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_WIDGET, SeahorseWidgetClass))
#define SEAHORSE_IS_WIDGET(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_WIDGET))
#define SEAHORSE_IS_WIDGET_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_WIDGET))
#define SEAHORSE_WIDGET_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_WIDGET, SeahorseWidgetClass))

typedef struct _SeahorseWidget SeahorseWidget;
typedef struct _SeahorseWidgetClass SeahorseWidgetClass;

struct _SeahorseWidget
{
	GObject		parent;
	
	/*< public >*/
	GladeXML	*xml;
	gchar		*name;
	SeahorseContext	*sctx;
};

struct _SeahorseWidgetClass
{
	GObjectClass	parent_class;
};

GType           seahorse_widget_get_type ();

SeahorseWidget*	seahorse_widget_new			(gchar			*name,
							 SeahorseContext	*sctx);

SeahorseWidget*	seahorse_widget_new_allow_multiple	(gchar			*name,
							 SeahorseContext	*sctx);

void		seahorse_widget_destroy			(SeahorseWidget		*swidget);

#endif /* __SEAHORSE_WIDGET_H__ */
