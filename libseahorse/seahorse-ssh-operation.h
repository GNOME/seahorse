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

/** 
 * SeahorsePGPOperation: Operations to be done on SSH keys
 * 
 * - Derived from SeahorseOperation
 *
 * Properties:
 *  TODO: 
 */
 
#ifndef __SEAHORSE_SSH_OPERATION_H__
#define __SEAHORSE_SSH_OPERATION_H__

#include "seahorse-operation.h"
#include "seahorse-ssh-source.h"
#include "seahorse-ssh-key.h"

#define SEAHORSE_TYPE_SSH_OPERATION            (seahorse_ssh_operation_get_type ())
#define SEAHORSE_SSH_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SSH_OPERATION, SeahorseSSHOperation))
#define SEAHORSE_SSH_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SSH_OPERATION, SeahorseSSHOperationClass))
#define SEAHORSE_IS_SSH_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SSH_OPERATION))
#define SEAHORSE_IS_SSH_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SSH_OPERATION))
#define SEAHORSE_SSH_OPERATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SSH_OPERATION, SeahorseSSHOperationClass))

DECLARE_OPERATION (SSH, ssh)

    /*< public >*/
    SeahorseSSHSource *sksrc;
    
END_DECLARE_OPERATION

SeahorseOperation*   seahorse_ssh_operation_new          (SeahorseSSHSource *ssrc,
                                                          const gchar *command, 
                                                          const gchar *input, 
                                                          gint length,
                                                          SeahorseSSHKey *key);

gchar*               seahorse_ssh_operation_sync         (SeahorseSSHSource *ssrc,
                                                          const gchar *command, 
                                                          GError **error);

/* result: nothing */
SeahorseOperation*   seahorse_ssh_operation_upload       (SeahorseSSHSource *ssrc,
                                                          GList *keys,
                                                          const gchar *username,
                                                          const gchar *hostname, 
                                                          const gchar *port);

/* result: the generated SeahorseKey */
SeahorseOperation*   seahorse_ssh_operation_generate     (SeahorseSSHSource *src, 
                                                          const gchar *email, 
                                                          guint type, 
                                                          guint bits);

/* result: nothing */
SeahorseOperation*   seahorse_ssh_operation_change_passphrase (SeahorseSSHKey *skey);

/* result: nothing */
SeahorseOperation*   seahorse_ssh_operation_agent_load     (SeahorseSSHSource *src, 
                                                            SeahorseSSHKey *skey);

/* result: fingerprint of imported key */
SeahorseOperation*   seahorse_ssh_operation_import_public  (SeahorseSSHSource *ssrc,
                                                            SeahorseSSHKeyData *data,
                                                            const gchar* filename);

/* result: fingerprint of imported key */
SeahorseOperation*   seahorse_ssh_operation_import_private (SeahorseSSHSource *ssrc, 
                                                            SeahorseSSHSecData *data,
                                                            const gchar* filename);

/* result: nothing */
SeahorseOperation*  seahorse_ssh_operation_authorize       (SeahorseSSHSource *ssrc,
                                                            SeahorseSSHKey *skey,
                                                            gboolean authorize);

/* result: nothing */
SeahorseOperation*  seahorse_ssh_operation_rename          (SeahorseSSHSource *ssrc,
                                                            SeahorseSSHKey *skey,
                                                            const gchar *newcomment);

#endif /* __SEAHORSE_SSH_OPERATION_H__ */
