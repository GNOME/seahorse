/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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

#ifndef __SEAHORSE_SSH_KEY_H__
#define __SEAHORSE_SSH_KEY_H__

#include <gtk/gtk.h>

#include "seahorse-object.h"
#include "seahorse-place.h"
#include "seahorse-validity.h"

#include "seahorse-ssh-key-data.h"

/* For vala's sake */
#define SEAHORSE_SSH_TYPE_KEY            SEAHORSE_TYPE_SSH_KEY

#define SEAHORSE_TYPE_SSH_KEY            (seahorse_ssh_key_get_type ())
#define SEAHORSE_SSH_KEY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SSH_KEY, SeahorseSSHKey))
#define SEAHORSE_SSH_KEY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SSH_KEY, SeahorseSSHKeyClass))
#define SEAHORSE_IS_SSH_KEY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SSH_KEY))
#define SEAHORSE_IS_SSH_KEY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SSH_KEY))
#define SEAHORSE_SSH_KEY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SSH_KEY, SeahorseSSHKeyClass))

typedef struct _SeahorseSSHKey SeahorseSSHKey;
typedef struct _SeahorseSSHKeyClass SeahorseSSHKeyClass;
typedef struct _SeahorseSSHKeyPrivate SeahorseSSHKeyPrivate;

struct _SeahorseSSHKey {
    SeahorseObject	parent;

    /*< public >*/
    struct _SeahorseSSHKeyData *keydata;
        
    /*< private >*/
    struct _SeahorseSSHKeyPrivate *priv;
};


struct _SeahorseSSHKeyClass {
    SeahorseObjectClass            parent_class;
};

SeahorseSSHKey*         seahorse_ssh_key_new                  (SeahorsePlace *sksrc,
                                                               SeahorseSSHKeyData *data);

void                    seahorse_ssh_key_refresh              (SeahorseSSHKey *self);

GType                   seahorse_ssh_key_get_type             (void);

guint                   seahorse_ssh_key_get_algo             (SeahorseSSHKey *skey);

const gchar*            seahorse_ssh_key_get_algo_str         (SeahorseSSHKey *skey);

guint                   seahorse_ssh_key_get_strength         (SeahorseSSHKey *skey);

const gchar*            seahorse_ssh_key_get_location         (SeahorseSSHKey *skey);

void             seahorse_ssh_key_op_change_passphrase_async  (SeahorseSSHKey *self,
                                                               GCancellable *cancellable,
                                                               GAsyncReadyCallback callback,
                                                               gpointer user_data);

gboolean         seahorse_ssh_key_op_change_passphrase_finish (SeahorseSSHKey *self,
                                                               GAsyncResult *result,
                                                               GError **error);

SeahorseValidity        seahorse_ssh_key_get_trust            (SeahorseSSHKey *self);

gchar*                  seahorse_ssh_key_get_fingerprint      (SeahorseSSHKey *self);

gchar*                  seahorse_ssh_key_calc_identifier      (const gchar *id);

#endif /* __SEAHORSE_SSH_KEY_H__ */
