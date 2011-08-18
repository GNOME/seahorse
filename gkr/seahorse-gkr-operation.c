/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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

#include "config.h"

#include <sys/wait.h>
#include <sys/socket.h>
#include <fcntl.h>

#include <glib/gi18n.h>

#include "seahorse-gkr-operation.h"
#include "seahorse-progress.h"
#include "seahorse-util.h"
#include "seahorse-passphrase.h"

#include <gnome-keyring.h>
#include <gnome-keyring-memory.h>

gboolean
seahorse_gkr_propagate_error (GnomeKeyringResult result, GError **error)
{
    static GQuark errorq = 0;
    const gchar *message = NULL;
    gint code = (gint)result;

    if (result == GNOME_KEYRING_RESULT_OK)
        return FALSE;

    /* An error mark it as such */
    switch (result) {
    case GNOME_KEYRING_RESULT_CANCELLED:
        message = _("The operation was cancelled");
        code = G_IO_ERROR_CANCELLED;
        errorq = G_IO_ERROR;
        break;
    case GNOME_KEYRING_RESULT_DENIED:
        message = _("Access to the key ring was denied");
        break;
    case GNOME_KEYRING_RESULT_NO_KEYRING_DAEMON:
        message = _("The gnome-keyring daemon is not running");
        break;
    case GNOME_KEYRING_RESULT_ALREADY_UNLOCKED:
        message = _("The key ring has already been unlocked");
        break;
    case GNOME_KEYRING_RESULT_NO_SUCH_KEYRING:
        message = _("No such key ring exists");
        break;
    case GNOME_KEYRING_RESULT_IO_ERROR:
        message = _("Couldn't communicate with key ring daemon");
        break;
    case GNOME_KEYRING_RESULT_ALREADY_EXISTS:
        message = _("The item already exists");
        break;
    case GNOME_KEYRING_RESULT_BAD_ARGUMENTS:
        g_warning ("bad arguments passed to gnome-keyring API");
        /* Fall through */
    default:
        message = _("Internal error accessing gnome-keyring");
        break;
    }
    
    if (!errorq) 
        errorq = g_quark_from_static_string ("seahorse-gnome-keyring");

    g_set_error (error, errorq, code, "%s", message);
    return TRUE;
}

typedef struct {
	GnomeKeyringItemInfo *info;
	gchar *secret;
	SeahorseGkrItem *item;
	GCancellable *cancellable;
	gulong cancelled_sig;
	gpointer request;
} update_secret_closure;

static void
update_secret_free (gpointer data)
{
	update_secret_closure *closure = data;
	gnome_keyring_free_password (closure->secret);
	g_object_unref (closure->item);
	if (closure->cancellable && closure->cancelled_sig)
		g_signal_handler_disconnect (closure->cancellable,
		                             closure->cancelled_sig);
	g_clear_object (&closure->cancellable);
	gnome_keyring_item_info_free (closure->info);
	g_assert (closure->request == NULL);
	g_free (closure);
}

static void
on_update_secret_set_info_complete (GnomeKeyringResult result,
                                    gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	update_secret_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	closure->request = NULL;
	seahorse_progress_end (closure->cancellable, res);

	if (seahorse_gkr_propagate_error (result, &error))
		g_simple_async_result_take_error (res, error);
	else
		seahorse_gkr_item_set_info (closure->item, closure->info);

	g_simple_async_result_complete_in_idle (res);
}

static void
on_update_secret_get_info_complete (GnomeKeyringResult result,
                                    GnomeKeyringItemInfo *info,
                                    gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	update_secret_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	closure->request = NULL;

	/* Operation to get info failed */
	if (seahorse_gkr_propagate_error (result, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);
		seahorse_progress_end (closure->cancellable, res);

	/* Update the description */
	} else {
		closure->info = gnome_keyring_item_info_copy (info);
		gnome_keyring_item_info_set_secret (closure->info, closure->secret);

		closure->request = gnome_keyring_item_set_info (seahorse_gkr_item_get_keyring_name (closure->item),
		                                                seahorse_gkr_item_get_item_id (closure->item),
		                                                closure->info, on_update_secret_set_info_complete,
		                                                g_object_ref (res), g_object_unref);
	}

	gnome_keyring_free_password (closure->secret);
	closure->secret = NULL;
}

static void
on_update_secret_cancelled (GCancellable *cancellable,
                            gpointer user_data)
{
	update_secret_closure *closure = user_data;

	if (closure->request)
		gnome_keyring_cancel_request (closure->request);
}

