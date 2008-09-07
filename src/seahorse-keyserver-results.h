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

#ifndef __SEAHORSE_KEYSERVER_RESULTS_H__
#define __SEAHORSE_KEYSERVER_RESULTS_H__

#include <glib.h>
#include <glib-object.h>
#include <seahorse-viewer.h>
#include <seahorse-operation.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <seahorse-object.h>

G_BEGIN_DECLS


#define SEAHORSE_TYPE_KEYSERVER_RESULTS (seahorse_keyserver_results_get_type ())
#define SEAHORSE_KEYSERVER_RESULTS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_KEYSERVER_RESULTS, SeahorseKeyserverResults))
#define SEAHORSE_KEYSERVER_RESULTS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEYSERVER_RESULTS, SeahorseKeyserverResultsClass))
#define SEAHORSE_IS_KEYSERVER_RESULTS(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_KEYSERVER_RESULTS))
#define SEAHORSE_IS_KEYSERVER_RESULTS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEYSERVER_RESULTS))
#define SEAHORSE_KEYSERVER_RESULTS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_KEYSERVER_RESULTS, SeahorseKeyserverResultsClass))

typedef struct _SeahorseKeyserverResults SeahorseKeyserverResults;
typedef struct _SeahorseKeyserverResultsClass SeahorseKeyserverResultsClass;
typedef struct _SeahorseKeyserverResultsPrivate SeahorseKeyserverResultsPrivate;

struct _SeahorseKeyserverResults {
	SeahorseViewer parent_instance;
	SeahorseKeyserverResultsPrivate * priv;
};

struct _SeahorseKeyserverResultsClass {
	SeahorseViewerClass parent_class;
};


void seahorse_keyserver_results_show (SeahorseOperation* op, GtkWindow* parent, const char* search_text);
const char* seahorse_keyserver_results_get_search (SeahorseKeyserverResults* self);
GType seahorse_keyserver_results_get_type (void);


G_END_DECLS

#endif
