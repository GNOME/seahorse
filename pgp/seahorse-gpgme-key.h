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

#include <gpgme.h>

#include "seahorse-pgp-key.h"

#define SEAHORSE_GPGME_TYPE_KEY (seahorse_gpgme_key_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseGpgmeKey, seahorse_gpgme_key,
                      SEAHORSE_GPGME, KEY,
                      SeahorsePgpKey)

SeahorseGpgmeKey* seahorse_gpgme_key_new                 (SeahorsePlace *sksrc,
                                                          gpgme_key_t pubkey,
                                                          gpgme_key_t seckey);

void              seahorse_gpgme_key_refresh              (SeahorseGpgmeKey *self);

void              seahorse_gpgme_key_realize              (SeahorseGpgmeKey *self);

void              seahorse_gpgme_key_ensure_signatures    (SeahorseGpgmeKey *self);

gpgme_key_t       seahorse_gpgme_key_get_public           (SeahorseGpgmeKey *self);

void              seahorse_gpgme_key_set_public           (SeahorseGpgmeKey *self,
                                                           gpgme_key_t key);

gpgme_key_t       seahorse_gpgme_key_get_private          (SeahorseGpgmeKey *self);

void              seahorse_gpgme_key_set_private          (SeahorseGpgmeKey *self,
                                                           gpgme_key_t key);

void              seahorse_gpgme_key_refresh_matching     (gpgme_key_t key);

SeahorseValidity  seahorse_gpgme_key_get_validity         (SeahorseGpgmeKey *self);

SeahorseValidity  seahorse_gpgme_key_get_trust            (SeahorseGpgmeKey *self);
