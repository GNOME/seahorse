/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
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

#ifndef __SEAHORSE_BACKEND_H__
#define __SEAHORSE_BACKEND_H__

#include <glib-object.h>

#define SEAHORSE_TYPE_BACKEND                (seahorse_backend_get_type ())
#define SEAHORSE_BACKEND(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_BACKEND, SeahorseBackend))
#define SEAHORSE_IS_BACKEND(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_BACKEND))
#define SEAHORSE_BACKEND_GET_INTERFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), SEAHORSE_TYPE_BACKEND, SeahorseBackendIface))

typedef struct _SeahorseBackend SeahorseBackend;
typedef struct _SeahorseBackendIface SeahorseBackendIface;

struct _SeahorseBackendIface {
	GTypeInterface parent;
};

GType            seahorse_backend_get_type             (void) G_GNUC_CONST;

#endif /* __SEAHORSE_BACKEND_H__ */
