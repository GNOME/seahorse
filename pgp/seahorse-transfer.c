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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "operation"

#include "seahorse-transfer.h"

#include "seahorse-server-source.h"
#include "seahorse-gpgme-key-export-operation.h"
#include "seahorse-gpgme-keyring.h"

#include "seahorse-common.h"

#include "libseahorse/seahorse-progress.h"
#include "libseahorse/seahorse-util.h"

#include <glib/gi18n.h>

#include <stdlib.h>

typedef struct {
    SeahorsePlace *from;
    SeahorsePlace *to;
    char **keyids;
    GList *keys;
} TransferClosure;

static void
transfer_closure_free (gpointer user_data)
{
    TransferClosure *closure = user_data;
    g_clear_object (&closure->from);
    g_clear_object (&closure->to);
    g_strfreev (closure->keyids);
    g_list_free_full (closure->keys, g_object_unref);
    g_free (closure);
}

#if 0
static void
on_source_import_ready (GObject *object,
                        GAsyncResult *result,
                        gpointer user_data)
{
    g_autoptr(GTask) task = G_TASK (user_data);
    TransferClosure *closure = g_task_get_task_data (task);
    GCancellable *cancellable = g_task_get_cancellable (task);
    GError *error = NULL;
    g_autoptr(GList) results = NULL;

    g_debug ("[transfer] import done");
    seahorse_progress_end (cancellable, &closure->to);

    if (SEAHORSE_IS_GPGME_KEYRING (closure->to)) {
        results = seahorse_gpgme_keyring_import_finish (SEAHORSE_GPGME_KEYRING (closure->to),
                                                        result, &error);
    } else {
        results = seahorse_server_source_import_finish (SEAHORSE_SERVER_SOURCE (closure->to),
                                                        result, &error);
    }

    if (results != NULL)
        g_cancellable_set_error_if_cancelled (cancellable, &error);

    if (error != NULL)
        g_task_return_error (task, g_steal_pointer (&error));
    else
        g_task_return_boolean (task, TRUE);
}

