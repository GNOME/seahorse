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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"

#include "seahorse-gpgme.h"
#include "seahorse-gpgme-subkey-delete-operation.h"
#include "seahorse-gpgme-key-op.h"

#include "seahorse-common.h"

#include <glib/gi18n.h>

struct _SeahorseGpgmeSubkeyDeleteOperation {
    SeahorseDeleteOperation parent;
};

G_DEFINE_TYPE (SeahorseGpgmeSubkeyDeleteOperation, seahorse_gpgme_subkey_delete_operation, SEAHORSE_TYPE_DELETE_OPERATION);

void
seahorse_gpgme_subkey_delete_operation_add_subkey (SeahorseGpgmeSubkeyDeleteOperation *self,
                                                   SeahorseGpgmeSubkey *subkey)
{
    SeahorseDeleteOperation *delete_op = SEAHORSE_DELETE_OPERATION (self);

    g_return_if_fail (SEAHORSE_GPGME_IS_SUBKEY_DELETE_OPERATION (self));
    g_return_if_fail (SEAHORSE_GPGME_IS_SUBKEY (subkey));

    if (seahorse_delete_operation_contains (delete_op, SEAHORSE_DELETABLE (subkey)))
        return;

    g_ptr_array_add (delete_op->items, g_object_ref (subkey));
}

static void
seahorse_gpgme_subkey_delete_operation_execute (SeahorseDeleteOperation *delete_op,
                                                GCancellable            *cancellable,
                                                GAsyncReadyCallback      callback,
                                                void                    *user_data)
{
    SeahorseGpgmeSubkeyDeleteOperation *self = SEAHORSE_GPGME_SUBKEY_DELETE_OPERATION (delete_op);
    g_autoptr(GTask) task = NULL;

    task = g_task_new (self, cancellable, callback, user_data);

    for (unsigned int i = 0; i < delete_op->items->len; i++) {
        SeahorseGpgmeSubkey *subkey;
        gpgme_error_t gerr;
        g_autoptr(GError) error = NULL;

        subkey = SEAHORSE_GPGME_SUBKEY (g_ptr_array_index (delete_op->items, i));
        gerr = seahorse_gpgme_key_op_del_subkey (subkey);
        if (seahorse_gpgme_propagate_error (gerr, &error)) {
            g_task_return_error (task, g_steal_pointer (&error));
            return;
        }
    }

    g_task_return_boolean (task, TRUE);
}

static gboolean
seahorse_gpgme_subkey_delete_operation_execute_finish (SeahorseDeleteOperation *delete_op,
                                                       GAsyncResult            *result,
                                                       GError                 **error)
{
    SeahorseGpgmeSubkeyDeleteOperation *self = SEAHORSE_GPGME_SUBKEY_DELETE_OPERATION (delete_op);

    g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
seahorse_gpgme_subkey_delete_operation_init (SeahorseGpgmeSubkeyDeleteOperation *self)
{
}

static void
seahorse_gpgme_subkey_delete_operation_class_init (SeahorseGpgmeSubkeyDeleteOperationClass *klass)
{
    SeahorseDeleteOperationClass *delete_op_class = SEAHORSE_DELETE_OPERATION_CLASS (klass);

    delete_op_class->execute = seahorse_gpgme_subkey_delete_operation_execute;
    delete_op_class->execute_finish = seahorse_gpgme_subkey_delete_operation_execute_finish;
}

SeahorseDeleteOperation *
seahorse_gpgme_subkey_delete_operation_new (SeahorseGpgmeSubkey *subkey)
{
    g_autoptr(SeahorseGpgmeSubkeyDeleteOperation) self = NULL;

    self = g_object_new (SEAHORSE_GPGME_TYPE_SUBKEY_DELETE_OPERATION,
                         NULL);
    seahorse_gpgme_subkey_delete_operation_add_subkey (self, subkey);

    return SEAHORSE_DELETE_OPERATION (g_steal_pointer (&self));
}
