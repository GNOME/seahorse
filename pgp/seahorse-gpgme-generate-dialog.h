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

#include "pgp/seahorse-gpgme-keyring.h"

#define SEAHORSE_GPGME_TYPE_GENERATE_DIALOG (seahorse_gpgme_generate_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseGpgmeGenerateDialog, seahorse_gpgme_generate_dialog,
                      SEAHORSE_GPGME, GENERATE_DIALOG,
                      AdwDialog)

void            seahorse_gpgme_generate_register    (void);

SeahorseGpgmeGenerateDialog *      seahorse_gpgme_generate_dialog_new  (SeahorseGpgmeKeyring *keyring);
