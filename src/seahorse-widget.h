/*
 * Seahorse
 *
 * Copyright (C) 2002 Jacob Perkins
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

/* SeahorseWidget is used by almost every dialog and window in seahorse.
 * It is a conveniance wrapper for a GladeXML definition that will
 * connect common callback functions windows and dialogs.
 * Given a name, the xml file should be 'seahorse-name.glade2',
 * and the name of the main window widget should also be the given name.
 * Basic callbacks include: showing help and closing/deleting.
 * The callbacks names must be 'help', 'closed', and 'delete_event'.
 * A component will contain additional callbacks used in windows.
 * Component callbacks include: status display, progress display,
 * and changing the visibility of the status bar and menu bar.
 * These callback names are 'focus_in_event', 'toolbar_activate', and 'statusbar_activate'
 * with the toolbar and status bar having the names 'tool_dock' and 'status'. */

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
	
	GladeXML	*xml;
	gchar		*name;
	SeahorseContext	*sctx;
};

struct _SeahorseWidgetClass
{
	GObjectClass	parent_class;
};

/* Creates a new SeahorseWidget.  This will load/show the primary window
 * and set any common callback functions */
SeahorseWidget*	seahorse_widget_new			(gchar			*name,
							 SeahorseContext	*sctx);

/* Same as seahorse_widget_new, but connects some extra callback funtions
 * that are used by main windows, such as status and progress display */
SeahorseWidget*	seahorse_widget_new_component		(gchar			*name,
							 SeahorseContext	*sctx);

/* Creates new widget without checking if type already exists */
SeahorseWidget*	seahorse_widget_new_allow_multiple	(gchar			*name,
							 SeahorseContext	*sctx);

/* Unrefs the SeahorseWidget.  Since dialogs don't ref, makes more sense. */
void		seahorse_widget_destroy			(SeahorseWidget		*swidget);

#endif /* __SEAHORSE_WIDGET_H__ */