void
seahorse_gkr_update_secret_async (SeahorseGkrItem *item,
                                  const gchar *secret,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	GSimpleAsyncResult *res;
	update_secret_closure *closure;

	g_return_if_fail (SEAHORSE_IS_GKR_ITEM (item));
	g_return_if_fail (secret);

	res = g_simple_async_result_new (G_OBJECT (item), callback, user_data,
	                                 seahorse_gkr_update_secret_async);

	closure = g_new0 (update_secret_closure, 1);
	closure->item = g_object_ref (item);
	closure->secret = gnome_keyring_memory_strdup (secret);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	g_simple_async_result_set_op_res_gpointer (res, closure, update_secret_free);

	seahorse_progress_prep_and_begin (cancellable, res, NULL);
	closure->request = gnome_keyring_item_get_info (seahorse_gkr_item_get_keyring_name (item),
	                                                seahorse_gkr_item_get_item_id (item),
	                                                on_update_secret_get_info_complete,
	                                                g_object_ref (res), g_object_unref);

	if (cancellable)
		closure->cancelled_sig = g_cancellable_connect (cancellable,
		                                                G_CALLBACK (on_update_secret_cancelled),
		                                                closure, NULL);
	g_object_unref (res);
}

gboolean
seahorse_gkr_update_secret_finish (SeahorseGkrItem *item,
                                   GAsyncResult *result,
                                   GError **error)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_ITEM (item), FALSE);
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (item),
	                      seahorse_gkr_update_secret_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

typedef struct {
	GnomeKeyringItemInfo *info;
	gchar *description;
	SeahorseGkrItem *item;
	gpointer request;
	GCancellable *cancellable;
	gulong cancelled_sig;
} update_description_closure;

static void
update_description_free (gpointer data)
{
	update_description_closure *closure = data;
	g_free (closure->description);
	g_object_unref (closure->item);
	if (closure->cancellable && closure->cancelled_sig)
		g_signal_handler_disconnect (closure->cancellable,
		                             closure->cancelled_sig);
	g_clear_object (&closure->cancellable);
	gnome_keyring_item_info_free (closure->info);
	g_assert (closure->request == NULL);
	g_free (closure);
}

static void
on_update_description_set_info_complete (GnomeKeyringResult result,
                                         gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	update_description_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	closure->request = NULL;
	seahorse_progress_end (closure->cancellable, res);

	if (seahorse_gkr_propagate_error (result, &error))
		g_simple_async_result_take_error (res, error);
	else
		seahorse_gkr_item_set_info (closure->item, closure->info);

	g_simple_async_result_complete_in_idle (res);
}

static void
on_update_description_get_info_complete (GnomeKeyringResult result,
                                         GnomeKeyringItemInfo *info,
                                         gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	update_description_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	closure->request = NULL;

	/* Operation to get info failed */
	if (seahorse_gkr_propagate_error (result, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);
		seahorse_progress_end (closure->cancellable, res);

	/* The description hasn't changed */
	} else if (g_str_equal (closure->description,
	                        gnome_keyring_item_info_get_display_name (info))) {
		g_simple_async_result_complete_in_idle (res);
		seahorse_progress_end (closure->cancellable, res);

	/* Update the description */
	} else {
		closure->info = gnome_keyring_item_info_copy (info);
		gnome_keyring_item_info_set_display_name (closure->info, closure->description);

		closure->request = gnome_keyring_item_set_info (seahorse_gkr_item_get_keyring_name (closure->item),
		                                                seahorse_gkr_item_get_item_id (closure->item),
		                                                closure->info, on_update_description_set_info_complete,
		                                                g_object_ref (res), g_object_unref);
	}
}

static void
on_update_description_cancelled (GCancellable *cancellable,
                                 gpointer user_data)
{
	update_description_closure *closure = user_data;

	if (closure->request)
		gnome_keyring_cancel_request (closure->request);
}

void
seahorse_gkr_update_description_async (SeahorseGkrItem *item,
                                       const gchar *description,
                                       GCancellable *cancellable,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
	GSimpleAsyncResult *res;
	update_description_closure *closure;

	g_return_if_fail (SEAHORSE_IS_GKR_ITEM (item));
	g_return_if_fail (description);

	res = g_simple_async_result_new (G_OBJECT (item), callback, user_data,
	                                 seahorse_gkr_update_description_async);

	closure = g_new0 (update_description_closure, 1);
	closure->item = g_object_ref (item);
	closure->description = g_strdup (description);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	g_simple_async_result_set_op_res_gpointer (res, closure, update_description_free);

	seahorse_progress_prep_and_begin (cancellable, res, NULL);
	closure->request = gnome_keyring_item_get_info (seahorse_gkr_item_get_keyring_name (item),
	                                                seahorse_gkr_item_get_item_id (item),
	                                                on_update_description_get_info_complete,
	                                                g_object_ref (res), g_object_unref);

	if (cancellable)
		closure->cancelled_sig = g_cancellable_connect (cancellable,
		                                                G_CALLBACK (on_update_description_cancelled),
		                                                closure, NULL);
	g_object_unref (res);
}

