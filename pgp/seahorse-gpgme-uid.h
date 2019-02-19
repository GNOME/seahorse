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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib-object.h>

#include <gpgme.h>

#include "seahorse-common.h"

#include "pgp/seahorse-gpgme-key.h"
#include "pgp/seahorse-pgp-uid.h"

#define SEAHORSE_GPGME_TYPE_UID (seahorse_gpgme_uid_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseGpgmeUid, seahorse_gpgme_uid,
                      SEAHORSE_GPGME, UID,
                      SeahorsePgpUid)

SeahorseGpgmeUid*   seahorse_gpgme_uid_new                  (SeahorseGpgmeKey *parent,
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

gboolean            seahorse_gpgme_uid_is_same              (SeahorseGpgmeUid *self,
                                                             gpgme_user_id_t userid);
