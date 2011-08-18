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

#include "config.h"

#include "seahorse-pkcs11-object.h"
#include "seahorse-pkcs11-operations.h"
#include "seahorse-pkcs11-source.h"

#include "seahorse-progress.h"
#include "common/seahorse-object-list.h"

#include <gck/gck.h>
#include <gck/pkcs11.h>

#include <glib/gi18n.h>

typedef struct {
	SeahorsePkcs11Source *source;
	GCancellable *cancellable;
	GHashTable *checks;
	GckSession *session;
} pkcs11_refresh_closure;

static void
pkcs11_refresh_free (gpointer data)
{
	pkcs11_refresh_closure *closure = data;
	g_object_unref (closure->source);
	g_clear_object (&closure->cancellable);
	g_hash_table_destroy (closure->checks);
	g_clear_object (&closure->session);
	g_free (closure);
}

static guint
ulong_hash (gconstpointer k)
{
	return (guint)*((gulong*)k); 
}

static gboolean
ulong_equal (gconstpointer a, gconstpointer b)
{
	return *((gulong*)a) == *((gulong*)b); 
}

static gboolean
remove_each_object (gpointer key, gpointer value, gpointer data)
{
	seahorse_context_remove_object (NULL, value);
	return TRUE;
}

static void 
on_refresh_find_objects (GckSession *session,
                         GAsyncResult *result,
                         gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	pkcs11_refresh_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GList *objects, *l;
	GError *error = NULL;
	gulong handle;

	objects = gck_session_find_objects_finish (session, result, &error);
	if (error != NULL) {
		g_simple_async_result_take_error (res, error);
	} else {

		/* Remove all objects that were found, from the check table */
		for (l = objects; l; l = g_list_next (l)) {
			seahorse_pkcs11_source_receive_object (closure->source, l->data);
			handle = gck_object_get_handle (l->data);
			g_hash_table_remove (closure->checks, &handle);
		}

		/* Remove everything not found from the context */
		g_hash_table_foreach_remove (closure->checks, remove_each_object, NULL);
	}

	g_simple_async_result_complete (res);
	g_object_unref (res);
}

static void
on_refresh_open_session (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	pkcs11_refresh_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;
	GckAttributes *attrs;

	closure->session = gck_slot_open_session_finish (GCK_SLOT (source), result, &error);
	if (!closure->session) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);

	/* Step 2. Load all the objects that we want */
	} else {
		attrs = gck_attributes_new ();
		gck_attributes_add_boolean (attrs, CKA_TOKEN, TRUE);
		gck_attributes_add_ulong (attrs, CKA_CLASS, CKO_CERTIFICATE);
		gck_session_find_objects_async (closure->session, attrs, closure->cancellable,
		                                (GAsyncReadyCallback)on_refresh_find_objects,
		                                g_object_ref (res));
		gck_attributes_unref (attrs);
	}

	g_object_unref (res);
}

void
seahorse_pkcs11_refresh_async (SeahorsePkcs11Source *source,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
	GSimpleAsyncResult *res;
	pkcs11_refresh_closure *closure;
	GckSlot *slot;
	GList *objects, *l;
	gulong handle;

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_pkcs11_refresh_async);
	closure = g_new0 (pkcs11_refresh_closure, 1);
	closure->checks = g_hash_table_new_full (ulong_hash, ulong_equal,
	                                         g_free, g_object_unref);
	closure->source = g_object_ref (source);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	g_simple_async_result_set_op_res_gpointer (res, closure, pkcs11_refresh_free);

	/* Make note of all the objects that were there */
	objects = seahorse_context_get_objects (seahorse_context_instance (),
	                                        SEAHORSE_SOURCE (source));
	for (l = objects; l; l = g_list_next (l)) {
		if (g_object_class_find_property (G_OBJECT_GET_CLASS (l->data), "pkcs11-handle")) {
			g_object_get (l->data, "pkcs11-handle", &handle, NULL);
			g_hash_table_insert (closure->checks,
			                     g_memdup (&handle, sizeof (handle)),
			                     g_object_ref (l->data));
		}
	}
	g_list_free (objects);

	/* Step 1. Load the session */
	slot = seahorse_pkcs11_source_get_slot (closure->source);
	gck_slot_open_session_async (slot, GCK_SESSION_READ_WRITE, closure->cancellable,
	                             on_refresh_open_session, g_object_ref (res));

	g_object_unref (res);
}

gboolean
seahorse_pkcs11_refresh_finish (SeahorsePkcs11Source *source,
                                GAsyncResult *result,
                                GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_pkcs11_refresh_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;

}

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
	SeahorseObject *object;

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
		seahorse_context_remove_object (seahorse_context_instance (),
		                                object);
		pkcs11_delete_one_object (res);
	}

	g_object_unref (object);
	g_object_unref (res);
}

static void
pkcs11_delete_one_object (GSimpleAsyncResult *res)
{
	pkcs11_delete_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	SeahorseObject *object;

	if (g_queue_is_empty (closure->objects)) {
		g_simple_async_result_complete_in_idle (res);
		return;
	}

	object = g_queue_peek_head (closure->objects);
	seahorse_progress_begin (closure->cancellable, object);

	gck_object_destroy_async (seahorse_pkcs11_object_get_pkcs11_object (SEAHORSE_PKCS11_OBJECT (object)),
	                          closure->cancellable, on_delete_object_completed, g_object_ref (res));
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
