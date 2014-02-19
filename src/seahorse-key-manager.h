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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef __SEAHORSE_KEY_MANAGER_H__
#define __SEAHORSE_KEY_MANAGER_H__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <gcr/gcr.h>

#include "seahorse-common.h"

G_BEGIN_DECLS

#define SEAHORSE_TYPE_KEY_MANAGER 	      (seahorse_key_manager_get_type ())
#define SEAHORSE_KEY_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_KEY_MANAGER, SeahorseKeyManager))
#define SEAHORSE_KEY_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEY_MANAGER, SeahorseKeyManagerClass))
#define SEAHORSE_IS_KEY_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_KEY_MANAGER))
#define SEAHORSE_IS_KEY_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEY_MANAGER))
#define SEAHORSE_KEY_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_KEY_MANAGER, SeahorseKeyManagerClass))

typedef struct _SeahorseKeyManager SeahorseKeyManager;
typedef struct _SeahorseKeyManagerClass SeahorseKeyManagerClass;
typedef struct _SeahorseKeyManagerPrivate SeahorseKeyManagerPrivate;

struct _SeahorseKeyManager {
	SeahorseCatalog parent_instance;
	SeahorseKeyManagerPrivate *pv;
};

struct _SeahorseKeyManagerClass {
	SeahorseCatalogClass parent_class;
};

GType            seahorse_key_manager_get_type     (void) G_GNUC_CONST;

SeahorseKeyManager * seahorse_key_manager_show     (guint32 timestamp);


G_END_DECLS

#endif
