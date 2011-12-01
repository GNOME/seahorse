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
 * 59 Temple Deletable, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef __SEAHORSE_DELETABLE_H__
#define __SEAHORSE_DELETABLE_H__

#include "seahorse-deleter.h"

#define SEAHORSE_TYPE_DELETABLE                (seahorse_deletable_get_type ())
#define SEAHORSE_DELETABLE(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_DELETABLE, SeahorseDeletable))
#define SEAHORSE_IS_DELETABLE(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_DELETABLE))
#define SEAHORSE_DELETABLE_GET_INTERFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), SEAHORSE_TYPE_DELETABLE, SeahorseDeletableIface))

typedef struct _SeahorseDeletable SeahorseDeletable;
typedef struct _SeahorseDeletableIface SeahorseDeletableIface;

struct _SeahorseDeletableIface {
	GTypeInterface parent;

	SeahorseDeleter *   (* create_deleter)      (SeahorseDeletable *deletable);
};

GType             seahorse_deletable_get_type                (void) G_GNUC_CONST;

SeahorseDeleter * seahorse_deletable_create_deleter          (SeahorseDeletable *deletable);

gboolean          seahorse_deletable_can_delete              (gpointer object);

guint             seahorse_deletable_delete_with_prompt_wait (GList *objects,
                                                              GtkWindow *parent,
                                                              GError **error);

#endif /* __SEAHORSE_DELETABLE_H__ */
