/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

#ifndef __SEAHORSE_GPGME_UID_H__
#define __SEAHORSE_GPGME_UID_H__

#include <glib-object.h>

#include <gpgme.h>

#include "seahorse-object.h"

#include "pgp/seahorse-pgp-uid.h"

#define SEAHORSE_TYPE_GPGME_UID            (seahorse_gpgme_uid_get_type ())
#define SEAHORSE_GPGME_UID(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GPGME_UID, SeahorseGpgmeUid))
#define SEAHORSE_GPGME_UID_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GPGME_UID, SeahorseGpgmeUidClass))
#define SEAHORSE_IS_GPGME_UID(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GPGME_UID))
#define SEAHORSE_IS_GPGME_UID_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GPGME_UID))
#define SEAHORSE_GPGME_UID_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GPGME_UID, SeahorseGpgmeUidClass))

typedef struct _SeahorseGpgmeUid SeahorseGpgmeUid;
typedef struct _SeahorseGpgmeUidClass SeahorseGpgmeUidClass;
typedef struct _SeahorseGpgmeUidPrivate SeahorseGpgmeUidPrivate;

struct _SeahorseGpgmeUid {
	SeahorsePgpUid parent;
	SeahorseGpgmeUidPrivate *pv;
};

struct _SeahorseGpgmeUidClass {
	SeahorsePgpUidClass parent_class;
};

GType               seahorse_gpgme_uid_get_type             (void);

SeahorseGpgmeUid*   seahorse_gpgme_uid_new                  (gpgme_key_t pubkey,
                                                             gpgme_user_id_t userid);

gpgme_key_t         seahorse_gpgme_uid_get_pubkey           (SeahorseGpgmeUid *self);

gpgme_user_id_t     seahorse_gpgme_uid_get_userid           (SeahorseGpgmeUid *self);

void                seahorse_gpgme_uid_set_userid           (SeahorseGpgmeUid *self,
                                                             gpgme_user_id_t userid);

guint               seahorse_gpgme_uid_get_gpgme_index      (SeahorseGpgmeUid *self);

guint               seahorse_gpgme_uid_get_actual_index     (SeahorseGpgmeUid *self);

void                seahorse_gpgme_uid_set_actual_index     (SeahorseGpgmeUid *self,
                                                             guint actual_index);

gchar*              seahorse_gpgme_uid_calc_name            (gpgme_user_id_t userid);

gchar*              seahorse_gpgme_uid_calc_label           (gpgme_user_id_t userid);

gchar*              seahorse_gpgme_uid_calc_markup          (gpgme_user_id_t userid,
                                                             guint flags);

#endif /* __SEAHORSE_GPGME_UID_H__ */
