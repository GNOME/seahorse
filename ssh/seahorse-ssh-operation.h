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
 
#ifndef __SEAHORSE_SSH_OPERATION_H__
#define __SEAHORSE_SSH_OPERATION_H__

#include "seahorse-ssh-source.h"
#include "seahorse-ssh-key.h"

/* result: nothing */
void              seahorse_ssh_op_upload_async             (SeahorseSSHSource *source,
                                                            GList *keys,
                                                            const gchar *username,
                                                            const gchar *hostname,
                                                            const gchar *port,
                                                            GCancellable *cancellable,
                                                            GAsyncReadyCallback callback,
                                                            gpointer user_data);

gboolean          seahorse_ssh_op_upload_finish            (SeahorseSSHSource *source,
                                                            GAsyncResult *result,
                                                            GError **error);

void              seahorse_ssh_op_generate_async           (SeahorseSSHSource *source,
                                                            const gchar *email,
                                                            guint type,
                                                            guint bits,
                                                            GCancellable *cancellable,
                                                            GAsyncReadyCallback callback,
                                                            gpointer user_data);

SeahorseObject *  seahorse_ssh_op_generate_finish          (SeahorseSSHSource *source,
                                                            GAsyncResult *result,
                                                            GError **error);

void              seahorse_ssh_op_change_passphrase_async  (SeahorseSSHKey *key,
                                                            GCancellable *cancellable,
                                                            GAsyncReadyCallback callback,
                                                            gpointer user_data);

gboolean          seahorse_ssh_op_change_passphrase_finish (SeahorseSSHKey *key,
                                                            GAsyncResult *result,
                                                            GError **error);

void              seahorse_ssh_op_import_public_async      (SeahorseSSHSource *source,
                                                            SeahorseSSHKeyData *data,
                                                            const gchar* filename,
                                                            GCancellable *cancellable,
                                                            GAsyncReadyCallback callback,
                                                            gpointer user_data);

gchar *           seahorse_ssh_op_import_public_finish     (SeahorseSSHSource *source,
                                                            GAsyncResult *result,
                                                            GError **error);

void              seahorse_ssh_op_import_private_async     (SeahorseSSHSource *source,
                                                            SeahorseSSHSecData *data,
                                                            const gchar* filename,
                                                            GCancellable *cancellable,
                                                            GAsyncReadyCallback callback,
                                                            gpointer user_data);

gchar *           seahorse_ssh_op_import_private_finish    (SeahorseSSHSource *source,
                                                            GAsyncResult *result,
                                                            GError **error);

void              seahorse_ssh_op_authorize_async          (SeahorseSSHSource *source,
                                                            SeahorseSSHKey *skey,
                                                            gboolean authorize,
                                                            GCancellable *cancellable,
                                                            GAsyncReadyCallback callback,
                                                            gpointer user_data);

gboolean          seahorse_ssh_op_authorize_finish         (SeahorseSSHSource *source,
                                                            GAsyncResult *result,
                                                            GError **error);

void              seahorse_ssh_op_rename_async             (SeahorseSSHSource *source,
                                                            SeahorseSSHKey *key,
                                                            const gchar *newcomment,
                                                            GCancellable *cancellable,
                                                            GAsyncReadyCallback callback,
                                                            gpointer user_data);

gboolean          seahorse_ssh_op_rename_finish            (SeahorseSSHSource *source,
                                                            GAsyncResult *result,
                                                            GError **error);

gboolean          seahorse_ssh_op_delete_sync              (SeahorseSSHKey *key,
                                                            GError **error);

#endif /* __SEAHORSE_SSH_OPERATION_H__ */
