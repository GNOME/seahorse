/*
 * Seahorse
 *
 * Copyright (C) 2004 Stefan Walter
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

/**
 * SeahorseGpgmeKeyring: A key keyring for PGP keys retrieved from GPGME.
 *
 * - Derived from SeahorseKeyring
 * - Since GPGME represents secret keys as seperate from public keys, this
 *   class takes care to combine them into one logical SeahorsePGPKey object.
 * - Adds the keys it loads to the SeahorseContext.
 * - Eventually a lot of stuff from seahorse-op.* should probably be merged
 *   into this class.
 * - Monitors ~/.gnupg for changes and reloads the key ring as necessary.
 *
 * Properties:
 *  location: (SeahorseLocation) The location of keys that come from this
 *         keyring. (ie: SEAHORSE_LOCATION_LOCAL, SEAHORSE_LOCATION_REMOTE)
 */

#pragma once

#include <gpgme.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "seahorse-gpgme-key.h"

#define SEAHORSE_TYPE_GPGME_KEYRING (seahorse_gpgme_keyring_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseGpgmeKeyring, seahorse_gpgme_keyring,
                      SEAHORSE, GPGME_KEYRING,
                      GObject);

SeahorseGpgmeKeyring * seahorse_gpgme_keyring_new            (void);

gpgme_ctx_t            seahorse_gpgme_keyring_new_context    (gpgme_error_t *gerr);

SeahorseGpgmeKey *     seahorse_gpgme_keyring_lookup         (SeahorseGpgmeKeyring *self,
                                                              const char           *keyid);

void                   seahorse_gpgme_keyring_remove_key     (SeahorseGpgmeKeyring *self,
                                                              SeahorseGpgmeKey *key);

void                   seahorse_gpgme_keyring_import_async   (SeahorseGpgmeKeyring *self,
                                                              GInputStream *input,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

GList *                seahorse_gpgme_keyring_import_finish  (SeahorseGpgmeKeyring *self,
                                                              GAsyncResult *result,
                                                              GError **error);