static void
on_source_export_ready (GObject *object,
                        GAsyncResult *result,
                        gpointer user_data)
{
    g_autoptr(GTask) task = G_TASK (user_data);
    TransferClosure *closure = g_task_get_task_data (task);
    GCancellable *cancellable = g_task_get_cancellable (task);
    GError *error = NULL;
    gsize stream_size;
    g_autoptr(GInputStream) input = NULL;
    gpointer stream_data = NULL;

    g_debug ("[transfer] export done");
    seahorse_progress_end (cancellable, &closure->from);

    if (SEAHORSE_IS_SERVER_SOURCE (closure->from)) {
        GBytes *stream_bytes;

        stream_bytes = seahorse_server_source_export_finish (SEAHORSE_SERVER_SOURCE (object),
                                                             result, &error);
        stream_data = g_bytes_unref_to_data (stream_bytes, &stream_size);

    } else if (SEAHORSE_IS_GPGME_KEYRING (closure->from)) {
        stream_data = seahorse_exporter_export_finish (SEAHORSE_EXPORTER (object), result,
                                                       &stream_size, &error);

    } else {
        g_warning ("unsupported source for export: %s", G_OBJECT_TYPE_NAME (object));
    }

    if (error == NULL)
        g_cancellable_set_error_if_cancelled (cancellable, &error);

    if (error != NULL) {
        g_debug ("[transfer] stopped after export");
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    seahorse_progress_begin (cancellable, &closure->to);

    if (!stream_size) {
        g_debug ("[transfer] nothing to import");
        seahorse_progress_end (cancellable, &closure->to);
        g_task_return_boolean (task, TRUE);
        return;
    }

    input = g_memory_input_stream_new_from_data (stream_data, stream_size, g_free);
    stream_data = NULL;
    stream_size = 0;

    g_debug ("[transfer] starting import");
    if (SEAHORSE_IS_GPGME_KEYRING (closure->to)) {
        seahorse_gpgme_keyring_import_async (SEAHORSE_GPGME_KEYRING (closure->to),
                                             input, cancellable,
                                             on_source_import_ready,
                                             g_steal_pointer (&task));
    } else {
        seahorse_server_source_import_async (SEAHORSE_SERVER_SOURCE (closure->to),
                                             input, cancellable,
                                             on_source_import_ready,
                                             g_steal_pointer (&task));
    }

    g_free (stream_data);
}
#endif

static gboolean
on_timeout_start_transfer (gpointer user_data)
{
    GTask *task = G_TASK (user_data);
    TransferClosure *closure = g_task_get_task_data (task);
    GCancellable *cancellable = g_task_get_cancellable (task);

    g_assert (SEAHORSE_IS_PLACE (closure->from));

    //XXX
#if 0
    seahorse_progress_begin (cancellable, &closure->from);
    if (SEAHORSE_IS_SERVER_SOURCE (closure->from)) {
        g_assert (closure->keyids != NULL);
        seahorse_server_source_export_async (SEAHORSE_SERVER_SOURCE (closure->from),
                                             (const char **) closure->keyids,
                                             cancellable, on_source_export_ready,
                                             g_object_ref (task));
        return G_SOURCE_REMOVE;
    }

    if (SEAHORSE_IS_GPGME_KEYRING (closure->from)) {
        SeahorseExporter *exporter = NULL;

        g_assert (closure->keys != NULL);
        exporter = seahorse_gpgme_exporter_new_multiple (closure->keys, TRUE);
        seahorse_exporter_export (exporter, cancellable,
                                  on_source_export_ready, g_object_ref (task));
        g_object_unref (exporter);
        return G_SOURCE_REMOVE;
    }
#endif

    g_warning ("unsupported source for transfer: %s", G_OBJECT_TYPE_NAME (closure->from));
    return G_SOURCE_REMOVE;
}

void
seahorse_transfer_keys_async (SeahorsePlace *from,
                              SeahorsePlace *to,
                              GList *keys,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
    g_autoptr(GTask) task = NULL;
    TransferClosure *closure;

    g_return_if_fail (SEAHORSE_IS_PLACE (from));
    g_return_if_fail (SEAHORSE_IS_PLACE (to));

    task = g_task_new (NULL, cancellable, callback, user_data);
    g_task_set_source_tag (task, seahorse_transfer_finish);

    if (!keys) {
        g_task_return_boolean (task, TRUE);
        return;
    }

    closure = g_new0 (TransferClosure, 1);
    closure->from = g_object_ref (from);
    closure->to = g_object_ref (to);
    g_task_set_task_data (task, closure, transfer_closure_free);

    if (SEAHORSE_IS_GPGME_KEYRING (from)) {
        closure->keys = g_list_copy_deep (keys, (GCopyFunc) g_object_ref, NULL);

    } else {
        GPtrArray *keyids;
        GList *l;

        keyids = g_ptr_array_new ();
        for (l = keys; l != NULL; l = g_list_next (l))
            g_ptr_array_add (keyids, g_strdup (seahorse_pgp_key_get_keyid (l->data)));
        g_ptr_array_add (keyids, NULL);
        closure->keyids = (char **)g_ptr_array_free (keyids, FALSE);
    }

    seahorse_progress_prep (cancellable, &closure->from,
                            SEAHORSE_IS_GPGME_KEYRING (closure->from) ?
                            _("Exporting data") : _("Retrieving data"));
    seahorse_progress_prep (cancellable, &closure->to,
                            SEAHORSE_IS_GPGME_KEYRING (closure->to) ?
                            _("Importing data") : _("Sending data"));

    g_debug ("starting export");

    /* We delay and continue from a callback */
    g_timeout_add_seconds_full (G_PRIORITY_DEFAULT, 0,
                                on_timeout_start_transfer,
                                g_steal_pointer (&task), g_object_unref);
}

void
seahorse_transfer_keyids_async (SeahorseServerSource *from,
                                SeahorsePlace *to,
                                const char **keyids,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
    g_autoptr(GTask) task = NULL;
    TransferClosure *closure;

    g_return_if_fail (SEAHORSE_IS_SERVER_SOURCE (from));
    g_return_if_fail (SEAHORSE_PLACE (to));

    task = g_task_new (NULL, cancellable, callback, user_data);
    g_task_set_source_tag (task, seahorse_transfer_finish);

    if (!keyids || !keyids[0]) {
        g_task_return_boolean (task, TRUE);
        return;
    }

    closure = g_new0 (TransferClosure, 1);
    closure->from = SEAHORSE_PLACE (g_object_ref (from));
    closure->to = g_object_ref (to);
    closure->keyids = g_strdupv ((char **)keyids);
    g_task_set_task_data (task, closure, transfer_closure_free);

    seahorse_progress_prep (cancellable, &closure->from,
                            SEAHORSE_IS_GPGME_KEYRING (closure->from) ?
                            _("Exporting data") : _("Retrieving data"));
    seahorse_progress_prep (cancellable, &closure->to,
                            SEAHORSE_IS_GPGME_KEYRING (closure->to) ?
                            _("Importing data") : _("Sending data"));

    g_debug ("starting export");

    /* We delay and continue from a callback */
    g_timeout_add_seconds_full (G_PRIORITY_DEFAULT, 0,
                                on_timeout_start_transfer,
                                g_steal_pointer (&task), g_object_unref);
}

gboolean
seahorse_transfer_finish (GAsyncResult *result,
                          GError **error)
{
    g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);
    g_return_val_if_fail (g_task_get_source_tag (G_TASK (result))
                          == seahorse_transfer_finish, FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}
