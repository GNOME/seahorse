/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2021 Niels De Graef
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

#include "pgp/seahorse-pgp-subkey.h"
#include "pgp/seahorse-gpgme-key.h"

#define SEAHORSE_GPGME_TYPE_SUBKEY (seahorse_gpgme_subkey_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseGpgmeSubkey, seahorse_gpgme_subkey,
                      SEAHORSE_GPGME, SUBKEY,
                      SeahorsePgpSubkey)

SeahorseGpgmeSubkey*  seahorse_gpgme_subkey_new               (SeahorseGpgmeKey *parent_key,
                                                               gpgme_subkey_t    subkey);

gpgme_subkey_t        seahorse_gpgme_subkey_get_subkey        (SeahorseGpgmeSubkey *self);

void                  seahorse_gpgme_subkey_set_subkey        (SeahorseGpgmeSubkey *self,
                                                               gpgme_subkey_t subkey);
