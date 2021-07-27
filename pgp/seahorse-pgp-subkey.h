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
#include "pgp/seahorse-pgp-key.h"

#define SEAHORSE_PGP_TYPE_SUBKEY            (seahorse_pgp_subkey_get_type ())
G_DECLARE_DERIVABLE_TYPE (SeahorsePgpSubkey, seahorse_pgp_subkey,
                          SEAHORSE_PGP, SUBKEY,
                          GObject)

struct _SeahorsePgpSubkeyClass {
    GObjectClass parent_class;
};

SeahorsePgpSubkey*  seahorse_pgp_subkey_new               (void);

SeahorsePgpKey *    seahorse_pgp_subkey_get_parent_key    (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_parent_key    (SeahorsePgpSubkey *self,
                                                           SeahorsePgpKey    *parent_key);

unsigned int        seahorse_pgp_subkey_get_index         (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_index         (SeahorsePgpSubkey *self,
                                                           unsigned int       index);

const char *        seahorse_pgp_subkey_get_keyid         (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_keyid         (SeahorsePgpSubkey *self,
                                                           const char        *keyid);

unsigned int        seahorse_pgp_subkey_get_flags         (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_flags         (SeahorsePgpSubkey *self,
                                                           unsigned int       flags);

const char *        seahorse_pgp_subkey_get_algorithm     (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_algorithm     (SeahorsePgpSubkey *self,
                                                           const char        *algorithm);

unsigned int        seahorse_pgp_subkey_get_length        (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_length        (SeahorsePgpSubkey *self,
                                                           unsigned int       index);

char *              seahorse_pgp_subkey_get_usage         (SeahorsePgpSubkey *self);

char **             seahorse_pgp_subkey_get_usages        (SeahorsePgpSubkey  *self,
                                                           char             ***descriptions);

GDateTime *         seahorse_pgp_subkey_get_created       (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_created       (SeahorsePgpSubkey *self,
                                                           GDateTime         *created);

GDateTime *         seahorse_pgp_subkey_get_expires       (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_expires       (SeahorsePgpSubkey *self,
                                                           GDateTime         *expires);

const char *        seahorse_pgp_subkey_get_description   (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_description   (SeahorsePgpSubkey *self,
                                                           const char        *description);

char *              seahorse_pgp_subkey_calc_description  (const char   *name,
                                                           unsigned int  index);

const char *        seahorse_pgp_subkey_get_fingerprint   (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_fingerprint   (SeahorsePgpSubkey *self,
                                                           const char        *description);

char *              seahorse_pgp_subkey_calc_fingerprint  (const char *raw_fingerprint);
