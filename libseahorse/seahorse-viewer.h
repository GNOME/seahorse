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

#ifndef __SEAHORSE_VIEWER_H__
#define __SEAHORSE_VIEWER_H__

#include <glib-object.h>

#include "seahorse-widget.h"

#define SEAHORSE_VIEWER_MENU_OBJECT   "ObjectPopup"

#define SEAHORSE_TYPE_VIEWER               (seahorse_viewer_get_type ())
#define SEAHORSE_VIEWER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_VIEWER, SeahorseViewer))
#define SEAHORSE_VIEWER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_VIEWER, SeahorseViewerClass))
#define SEAHORSE_IS_VIEWER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_VIEWER))
#define SEAHORSE_IS_VIEWER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_VIEWER))
#define SEAHORSE_VIEWER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_VIEWER, SeahorseViewerClass))

typedef struct _SeahorseViewer SeahorseViewer;
typedef struct _SeahorseViewerClass SeahorseViewerClass;
typedef struct _SeahorseViewerPrivate SeahorseViewerPrivate;

struct _SeahorseViewer {
	SeahorseWidget parent;
	SeahorseViewerPrivate *pv;
};

struct _SeahorseViewerClass {
	SeahorseWidgetClass parent;

	GList *          (*get_backends)                  (SeahorseViewer *self);

	GList *          (*get_selected_places)           (SeahorseViewer *self);

	GList *          (*get_selected_objects)          (SeahorseViewer *self);

	void             (*selection_changed)             (SeahorseViewer *viewer);
};

GType               seahorse_viewer_get_type                        (void);

void                seahorse_viewer_ensure_updated                  (SeahorseViewer* self);

void                seahorse_viewer_include_actions                 (SeahorseViewer* self,
                                                                     GtkActionGroup* actions);

GList *             seahorse_viewer_get_selected_objects            (SeahorseViewer* self);

GList *             seahorse_viewer_get_selected_places             (SeahorseViewer* self);

GList *             seahorse_viewer_get_backends                    (SeahorseViewer* self);

void                seahorse_viewer_show_context_menu               (SeahorseViewer* self,
                                                                     const gchar *which,
                                                                     guint button,
                                                                     guint time);

void                seahorse_viewer_show_properties                 (SeahorseViewer* self,
                                                                     GObject* obj);

GtkWindow *         seahorse_viewer_get_window                      (SeahorseViewer* self);

#endif /* __SEAHORSE_VIEWER_H__ */