gboolean
seahorse_gkr_update_description_finish (SeahorseGkrItem *item,
                                        GAsyncResult *result,
                                        GError **error)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_ITEM (item), FALSE);
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (item),
	                      seahorse_gkr_update_description_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

typedef struct {
	gpointer request;
	GQueue *objects;
	GCancellable *cancellable;
	gulong cancelled_sig;
} delete_gkr_closure;

static void
delete_gkr_free (gpointer data)
{
	delete_gkr_closure *closure = data;
	g_queue_foreach (closure->objects, (GFunc)g_object_unref, NULL);
	g_queue_free (closure->objects);
	if (closure->cancellable && closure->cancelled_sig)
		g_signal_handler_disconnect (closure->cancellable,
		                             closure->cancelled_sig);
	g_clear_object (&closure->cancellable);
	g_assert (!closure->request);
	g_free (closure);
}

static void            delete_gkr_one_object         (GSimpleAsyncResult *res);

static void 
on_delete_gkr_complete (GnomeKeyringResult result,
                        gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	delete_gkr_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;
	SeahorseObject *object;

	closure->request = NULL;
	object = g_queue_pop_head (closure->objects);
	seahorse_progress_end (closure->cancellable, object);

	if (seahorse_gkr_propagate_error (result, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);

	} else {
		seahorse_context_remove_object (seahorse_context_instance (),
		                                object);
		delete_gkr_one_object (res);
	}

	g_object_unref (object);
}

static void
on_delete_gkr_cancelled (GCancellable *cancellable,
                         gpointer user_data)
{
	delete_gkr_closure *closure = user_data;

	if (closure->request)
		gnome_keyring_cancel_request (closure->request);
}

static void
delete_gkr_one_object (GSimpleAsyncResult *res)
{
	delete_gkr_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	SeahorseObject *object;
	const gchar *keyring;
	guint32 item;

	if (g_queue_is_empty (closure->objects)) {
		g_simple_async_result_complete_in_idle (res);
		return;
	}

	g_assert (!closure->request);
	object = g_queue_peek_head (closure->objects);

	seahorse_progress_begin (closure->cancellable, object);
	if (SEAHORSE_IS_GKR_ITEM (object)) {
		keyring = seahorse_gkr_item_get_keyring_name (SEAHORSE_GKR_ITEM (object));
		item = seahorse_gkr_item_get_item_id (SEAHORSE_GKR_ITEM (object));
		closure->request = gnome_keyring_item_delete (keyring, item,
		                                              on_delete_gkr_complete,
		                                              g_object_ref (res), g_object_unref);
	} else if (SEAHORSE_IS_GKR_KEYRING (object)) {
		keyring = seahorse_gkr_keyring_get_name (SEAHORSE_GKR_KEYRING (object));
		closure->request = gnome_keyring_delete (keyring,
		                                         on_delete_gkr_complete,
		                                         g_object_ref (res), g_object_unref);
	} else {
		g_assert_not_reached ();
	}
}

void
seahorse_gkr_delete_async (GList *objects,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
	GSimpleAsyncResult *res;
	delete_gkr_closure *closure;
	GList *l;

	res = g_simple_async_result_new (NULL, callback, user_data,
	                                 seahorse_gkr_delete_async);

	closure = g_new0 (delete_gkr_closure, 1);
	closure->objects = g_queue_new ();
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	g_simple_async_result_set_op_res_gpointer (res, closure, delete_gkr_free);

	for (l = objects; l != NULL; l = g_list_next (l)) {
		g_return_if_fail (SEAHORSE_IS_GKR_ITEM (l->data) || SEAHORSE_IS_GKR_KEYRING (l->data));
		g_queue_push_tail (closure->objects, g_object_ref (l->data));
		seahorse_progress_prep (cancellable, l->data, NULL);
	}

	delete_gkr_one_object (res);

	if (cancellable)
		closure->cancelled_sig = g_cancellable_connect (cancellable,
		                                                G_CALLBACK (on_delete_gkr_cancelled),
		                                                closure, NULL);

	g_object_unref (res);
}

gboolean
seahorse_gkr_delete_finish (GAsyncResult *result,
                            GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, NULL,
	                      seahorse_gkr_delete_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}
