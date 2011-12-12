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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef __SEAHORSE_LOCKABLE_H__
#define __SEAHORSE_LOCKABLE_H__

#include <gio/gio.h>

#include <gtk/gtk.h>

#define SEAHORSE_TYPE_LOCKABLE                (seahorse_lockable_get_type ())
#define SEAHORSE_LOCKABLE(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_LOCKABLE, SeahorseLockable))
#define SEAHORSE_IS_LOCKABLE(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_LOCKABLE))
#define SEAHORSE_LOCKABLE_GET_INTERFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), SEAHORSE_TYPE_LOCKABLE, SeahorseLockableIface))

typedef struct _SeahorseLockable SeahorseLockable;
typedef struct _SeahorseLockableIface SeahorseLockableIface;

struct _SeahorseLockableIface {
	GTypeInterface parent;

	void        (* lock_async)         (SeahorseLockable *lockable,
	                                    GTlsInteraction *interaction,
	                                    GCancellable *cancellable,
	                                    GAsyncReadyCallback callback,
	                                    gpointer user_data);

	gboolean    (* lock_finish)        (SeahorseLockable *lockable,
	                                    GAsyncResult *result,
	                                    GError **error);

	void        (* unlock_async)       (SeahorseLockable *lockable,
	                                    GTlsInteraction *interaction,
	                                    GCancellable *cancellable,
	                                    GAsyncReadyCallback callback,
	                                    gpointer user_data);

	gboolean    (* unlock_finish)      (SeahorseLockable *lockable,
	                                    GAsyncResult *result,
	                                    GError **error);
};

GType              seahorse_lockable_get_type               (void) G_GNUC_CONST;

void               seahorse_lockable_lock_async             (SeahorseLockable *lockable,
                                                             GTlsInteraction *interaction,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

gboolean           seahorse_lockable_lock_finish            (SeahorseLockable *lockable,
                                                             GAsyncResult *result,
                                                             GError **error);

void               seahorse_lockable_unlock_async           (SeahorseLockable *lockable,
                                                             GTlsInteraction *interaction,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

gboolean           seahorse_lockable_unlock_finish          (SeahorseLockable *lockable,
                                                             GAsyncResult *result,
                                                             GError **error);

gboolean           seahorse_lockable_can_lock               (gpointer object);

gboolean           seahorse_lockable_can_unlock             (gpointer object);

#endif /* __SEAHORSE_LOCKABLE_H__ */
