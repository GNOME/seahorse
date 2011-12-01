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

#include "config.h"

#include "seahorse-deleter.h"

G_DEFINE_TYPE (SeahorseDeleter, seahorse_deleter, G_TYPE_OBJECT);

static void
seahorse_deleter_init (SeahorseDeleter *deleter)
{

}

static void
seahorse_deleter_class_init (SeahorseDeleterClass *klass)
{

}

GtkDialog *
seahorse_deleter_create_confirm (SeahorseDeleter *deleter,
                                 GtkWindow *parent)
{
	SeahorseDeleterClass *class;

	g_return_val_if_fail (SEAHORSE_IS_DELETER (deleter), NULL);

	class = SEAHORSE_DELETER_GET_CLASS (deleter);
	g_return_val_if_fail (class->create_confirm != NULL, NULL);

	return (class->create_confirm) (deleter, parent);
}

GList *
seahorse_deleter_get_objects (SeahorseDeleter *deleter)
{
	SeahorseDeleterClass *class;

	g_return_val_if_fail (SEAHORSE_IS_DELETER (deleter), NULL);

	class = SEAHORSE_DELETER_GET_CLASS (deleter);
	g_return_val_if_fail (class->get_objects != NULL, NULL);

	return (class->get_objects) (deleter);
}

gboolean
seahorse_deleter_add_object (SeahorseDeleter *deleter,
                             GObject *object)
{
	SeahorseDeleterClass *class;

	g_return_val_if_fail (SEAHORSE_IS_DELETER (deleter), FALSE);
	g_return_val_if_fail (G_IS_OBJECT (object), FALSE);

	class = SEAHORSE_DELETER_GET_CLASS (deleter);
	g_return_val_if_fail (class->add_object != NULL, FALSE);

	return (class->add_object) (deleter, object);
}

void
seahorse_deleter_delete_async (SeahorseDeleter *deleter,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
	SeahorseDeleterClass *class;

	g_return_if_fail (SEAHORSE_IS_DELETER (deleter));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	class = SEAHORSE_DELETER_GET_CLASS (deleter);
	g_return_if_fail (class->delete_async != NULL);

	(class->delete_async) (deleter, cancellable, callback, user_data);
}

gboolean
seahorse_deleter_delete_finish (SeahorseDeleter *deleter,
                                GAsyncResult *result,
                                GError **error)
{
	SeahorseDeleterClass *class;

	g_return_val_if_fail (SEAHORSE_IS_DELETER (deleter), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	class = SEAHORSE_DELETER_GET_CLASS (deleter);
	g_return_val_if_fail (class->delete_finish != NULL, FALSE);

	return (class->delete_finish) (deleter, result, error);
}

gboolean
seahorse_deleter_prompt (SeahorseDeleter *deleter,
                         GtkWindow *parent)
{
	GtkDialog *prompt;
	gint res;

	g_return_val_if_fail (SEAHORSE_IS_DELETER (deleter), FALSE);
	g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), FALSE);

	prompt = seahorse_deleter_create_confirm (deleter, parent);
	g_return_val_if_fail (prompt != NULL, FALSE);

	res = gtk_dialog_run (prompt);
	gtk_widget_destroy (GTK_WIDGET (prompt));

	return res == GTK_RESPONSE_OK || res == GTK_RESPONSE_ACCEPT;
}
