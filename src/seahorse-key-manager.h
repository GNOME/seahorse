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

#ifndef __SEAHORSE_KEY_MANAGER_H__
#define __SEAHORSE_KEY_MANAGER_H__

#include <glib.h>
#include <glib-object.h>
#include <seahorse-view.h>
#include <gtk/gtk.h>
#include <seahorse-operation.h>
#include <seahorse-key.h>
#include "seahorse-viewer.h"

G_BEGIN_DECLS


#define SEAHORSE_TYPE_KEY_MANAGER (seahorse_key_manager_get_type ())
#define SEAHORSE_KEY_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_KEY_MANAGER, SeahorseKeyManager))
#define SEAHORSE_KEY_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEY_MANAGER, SeahorseKeyManagerClass))
#define SEAHORSE_IS_KEY_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_KEY_MANAGER))
#define SEAHORSE_IS_KEY_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEY_MANAGER))
#define SEAHORSE_KEY_MANAGER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_KEY_MANAGER, SeahorseKeyManagerClass))

typedef struct _SeahorseKeyManager SeahorseKeyManager;
typedef struct _SeahorseKeyManagerClass SeahorseKeyManagerClass;
typedef struct _SeahorseKeyManagerPrivate SeahorseKeyManagerPrivate;

struct _SeahorseKeyManager {
	SeahorseViewer parent_instance;
	SeahorseKeyManagerPrivate * priv;
};

struct _SeahorseKeyManagerClass {
	SeahorseViewerClass parent_class;
};

/* 
 * The various tabs. If adding a tab be sure to fix 
 * logic throughout this file. 
 */

GtkWindow* seahorse_key_manager_show (SeahorseOperation* op);
GType seahorse_key_manager_get_type (void);


G_END_DECLS

#endif
