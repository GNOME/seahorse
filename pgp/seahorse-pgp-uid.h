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

#include "seahorse-common.h"

#include "seahorse-pgp-types.h"

#define SEAHORSE_PGP_TYPE_UID (seahorse_pgp_uid_get_type ())
G_DECLARE_DERIVABLE_TYPE (SeahorsePgpUid, seahorse_pgp_uid, SEAHORSE_PGP, UID, GObject)

struct _SeahorsePgpUidClass {
    GObjectClass parent_class;
};

SeahorsePgpUid*   seahorse_pgp_uid_new                  (SeahorsePgpKey *parent,
                                                         const char     *uid_string);

SeahorsePgpKey *  seahorse_pgp_uid_get_parent           (SeahorsePgpUid *self);

void              seahorse_pgp_uid_realize              (SeahorsePgpUid *self);

GListModel *      seahorse_pgp_uid_get_signatures       (SeahorsePgpUid *self);

void              seahorse_pgp_uid_add_signature        (SeahorsePgpUid       *self,
                                                         SeahorsePgpSignature *signature);

void              seahorse_pgp_uid_remove_signature     (SeahorsePgpUid       *self,
                                                         SeahorsePgpSignature *signature);

SeahorseValidity  seahorse_pgp_uid_get_validity         (SeahorsePgpUid *self);

void              seahorse_pgp_uid_set_validity         (SeahorsePgpUid *self,
                                                         SeahorseValidity validity);

const char *      seahorse_pgp_uid_get_name             (SeahorsePgpUid *self);

void              seahorse_pgp_uid_set_name             (SeahorsePgpUid *self,
                                                         const char     *name);

const char *      seahorse_pgp_uid_get_email            (SeahorsePgpUid *self);

void              seahorse_pgp_uid_set_email            (SeahorsePgpUid *self,
                                                         const char     *comment);

const char *      seahorse_pgp_uid_get_comment          (SeahorsePgpUid *self);

void              seahorse_pgp_uid_set_comment          (SeahorsePgpUid *self,
                                                         const char     *comment);

char *            seahorse_pgp_uid_calc_label           (const char *name,
                                                         const char *email,
                                                         const char *comment);

char *            seahorse_pgp_uid_calc_markup          (const char  *name,
                                                         const char  *email,
                                                         const char  *comment,
                                                         unsigned int flags);
