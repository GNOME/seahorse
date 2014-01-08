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

#ifndef __SEAHORSE_GPGME_KEYRING_H__
#define __SEAHORSE_GPGME_KEYRING_H__

#include <gpgme.h>

#include <glib-object.h>

#include <gio/gio.h>

#include "seahorse-gpgme-key.h"

#define SEAHORSE_TYPE_GPGME_KEYRING            (seahorse_gpgme_keyring_get_type ())
#define SEAHORSE_GPGME_KEYRING(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GPGME_KEYRING, SeahorseGpgmeKeyring))
#define SEAHORSE_GPGME_KEYRING_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GPGME_KEYRING, SeahorseGpgmeKeyringClass))
#define SEAHORSE_IS_GPGME_KEYRING(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GPGME_KEYRING))
#define SEAHORSE_IS_GPGME_KEYRING_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GPGME_KEYRING))
#define SEAHORSE_GPGME_KEYRING_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GPGME_KEYRING, SeahorseGpgmeKeyringClass))

typedef struct _SeahorseGpgmeKeyring SeahorseGpgmeKeyring;
typedef struct _SeahorseGpgmeKeyringClass SeahorseGpgmeKeyringClass;
typedef struct _SeahorseGpgmeKeyringPrivate SeahorseGpgmeKeyringPrivate;

struct _SeahorseGpgmeKeyring {
	GObject parent;
	SeahorseGpgmeKeyringPrivate *pv;
};

struct _SeahorseGpgmeKeyringClass {
	GObjectClass parent_class;
};

GType                  seahorse_gpgme_keyring_get_type       (void);

SeahorseGpgmeKeyring * seahorse_gpgme_keyring_new            (void);

gpgme_ctx_t            seahorse_gpgme_keyring_new_context    (gpgme_error_t *gerr);

SeahorseGpgmeKey *     seahorse_gpgme_keyring_lookup         (SeahorseGpgmeKeyring *self,
                                                              const gchar *keyid);

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

#endif /* __SEAHORSE_GPGME_KEYRING_H__ */
