/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
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
 */

#include "config.h"

#include "seahorse-pkcs11-helpers.h"
#include "seahorse-pkcs11-operations.h"
#include "seahorse-token.h"

#include "seahorse-progress.h"

#include <gck/gck.h>
#include <gck/pkcs11.h>

#include <glib/gi18n.h>

#include <gcr/gcr.h>

typedef struct {
	GQueue *objects;
	GCancellable *cancellable;
} pkcs11_delete_closure;

static void
pkcs11_delete_free (gpointer data)
{
	pkcs11_delete_closure *closure = data;
	g_queue_foreach (closure->objects, (GFunc)g_object_unref, NULL);
	g_queue_free (closure->objects);
	g_clear_object (&closure->cancellable);
	g_free (closure);
}

static void pkcs11_delete_one_object (GSimpleAsyncResult *res);

static void
on_delete_object_completed (GObject *source,
                            GAsyncResult *result,
                            gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	pkcs11_delete_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;
	GckObject *object;
	SeahorseToken *pkcs11_token;

	object = g_queue_pop_head (closure->objects);
	seahorse_progress_end (closure->cancellable, object);

	if (!gck_object_destroy_finish (GCK_OBJECT (source), result, &error)) {

		/* Ignore objects that have gone away */
		if (g_error_matches (error, GCK_ERROR, CKR_OBJECT_HANDLE_INVALID)) {
			g_clear_error (&error);
		} else {
			g_simple_async_result_take_error (res, error);
			g_simple_async_result_complete (res);
		}

	}

	if (error == NULL) {
		g_object_get (object, "source", &pkcs11_token, NULL);
		g_return_if_fail (SEAHORSE_IS_TOKEN (pkcs11_token));
		seahorse_token_remove_object (pkcs11_token, GCK_OBJECT (object));
		pkcs11_delete_one_object (res);
	}

	g_object_unref (object);
	g_object_unref (res);
}

static void
pkcs11_delete_one_object (GSimpleAsyncResult *res)
{
	pkcs11_delete_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GckObject *object;

	if (g_queue_is_empty (closure->objects)) {
		g_simple_async_result_complete_in_idle (res);
		return;
	}

	object = g_queue_peek_head (closure->objects);
	seahorse_progress_begin (closure->cancellable, object);

	gck_object_destroy_async (object, closure->cancellable,
	                          on_delete_object_completed, g_object_ref (res));
}

void
seahorse_pkcs11_delete_async (GList *objects,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
	GSimpleAsyncResult *res;
	pkcs11_delete_closure *closure;
	GList *l;

	res = g_simple_async_result_new (NULL, callback, user_data,
	                                 seahorse_pkcs11_delete_async);
	closure = g_new0 (pkcs11_delete_closure, 1);
	closure->objects = g_queue_new ();
	for (l = objects; l != NULL; l = g_list_next (l)) {
		g_queue_push_tail (closure->objects, g_object_ref (l->data));
		seahorse_progress_prep (cancellable, l->data, NULL);
	}
	g_simple_async_result_set_op_res_gpointer (res, closure, pkcs11_delete_free);

	pkcs11_delete_one_object (res);

	g_object_unref (res);
}

gboolean
seahorse_pkcs11_delete_finish (GAsyncResult *result,
                               GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, NULL,
	                      seahorse_pkcs11_delete_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}
