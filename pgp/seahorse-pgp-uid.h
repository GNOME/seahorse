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

#ifndef __SEAHORSE_PGP_UID_H__
#define __SEAHORSE_PGP_UID_H__

#include <gtk/gtk.h>
#include <gpgme.h>

#include "seahorse-object.h"

#include "pgp/seahorse-pgp-module.h"
#include "pgp/seahorse-gpgmex.h"

#define SEAHORSE_TYPE_PGP_UID            (seahorse_pgp_uid_get_type ())

/* For vala's sake */
#define SEAHORSE_PGP_TYPE_UID 		SEAHORSE_TYPE_PGP_UID

#define SEAHORSE_PGP_UID(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PGP_UID, SeahorsePgpUid))
#define SEAHORSE_PGP_UID_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PGP_UID, SeahorsePgpUidClass))
#define SEAHORSE_IS_PGP_UID(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PGP_UID))
#define SEAHORSE_IS_PGP_UID_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PGP_UID))
#define SEAHORSE_PGP_UID_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PGP_UID, SeahorsePgpUidClass))

typedef struct _SeahorsePgpUid SeahorsePgpUid;
typedef struct _SeahorsePgpUidClass SeahorsePgpUidClass;
typedef struct _SeahorsePgpUidPrivate SeahorsePgpUidPrivate;

struct _SeahorsePgpUid {
	SeahorseObject parent;
	SeahorsePgpUidPrivate *pv;
};

struct _SeahorsePgpUidClass {
	SeahorseObjectClass parent_class;
};

GType             seahorse_pgp_uid_get_type             (void);

SeahorsePgpUid*   seahorse_pgp_uid_new                  (gpgme_key_t pubkey,
                                                         gpgme_user_id_t userid);

gpgme_key_t       seahorse_pgp_uid_get_pubkey           (SeahorsePgpUid *self);

gpgme_user_id_t   seahorse_pgp_uid_get_userid           (SeahorsePgpUid *self);

void              seahorse_pgp_uid_set_userid           (SeahorsePgpUid *self,
                                                         gpgme_user_id_t userid);

guint             seahorse_pgp_uid_get_gpgme_index      (SeahorsePgpUid *self);

guint             seahorse_pgp_uid_get_actual_index     (SeahorsePgpUid *self);

void              seahorse_pgp_uid_set_actual_index     (SeahorsePgpUid *self,
                                                         guint actual_index);

SeahorseValidity  seahorse_pgp_uid_get_validity         (SeahorsePgpUid *self);

const gchar*      seahorse_pgp_uid_get_validity_str     (SeahorsePgpUid *self);


gchar*            seahorse_pgp_uid_get_name             (SeahorsePgpUid *self);

gchar*            seahorse_pgp_uid_get_email            (SeahorsePgpUid *self);

gchar*            seahorse_pgp_uid_get_comment          (SeahorsePgpUid *self);

gchar*            seahorse_pgp_uid_calc_name            (gpgme_user_id_t userid);

gchar*            seahorse_pgp_uid_calc_label           (gpgme_user_id_t userid);

gchar*            seahorse_pgp_uid_calc_markup          (gpgme_user_id_t userid,
                                                         guint flags);

guint             seahorse_pgp_uid_signature_get_type   (gpgme_key_sig_t  signature);

void              seahorse_pgp_uid_signature_get_text   (gpgme_key_sig_t  signature,
                                                         gchar            **name,
                                                         gchar            **email,
                                                         gchar            **comment);

GQuark            seahorse_pgp_uid_get_cannonical_id    (const gchar *id);

#endif /* __SEAHORSE_PGP_UID_H__ */
