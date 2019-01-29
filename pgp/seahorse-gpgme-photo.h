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

#include "pgp/seahorse-gpgme.h"
#include "pgp/seahorse-pgp-photo.h"

#define SEAHORSE_TYPE_GPGME_PHOTO (seahorse_gpgme_photo_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseGpgmePhoto, seahorse_gpgme_photo,
                      SEAHORSE, GPGME_PHOTO,
                      SeahorsePgpPhoto);

SeahorseGpgmePhoto* seahorse_gpgme_photo_new             (gpgme_key_t key,
                                                          GdkPixbuf *pixbuf,
                                                          guint index);

gpgme_key_t         seahorse_gpgme_photo_get_pubkey      (SeahorseGpgmePhoto *self);

guint               seahorse_gpgme_photo_get_index       (SeahorseGpgmePhoto *self);

void                seahorse_gpgme_photo_set_index       (SeahorseGpgmePhoto *self,
                                                          guint index);
