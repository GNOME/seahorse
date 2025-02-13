/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Stefan Walter
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

/*
 * Various UI elements and dialogs used in pgp component.
 */

#pragma once

#include <gtk/gtk.h>

#include "pgp/seahorse-gpgme-key.h"
#include "pgp/seahorse-gpgme-photo.h"
#include "pgp/seahorse-gpgme-subkey.h"
#include "pgp/seahorse-gpgme-keyring.h"
#include "pgp/seahorse-gpgme-uid.h"

void            seahorse_gpgme_generate_register    (void);

void            seahorse_gpgme_generate_show        (SeahorseGpgmeKeyring *keyring,
                                                     GtkWindow            *parent,
                                                     const char           *name,
                                                     const char           *email,
                                                     const char           *comment);

void            seahorse_gpgme_generate_key         (SeahorseGpgmeKeyring *keyring,
                                                     const char           *name,
                                                     const char           *email,
                                                     const char           *comment,
                                                     unsigned int          type,
                                                     unsigned int          bits,
                                                     time_t                expires,
                                                     GtkWindow            *parent);

void            seahorse_gpgme_add_revoker_new      (SeahorseGpgmeKey *pkey,
                                                     GtkWindow *parent);

void            seahorse_gpgme_expires_new          (SeahorseGpgmeSubkey *subkey,
                                                     GtkWindow *parent);

void            seahorse_gpgme_photo_add            (SeahorseGpgmeKey    *key,
                                                     GtkWindow           *parent,
                                                     GCancellable        *cancellable,
                                                     GAsyncReadyCallback  callback,
                                                     void                *user_data);

gboolean        seahorse_gpgme_photo_add_finish     (SeahorseGpgmeKey *pkey,
                                                     GAsyncResult     *result,
                                                     GError          **error);
