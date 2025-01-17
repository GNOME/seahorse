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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"

#include "seahorse-gpgme.h"
#include "seahorse-gpgme-data.h"
#include "seahorse-gpgme-key-export-operation.h"
#include "seahorse-gpgme-keyring.h"
#include "seahorse-gpg-op.h"

#include "libseahorse/seahorse-progress.h"
#include "libseahorse/seahorse-util.h"

#include <glib/gi18n.h>

#include <string.h>

struct _SeahorseGpgmeKeyExportOperation {
    SeahorseExportOperation parent;

    SeahorseGpgmeKey *key;
    gboolean armor;
    gboolean secret;
};

enum {
    PROP_0,
    PROP_KEY,
    PROP_ARMOR,
    PROP_SECRET,
    N_PROPS
};
static GParamSpec *obj_props[N_PROPS] = { NULL, };

G_DEFINE_TYPE (SeahorseGpgmeKeyExportOperation, seahorse_gpgme_key_export_operation,
               SEAHORSE_TYPE_EXPORT_OPERATION);

#define BAD_FILENAME_CHARS "/\\<>|:?;"

typedef struct {
    gpgme_data_t data;
    gpgme_ctx_t gctx;
} GpgmeExportClosure;

static void
gpgme_export_closure_free (void *data)
{
    GpgmeExportClosure *closure = data;
    gpgme_data_release (closure->data);
    g_clear_pointer (&closure->gctx, gpgme_release);
    g_free (closure);
}


