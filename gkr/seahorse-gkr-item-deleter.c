/* 
 * Seahorse
 * 
 * Copyright (C) 2011 Collabora Ltd.
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"

#include "seahorse-gkr-backend.h"
#include "seahorse-gkr-item-deleter.h"

#include "seahorse-common.h"
#include "seahorse-object.h"

#include <glib/gi18n.h>

#define SEAHORSE_GKR_ITEM_DELETER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GKR_ITEM_DELETER, SeahorseGkrItemDeleterClass))
#define SEAHORSE_IS_GKR_ITEM_DELETER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GKR_ITEM_DELETER))
#define SEAHORSE_GKR_ITEM_DELETER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GKR_ITEM_DELETER, SeahorseGkrItemDeleterClass))

typedef struct _SeahorseGkrItemDeleterClass SeahorseGkrItemDeleterClass;

struct _SeahorseGkrItemDeleter {
	SeahorseDeleter parent;
	GList *items;
};

struct _SeahorseGkrItemDeleterClass {
	SeahorseDeleterClass parent_class;
};

static void     delete_one_item                    (GSimpleAsyncResult *res);

G_DEFINE_TYPE (SeahorseGkrItemDeleter, seahorse_gkr_item_deleter, SEAHORSE_TYPE_DELETER);

static void
seahorse_gkr_item_deleter_init (SeahorseGkrItemDeleter *self)
{

}

static void
seahorse_gkr_item_deleter_finalize (GObject *obj)
{
	SeahorseGkrItemDeleter *self = SEAHORSE_GKR_ITEM_DELETER (obj);

	g_list_free_full (self->items, g_object_unref);

	G_OBJECT_CLASS (seahorse_gkr_item_deleter_parent_class)->finalize (obj);
}

static GtkDialog *
seahorse_gkr_item_deleter_create_confirm (SeahorseDeleter *deleter,
                                          GtkWindow *parent)
{
	SeahorseGkrItemDeleter *self = SEAHORSE_GKR_ITEM_DELETER (deleter);
	GtkDialog *dialog;
	gchar *prompt;
	guint num;

	num = g_list_length (self->items);
	if (num == 1) {
		prompt = g_strdup_printf (_ ("Are you sure you want to delete the password '%s'?"),
		                          secret_item_get_label (SECRET_ITEM (self->items->data)));
	} else {
		prompt = g_strdup_printf (ngettext ("Are you sure you want to delete %d password?",
		                                    "Are you sure you want to delete %d passwords?",
		                                    num), num);
	}


	dialog = seahorse_delete_dialog_new (parent, "%s", prompt);
	g_free (prompt);

	return dialog;
}

static GList *
seahorse_gkr_item_deleter_get_objects (SeahorseDeleter *deleter)
{
	SeahorseGkrItemDeleter *self = SEAHORSE_GKR_ITEM_DELETER (deleter);
	return self->items;
}

static gboolean
seahorse_gkr_item_deleter_add_object (SeahorseDeleter *deleter,
                                      GObject *object)
{
	SeahorseGkrItemDeleter *self = SEAHORSE_GKR_ITEM_DELETER (deleter);

	if (!SEAHORSE_IS_GKR_ITEM (object))
		return FALSE;
	self->items = g_list_append (self->items, g_object_ref (object));
	return TRUE;
}

typedef struct {
	SeahorseGkrItem *current;
	GCancellable *cancellable;
	GQueue queue;
	gpointer request;
	gulong cancelled_sig;
} DeleteClosure;

static void
delete_closure_free (gpointer data)
{
	DeleteClosure *closure = data;
	SeahorseGkrItem *item;

	g_clear_object (&closure->current);
	while ((item = g_queue_pop_head (&closure->queue)))
		g_object_unref (item);
	g_queue_clear (&closure->queue);
	if (closure->cancellable && closure->cancelled_sig)
		g_signal_handler_disconnect (closure->cancellable,
		                             closure->cancelled_sig);
	g_clear_object (&closure->cancellable);
	g_assert (!closure->request);
	g_free (closure);
}

static void
on_delete_gkr_complete (GObject *source,
                        GAsyncResult *result,
                        gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	GError *error = NULL;

	secret_item_delete_finish (SECRET_ITEM (source), result, &error);
	if (error == NULL) {
		delete_one_item (res);

	} else {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);
	}

	g_object_unref (res);
}

static void
delete_one_item (GSimpleAsyncResult *res)
{
	DeleteClosure *closure = g_simple_async_result_get_op_res_gpointer (res);

	g_clear_object (&closure->current);
	closure->current = g_queue_pop_head (&closure->queue);
	if (closure->current == NULL) {
		g_simple_async_result_complete_in_idle (res);

	} else {
		secret_item_delete (SECRET_ITEM (closure->current),
		                    closure->cancellable,
		                    on_delete_gkr_complete,
		                    g_object_ref (res));
	}
}

static void
seahorse_gkr_item_deleter_delete_async (SeahorseDeleter *deleter,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data)
{
	SeahorseGkrItemDeleter *self = SEAHORSE_GKR_ITEM_DELETER (deleter);
	GSimpleAsyncResult *res;
	DeleteClosure *closure;
	GList *l;

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_gkr_item_deleter_delete_async);
	closure = g_new0 (DeleteClosure, 1);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	g_queue_init (&closure->queue);
	for (l = self->items; l != NULL; l = g_list_next (l))
		g_queue_push_tail (&closure->queue, g_object_ref (l->data));
	g_simple_async_result_set_op_res_gpointer (res, closure, delete_closure_free);

	delete_one_item (res);
	g_object_unref (res);
}

static gboolean
seahorse_gkr_item_deleter_delete_finish (SeahorseDeleter *deleter,
                                            GAsyncResult *result,
                                            GError **error)
{
	SeahorseGkrItemDeleter *self = SEAHORSE_GKR_ITEM_DELETER (deleter);

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
	                      seahorse_gkr_item_deleter_delete_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

static void
seahorse_gkr_item_deleter_class_init (SeahorseGkrItemDeleterClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseDeleterClass *deleter_class = SEAHORSE_DELETER_CLASS (klass);

	gobject_class->finalize = seahorse_gkr_item_deleter_finalize;

	deleter_class->add_object = seahorse_gkr_item_deleter_add_object;
	deleter_class->create_confirm = seahorse_gkr_item_deleter_create_confirm;
	deleter_class->delete = seahorse_gkr_item_deleter_delete_async;
	deleter_class->delete_finish = seahorse_gkr_item_deleter_delete_finish;
	deleter_class->get_objects = seahorse_gkr_item_deleter_get_objects;
}

SeahorseDeleter *
seahorse_gkr_item_deleter_new (SeahorseGkrItem *item)
{
	SeahorseDeleter *deleter;

	deleter = g_object_new (SEAHORSE_TYPE_GKR_ITEM_DELETER, NULL);
	if (!seahorse_deleter_add_object (deleter, G_OBJECT (item)))
		g_assert_not_reached ();

	return deleter;
}
