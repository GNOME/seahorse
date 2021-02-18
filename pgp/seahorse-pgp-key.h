/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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

enum {
    SKEY_PGPSIG_TRUSTED = 0x0001,
    SKEY_PGPSIG_PERSONAL = 0x0002
};

#define SEAHORSE_PGP_TYPE_KEY (seahorse_pgp_key_get_type ())
G_DECLARE_DERIVABLE_TYPE (SeahorsePgpKey, seahorse_pgp_key, SEAHORSE_PGP, KEY, SeahorseObject)

struct _SeahorsePgpKeyClass {
    SeahorseObjectClass parent_class;

    /* virtual methods */
    GList* (*get_uids) (SeahorsePgpKey *self);
    void   (*set_uids) (SeahorsePgpKey *self, GList *uids);

    GList* (*get_subkeys) (SeahorsePgpKey *self);
    void   (*set_subkeys) (SeahorsePgpKey *self, GList *uids);

    GList* (*get_photos) (SeahorsePgpKey *self);
    void   (*set_photos) (SeahorsePgpKey *self, GList *uids);
};

SeahorsePgpKey *  seahorse_pgp_key_new                  (void);

void              seahorse_pgp_key_realize              (SeahorsePgpKey *self);

GList*            seahorse_pgp_key_get_subkeys          (SeahorsePgpKey *self);

void              seahorse_pgp_key_set_subkeys          (SeahorsePgpKey *self,
                                                         GList *subkeys);

GList*            seahorse_pgp_key_get_uids             (SeahorsePgpKey *self);

void              seahorse_pgp_key_set_uids             (SeahorsePgpKey *self,
                                                         GList *subkeys);

GList*            seahorse_pgp_key_get_photos           (SeahorsePgpKey *self);

void              seahorse_pgp_key_set_photos           (SeahorsePgpKey *self,
                                                         GList *subkeys);

const char*       seahorse_pgp_key_get_fingerprint      (SeahorsePgpKey *self);

SeahorseValidity  seahorse_pgp_key_get_validity         (SeahorsePgpKey *self);

gulong            seahorse_pgp_key_get_expires          (SeahorsePgpKey *self);

SeahorseValidity  seahorse_pgp_key_get_trust            (SeahorsePgpKey *self);

guint             seahorse_pgp_key_get_length           (SeahorsePgpKey *self);

const char*       seahorse_pgp_key_get_algo             (SeahorsePgpKey *self);

const char*       seahorse_pgp_key_get_keyid            (SeahorsePgpKey *self);

gboolean          seahorse_pgp_key_has_keyid            (SeahorsePgpKey *self,
                                                         const char     *keyid);

const char*       seahorse_pgp_key_calc_identifier      (const char *keyid);

guint             seahorse_pgp_keyid_hash               (gconstpointer v);

gboolean          seahorse_pgp_keyid_equal              (gconstpointer v1,
                                                         gconstpointer v2);
