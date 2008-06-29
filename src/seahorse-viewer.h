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

#include <glib.h>
#include <glib-object.h>
#include <seahorse-widget.h>
#include <seahorse-view.h>
#include <gtk/gtk.h>
#include <seahorse-key.h>
#include <stdlib.h>
#include <string.h>
#include <seahorse-keyset.h>

G_BEGIN_DECLS


#define SEAHORSE_TYPE_VIEWER (seahorse_viewer_get_type ())
#define SEAHORSE_VIEWER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_VIEWER, SeahorseViewer))
#define SEAHORSE_VIEWER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_VIEWER, SeahorseViewerClass))
#define SEAHORSE_IS_VIEWER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_VIEWER))
#define SEAHORSE_IS_VIEWER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_VIEWER))
#define SEAHORSE_VIEWER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_VIEWER, SeahorseViewerClass))

typedef struct _SeahorseViewer SeahorseViewer;
typedef struct _SeahorseViewerClass SeahorseViewerClass;
typedef struct _SeahorseViewerPrivate SeahorseViewerPrivate;

struct _SeahorseViewer {
	SeahorseWidget parent_instance;
	SeahorseViewerPrivate * priv;
};

struct _SeahorseViewerClass {
	SeahorseWidgetClass parent_class;
	GList* (*get_selected_keys) (SeahorseViewer* self);
	void (*set_selected_keys) (SeahorseViewer* self, GList* keys);
	SeahorseKey* (*get_selected_key_and_uid) (SeahorseViewer* self, guint* uid);
};


void seahorse_viewer_ensure_updated (SeahorseViewer* self);
void seahorse_viewer_include_actions (SeahorseViewer* self, GtkActionGroup* actions);
GList* seahorse_viewer_get_selected_keys (SeahorseViewer* self);
void seahorse_viewer_set_selected_keys (SeahorseViewer* self, GList* keys);
SeahorseKey* seahorse_viewer_get_selected_key_and_uid (SeahorseViewer* self, guint* uid);
void seahorse_viewer_show_context_menu (SeahorseViewer* self, guint button, guint time);
void seahorse_viewer_show_properties (SeahorseViewer* self, SeahorseKey* key);
void seahorse_viewer_set_status (SeahorseViewer* self, const char* text);
void seahorse_viewer_set_numbered_status (SeahorseViewer* self, const char* text, gint num);
SeahorseKey* seahorse_viewer_get_selected_key (SeahorseViewer* self);
void seahorse_viewer_set_selected_key (SeahorseViewer* self, SeahorseKey* value);
SeahorseKeyset* seahorse_viewer_get_current_keyset (SeahorseViewer* self);
GType seahorse_viewer_get_type (void);


G_END_DECLS

#endif
