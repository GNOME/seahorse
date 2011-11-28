/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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

#include "config.h"

#include "seahorse-server-source.h"
#include "seahorse-gpgme-exporter.h"
#include "seahorse-gpgme-keyring.h"

#define DEBUG_FLAG SEAHORSE_DEBUG_OPERATION
#include "seahorse-debug.h"
#include "seahorse-exporter.h"
#include "seahorse-object-list.h"
#include "seahorse-progress.h"
#include "seahorse-transfer.h"
#include "seahorse-util.h"

#include <glib/gi18n.h>

#include <stdlib.h>

typedef struct {
	GCancellable *cancellable;
	SeahorsePlace *from;
	SeahorsePlace *to;
	GList *keys;
} TransferClosure;

static void
transfer_closure_free (gpointer user_data)
{
	TransferClosure *closure = user_data;
	g_clear_object (&closure->from);
	g_clear_object (&closure->to);
	g_clear_object (&closure->cancellable);
	seahorse_object_list_free (closure->keys);
	g_free (closure);
}

static void
on_source_import_ready (GObject *object,
                        GAsyncResult *result,
                        gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	TransferClosure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	seahorse_debug ("[transfer] import done");
	seahorse_progress_end (closure->cancellable, &closure->to);

	if (seahorse_place_import_finish (closure->to, result, &error))
		g_cancellable_set_error_if_cancelled (closure->cancellable, &error);

	if (error != NULL)
		g_simple_async_result_take_error (res, error);

	g_simple_async_result_complete (res);
	g_object_unref (user_data);
}

static void
on_source_export_ready (GObject *object,
                        GAsyncResult *result,
                        gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	TransferClosure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;
	gpointer stream_data = NULL;
	gsize stream_size;
	GInputStream *input;

	seahorse_debug ("[transfer] export done");
	seahorse_progress_end (closure->cancellable, &closure->from);

	if (SEAHORSE_IS_SERVER_SOURCE (closure->from)) {
		stream_data = seahorse_server_source_export_finish (SEAHORSE_SERVER_SOURCE (object),
		                                                    result, &stream_size, &error);

	} else if (SEAHORSE_IS_GPGME_KEYRING (closure->from)) {
		stream_data = seahorse_exporter_export_finish (SEAHORSE_EXPORTER (object), result,
		                                               &stream_size, &error);

	} else {
		g_warning ("unsupported source for export: %s", G_OBJECT_TYPE_NAME (object));
	}

	if (error == NULL)
		g_cancellable_set_error_if_cancelled (closure->cancellable, &error);

	if (error == NULL) {
		seahorse_progress_begin (closure->cancellable, &closure->to);

		if (!stream_size) {
			seahorse_debug ("[transfer] nothing to import");
			seahorse_progress_end (closure->cancellable, &closure->to);
			g_simple_async_result_complete (res);

		} else {
			input = g_memory_input_stream_new_from_data (stream_data, stream_size, g_free);
			stream_data = NULL;
			stream_size = 0;

			seahorse_debug ("[transfer] starting import");
			seahorse_place_import_async (closure->to, input, closure->cancellable,
			                             on_source_import_ready, g_object_ref (res));
			g_object_unref (input);
		}

	} else {
		seahorse_debug ("[transfer] stopped after export");
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);
	}

	g_free (stream_data);
	g_object_unref (user_data);
}

static gboolean
on_timeout_start_transfer (gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	TransferClosure *closure = g_simple_async_result_get_op_res_gpointer (res);
	SeahorseExporter *exporter;
	GList *keyids, *l;

	g_assert (SEAHORSE_IS_PLACE (closure->from));
	g_assert (closure->keys);

	seahorse_progress_begin (closure->cancellable, &closure->from);
	if (SEAHORSE_IS_SERVER_SOURCE (closure->from)) {
		keyids = NULL;
		for (l = closure->keys; l != NULL; l = g_list_next (l))
			keyids = g_list_prepend (keyids, (gpointer)seahorse_pgp_key_get_keyid (l->data));
		keyids = g_list_reverse (keyids);
		seahorse_server_source_export_async (SEAHORSE_SERVER_SOURCE (closure->from),
		                                     keyids, closure->cancellable,
		                                     on_source_export_ready, g_object_ref (res));
		g_list_free (keyids);

	} else if (SEAHORSE_IS_GPGME_KEYRING (closure->from)) {
		exporter = seahorse_gpgme_exporter_new_multiple (closure->keys, TRUE);
		seahorse_exporter_export_async (exporter, closure->cancellable,
		                                on_source_export_ready, g_object_ref (res));
		g_object_unref (exporter);

	} else {
		g_warning ("unsupported source for transfer: %s", G_OBJECT_TYPE_NAME (closure->from));
	}

	return FALSE; /* Don't run again */
}

void
seahorse_transfer_async (SeahorsePlace *from,
                         SeahorsePlace *to,
                         GList *keys,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
	GSimpleAsyncResult *res;
	TransferClosure *closure;

	g_return_if_fail (SEAHORSE_PLACE (from));
	g_return_if_fail (SEAHORSE_PLACE (to));

	res = g_simple_async_result_new (NULL, callback, user_data,
	                                 seahorse_transfer_async);

	if (!keys) {
		g_simple_async_result_complete_in_idle (res);
		g_object_unref (res);
		return;
	}

	closure = g_new0 (TransferClosure, 1);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : cancellable;
	closure->from = g_object_ref (from);
	closure->to = g_object_ref (to);
	closure->keys = seahorse_object_list_copy (keys);
	g_simple_async_result_set_op_res_gpointer (res, closure, transfer_closure_free);

	seahorse_progress_prep (cancellable, &closure->from,
	                        SEAHORSE_IS_GPGME_KEYRING (closure->from) ?
	                        _("Exporting data") : _("Retrieving data"));
	seahorse_progress_prep (cancellable, &closure->to,
	                        SEAHORSE_IS_GPGME_KEYRING (closure->to) ?
	                        _("Importing data") : _("Sending data"));

	seahorse_debug ("starting export");

	/* We delay and continue from a callback */
	g_timeout_add_seconds_full (G_PRIORITY_DEFAULT, 0,
	                            on_timeout_start_transfer,
	                            g_object_ref (res), g_object_unref);

	g_object_unref (res);
}

gboolean
seahorse_transfer_finish (GAsyncResult *result,
                          GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, NULL,
	                      seahorse_transfer_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}
