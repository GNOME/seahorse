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
 * 59 Temple Deleter, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef __SEAHORSE_DELETER_H__
#define __SEAHORSE_DELETER_H__

#include "seahorse-types.h"

#include <gio/gio.h>

#include <gtk/gtk.h>

#define SEAHORSE_TYPE_DELETER             (seahorse_deleter_get_type ())
#define SEAHORSE_DELETER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_DELETER, SeahorseDeleter))
#define SEAHORSE_DELETER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_DELETER, SeahorseDeleterClass))
#define SEAHORSE_IS_DELETER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_DELETER))
#define SEAHORSE_IS_DELETER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_DELETER))
#define SEAHORSE_DELETER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_DELETER, SeahorseDeleterClass))

typedef struct _SeahorseDeleter SeahorseDeleter;
typedef struct _SeahorseDeleterClass SeahorseDeleterClass;

struct _SeahorseDeleter {
	GObject parent;
};

struct _SeahorseDeleterClass {
	GObjectClass parent;

	/* virtual methods ------------------------------------------------- */

	GtkDialog *     (* create_confirm)           (SeahorseDeleter *deleter,
	                                              GtkWindow *parent);

	GList *         (* get_objects)              (SeahorseDeleter *deleter);

	gboolean        (* add_object)               (SeahorseDeleter *deleter,
	                                              GObject *object);

	void            (* delete_async)             (SeahorseDeleter *deleter,
	                                              GCancellable *cancellable,
	                                              GAsyncReadyCallback callback,
	                                              gpointer user_data);

	gboolean        (* delete_finish)            (SeahorseDeleter *deleter,
	                                              GAsyncResult *result,
	                                              GError **error);
};

GType            seahorse_deleter_get_type           (void) G_GNUC_CONST;

GtkDialog *      seahorse_deleter_create_confirm     (SeahorseDeleter *deleter,
                                                      GtkWindow *parent);

GList *          seahorse_deleter_get_objects        (SeahorseDeleter *deleter);

gboolean         seahorse_deleter_add_object         (SeahorseDeleter *deleter,
                                                      GObject *object);

void             seahorse_deleter_delete_async       (SeahorseDeleter *deleter,
                                                      GCancellable *cancellable,
                                                      GAsyncReadyCallback callback,
                                                      gpointer user_data);

gboolean         seahorse_deleter_delete_finish      (SeahorseDeleter *deleter,
                                                      GAsyncResult *result,
                                                      GError **error);

gboolean         seahorse_deleter_prompt             (SeahorseDeleter *deleter,
                                                      GtkWindow *parent);

#endif /* __SEAHORSE_DELETER_H__ */
