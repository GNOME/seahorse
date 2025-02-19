/*
 * Seahorse
 *
 * Copyright (C) 2020 Niels De Graef
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

#include <gtk/gtk.h>

#include "pgp/seahorse-gpgme.h"
#include "pgp/seahorse-gpgme-key.h"
#include "pgp/seahorse-gpgme-key-gen-type.h"
#include "pgp/seahorse-gpgme-subkey.h"
#include "pgp/seahorse-pgp-subkey-usage.h"

#define SEAHORSE_GPGME_TYPE_ADD_SUBKEY (seahorse_gpgme_add_subkey_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseGpgmeAddSubkey, seahorse_gpgme_add_subkey,
                      SEAHORSE_GPGME, ADD_SUBKEY,
                      GtkDialog)

SeahorseGpgmeAddSubkey*  seahorse_gpgme_add_subkey_new               (SeahorseGpgmeKey *pkey,
                                                                      GtkWindow *parent);

SeahorsePgpKeyAlgorithm  seahorse_gpgme_add_subkey_get_selected_algo (SeahorseGpgmeAddSubkey *self);

SeahorsePgpSubkeyUsage   seahorse_gpgme_add_subkey_get_selected_usage (SeahorseGpgmeAddSubkey *self);

guint                    seahorse_gpgme_add_subkey_get_keysize       (SeahorseGpgmeAddSubkey *self);

GDateTime *              seahorse_gpgme_add_subkey_get_expires       (SeahorseGpgmeAddSubkey *self);
