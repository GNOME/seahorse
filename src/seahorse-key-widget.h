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

#ifndef __SEAHORSE_KEY_WIDGET_H__
#define __SEAHORSE_KEY_WIDGET_H__

#include <glib.h>

#include "seahorse-widget.h"
#include "seahorse-key.h"

#define SEAHORSE_TYPE_KEY_WIDGET		(seahorse_key_widget_get_type ())
#define SEAHORSE_KEY_WIDGET(obj)		(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_KEY_WIDGET, SeahorseKeyWidget))
#define SEAHORSE_KEY_WIDGET_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEY_WIDGET, SeahorseKeyWidgetClass))
#define SEAHORSE_IS_KEY_WIDGET(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_KEY_WIDGET))
#define SEAHORSE_IS_KEY_WIDGET_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEY_WIDGET))
#define SEAHORSE_KEY_WIDGET_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_KEY_WIDGET, SeahorseKeyWidgetClass))

typedef struct _SeahorseKeyWidget SeahorseKeyWidget;
typedef struct _SeahorseKeyWidgetClass SeahorseKeyWidgetClass;
	
struct _SeahorseKeyWidget
{
	SeahorseWidget		parent;
	
	SeahorseKey		*skey;
	guint			index;
};

struct _SeahorseKeyWidgetClass
{
	SeahorseWidgetClass	parent_class;
};

SeahorseWidget*	seahorse_key_widget_new			(gchar			*name,
							 SeahorseContext	*sctx,
							 SeahorseKey		*skey);

SeahorseWidget*	seahorse_key_widget_new_with_index	(gchar			*name,
							 SeahorseContext	*sctx,
							 SeahorseKey		*skey,
							 guint			index);

gboolean	seahorse_key_widget_can_create		(gchar			*name,
							 SeahorseKey		*skey);
	
#endif /* __SEAHORSE_KEY_WIDGET_H__ */