static gboolean
on_keyring_export_complete (gpgme_error_t gerr,
                            void *user_data)
{
    GTask *task = G_TASK (user_data);
    g_autoptr(GError) error = NULL;

    if (seahorse_gpgme_propagate_error (gerr, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return G_SOURCE_REMOVE;
    }

    seahorse_progress_end (g_task_get_cancellable (task), task);
    g_task_return_boolean (task, TRUE);
    return G_SOURCE_REMOVE;
}

static void
seahorse_gpgme_key_export_operation_execute (SeahorseExportOperation *export_op,
                                             GCancellable *cancellable,
                                             GAsyncReadyCallback callback,
                                             void *user_data)
{
    SeahorseGpgmeKeyExportOperation *self = SEAHORSE_GPGME_KEY_EXPORT_OPERATION (export_op);
    g_autoptr(GTask) task = NULL;
    GOutputStream *output;
    GpgmeExportClosure *closure;
    g_autoptr(GError) error = NULL;
    gpgme_error_t gerr = 0;
    g_autoptr(GSource) gsource = NULL;
    gpgme_export_mode_t flags = 0;
    const char *keyid;

    task = g_task_new (self, cancellable, callback, user_data);
    closure = g_new0 (GpgmeExportClosure, 1);
    closure->gctx = seahorse_gpgme_keyring_new_context (&gerr);
    g_task_set_task_data (task, closure, gpgme_export_closure_free);

    output = seahorse_export_operation_get_output (SEAHORSE_EXPORT_OPERATION (self));
    closure->data = seahorse_gpgme_data_output (output);

    if (seahorse_gpgme_propagate_error (gerr, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    gpgme_set_armor (closure->gctx, self->armor);
    if (self->secret)
        flags |= GPGME_EXPORT_MODE_SECRET;

    keyid = seahorse_pgp_key_get_keyid (SEAHORSE_PGP_KEY (self->key));

    seahorse_progress_prep_and_begin (cancellable, task, NULL);
    gsource = seahorse_gpgme_gsource_new (closure->gctx, cancellable);
    g_source_set_callback (gsource, (GSourceFunc) on_keyring_export_complete,
                           g_object_ref (task), g_object_unref);

    if (seahorse_gpgme_propagate_error (gerr, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    } else {
        gerr = gpgme_op_export_start (closure->gctx,
                                      keyid,
                                      flags,
                                      closure->data);
    }

    g_source_attach (gsource, g_main_context_default ());
}

static gboolean
seahorse_gpgme_key_export_operation_execute_finish (SeahorseExportOperation *export_op,
                                                    GAsyncResult *result,
                                                    GError **error)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY_EXPORT_OPERATION (export_op), FALSE);
    g_return_val_if_fail (g_task_is_valid (result, export_op), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

static char *
get_suggested_filename (SeahorseGpgmeKeyExportOperation *self)
{
    const char *basename = NULL;
    char *filename;

    basename = seahorse_pgp_key_get_primary_name (SEAHORSE_PGP_KEY (self->key));
    if (basename == NULL)
        basename = _("Key Data");

    if (self->armor)
        filename = g_strconcat (basename, ".asc", NULL);
    else
        filename = g_strconcat (basename, ".pgp", NULL);
    g_strstrip (filename);
    g_strdelimit (filename, BAD_FILENAME_CHARS, '_');
    return filename;
}

static GtkFileDialog *
seahorse_gpgme_key_export_operation_create_file_dialog (SeahorseExportOperation *export_op,
                                                        GtkWindow *parent)
{
    SeahorseGpgmeKeyExportOperation *self = SEAHORSE_GPGME_KEY_EXPORT_OPERATION (export_op);
    g_autoptr(GtkFileDialog) dialog = NULL;
    g_autofree char *filename = NULL;
    g_autoptr(GListStore) filters = NULL;
    g_autoptr(GtkFileFilter) filter = NULL;

    dialog = gtk_file_dialog_new ();
    gtk_file_dialog_set_title (dialog, _("Export GPGME key"));
    gtk_file_dialog_set_accept_label (dialog, _("Export"));

    filename = get_suggested_filename (self);
    gtk_file_dialog_set_initial_name (dialog, filename);

    filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
    filter = gtk_file_filter_new ();
    if (self->armor) {
        gtk_file_filter_set_name (filter, _("Armored PGP keys"));
        gtk_file_filter_add_mime_type (filter, "application/pgp-keys+armor");
        gtk_file_filter_add_pattern (filter, "*.asc");
    } else {
        gtk_file_filter_set_name (filter, _("PGP keys"));
        gtk_file_filter_add_mime_type (filter, "application/pgp-keys");
        gtk_file_filter_add_pattern (filter, "*.pgp");
        gtk_file_filter_add_pattern (filter, "*.gpg");
    }
    g_list_store_append (filters, filter);
    gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

    return g_steal_pointer (&dialog);
}

static void
seahorse_gpgme_key_export_operation_init (SeahorseGpgmeKeyExportOperation *self)
{

}

static void
seahorse_gpgme_key_export_operation_get_property (GObject *object,
                                                  unsigned int prop_id,
                                                  GValue *value,
                                                  GParamSpec *pspec)
{
    SeahorseGpgmeKeyExportOperation *self = SEAHORSE_GPGME_KEY_EXPORT_OPERATION (object);

    switch (prop_id) {
    case PROP_KEY:
        g_value_set_object (value, self->key);
        break;
    case PROP_ARMOR:
        g_value_set_boolean (value, self->armor);
        break;
    case PROP_SECRET:
        g_value_set_boolean (value, self->secret);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_key_export_operation_set_property (GObject *object,
                                                  unsigned int prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec)
{
    SeahorseGpgmeKeyExportOperation *self = SEAHORSE_GPGME_KEY_EXPORT_OPERATION (object);

    switch (prop_id) {
    case PROP_KEY:
        g_set_object (&self->key, g_value_get_object (value));
        break;
    case PROP_ARMOR:
        self->armor = g_value_get_boolean (value);
        break;
    case PROP_SECRET:
        self->secret = g_value_get_boolean (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_key_export_operation_finalize (GObject *obj)
{
    SeahorseGpgmeKeyExportOperation *self = SEAHORSE_GPGME_KEY_EXPORT_OPERATION (obj);

    g_clear_object (&self->key);

    G_OBJECT_CLASS (seahorse_gpgme_key_export_operation_parent_class)->finalize (obj);
}

static void
seahorse_gpgme_key_export_operation_class_init (SeahorseGpgmeKeyExportOperationClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    SeahorseExportOperationClass *export_op_class = SEAHORSE_EXPORT_OPERATION_CLASS (klass);

    export_op_class->create_file_dialog = seahorse_gpgme_key_export_operation_create_file_dialog;
    export_op_class->execute = seahorse_gpgme_key_export_operation_execute;
    export_op_class->execute_finish = seahorse_gpgme_key_export_operation_execute_finish;

    gobject_class->finalize = seahorse_gpgme_key_export_operation_finalize;
    gobject_class->set_property = seahorse_gpgme_key_export_operation_set_property;
    gobject_class->get_property = seahorse_gpgme_key_export_operation_get_property;

    obj_props[PROP_KEY] =
        g_param_spec_object ("key", NULL, NULL,
                             SEAHORSE_GPGME_TYPE_KEY,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    obj_props[PROP_ARMOR] =
        g_param_spec_boolean ("armor", NULL, NULL,
                              FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    obj_props[PROP_SECRET] =
        g_param_spec_boolean ("secret", NULL, NULL,
                              FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class, N_PROPS, obj_props);
}

SeahorseExportOperation *
seahorse_gpgme_key_export_operation_new (SeahorseGpgmeKey *key,
                                         gboolean armor,
                                         gboolean secret)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY (key), NULL);
    g_return_val_if_fail (!secret || seahorse_gpgme_key_get_private (key) != NULL, NULL);
    g_return_val_if_fail (secret == FALSE || armor == TRUE, NULL);

    return g_object_new (SEAHORSE_GPGME_TYPE_KEY_EXPORT_OPERATION,
                         "key", key,
                         "armor", armor,
                         "secret", secret,
                         NULL);
}
