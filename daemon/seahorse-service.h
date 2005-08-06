/*
 * Seahorse
 *
 * Copyright (C) 2005 Nate Nielsen
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

#ifndef __SEAHORSE_SERVICE_H__
#define __SEAHORSE_SERVICE_H__

#include <glib/gerror.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _SeahorseService SeahorseService;
typedef struct _SeahorseServiceClass SeahorseServiceClass;
    
#define SEAHORSE_TYPE_SERVICE               (seahorse_service_get_type ())
#define SEAHORSE_SERVICE(object)            (G_TYPE_CHECK_INSTANCE_CAST((object), SEAHORSE_TYPE_SERVICE, SeahorseService))
#define SEAHORSE_SERVICE_CLASS(klass)       (G_TYPE_CHACK_CLASS_CAST((klass), SEAHORSE_TYPE_SERVICE, SeahorseServiceClass))
#define SEAHORSE_IS_SERVICE(object)         (G_TYPE_CHECK_INSTANCE_TYPE((object), SEAHORSE_TYPE_SERVICE))
#define SEAHORSE_IS_SERVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass), SEAHORSE_TYPE_SERVICE))
#define SEAHORSE_SERVICE_GET_CLASS(object)  (G_TYPE_INSTANCE_GET_CLASS((object), SEAHORSE_TYPE_SERVICE, SeahorseServiceClass))

struct _SeahorseService {
	GObject base;
};

struct _SeahorseServiceClass {
	GObjectClass base;
};

GType 
seahorse_service_get_type ();

gboolean 
seahorse_service_get_key_types (SeahorseService *svc, gchar** ret, 
                                GError **error);

#endif /* __SEAHORSE_SERVICE_H__ */
