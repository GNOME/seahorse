/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#ifndef __SEAHORSE_GPGME_KEY_OP_H__
#define __SEAHORSE_GPGME_KEY_OP_H__

#include <glib.h>
#include <gpgme.h>
#include <time.h>

#include "pgp/seahorse-gpgme-key.h"
#include "pgp/seahorse-gpgme-keyring.h"
#include "pgp/seahorse-gpgme-subkey.h"
#include "pgp/seahorse-gpgme-uid.h"
#include "pgp/seahorse-gpgme-photo.h"

/*
 * Key type options.
 * We only support GPG version >=2.0.12 or >= 2.1.4
 */

typedef enum {
	RSA_RSA = 1,
	DSA_ELGAMAL = 2,
	DSA = 3,
	RSA_SIGN = 4,
	ELGAMAL = 5,
	RSA_ENCRYPT = 6
} SeahorseKeyEncType;

/* Length ranges for key types */
typedef enum {
	/* Minimum length for #DSA. */
	DSA_MIN = 768,
	/* Maximum length for #DSA. */
#if ( GPG_MAJOR == 2 &&   GPG_MINOR == 0 && GPG_MICRO < 12 ) || \
    ( GPG_MAJOR == 1 && ( GPG_MINOR <  4 || GPG_MICRO < 10 ) )
	DSA_MAX = 1024,
#else
	DSA_MAX = 3072,
#endif
	/* Minimum length for #ELGAMAL. Maximum length is #LENGTH_MAX. */
	ELGAMAL_MIN = 768,
	/* Minimum length of #RSA_SIGN and #RSA_ENCRYPT. Maximum length is
	 * #LENGTH_MAX.
	 */
	RSA_MIN = 1024,
	/* Maximum length for #ELGAMAL, #RSA_SIGN, and #RSA_ENCRYPT. */
	LENGTH_MAX = 4096,
	/* Default length for #ELGAMAL, #RSA_SIGN, #RSA_ENCRYPT, and #DSA. */
	LENGTH_DEFAULT = 2048
} SeahorseKeyLength;

typedef enum {
	/* Unknown key check */
	SIGN_CHECK_NO_ANSWER,
	/* Key not checked */
	SIGN_CHECK_NONE,
	/* Key casually checked */
	SIGN_CHECK_CASUAL,
	/* Key carefully checked */
	SIGN_CHECK_CAREFUL
} SeahorseSignCheck;

typedef enum {
	/* If signature is local */
	SIGN_LOCAL = 1 << 0,
	/* If signature is non-revocable */
	SIGN_NO_REVOKE = 1 << 1,
	/* If signature expires with key */
	SIGN_EXPIRES = 1 << 2
} SeahorseSignOptions;

typedef enum {
	/* No revocation reason */
	REVOKE_NO_REASON = 0,
	/* Key compromised */
	REVOKE_COMPROMISED = 1,
	/* Key replaced */
	REVOKE_SUPERSEDED = 2,
	/* Key no longer used */
	REVOKE_NOT_USED = 3
} SeahorseRevokeReason;

void                  seahorse_gpgme_key_op_generate_async   (SeahorseGpgmeKeyring *keyring,
                                                              const gchar *name,
                                                              const gchar *email,
                                                              const gchar *comment,
                                                              const gchar *passphrase,
                                                              SeahorseKeyEncType type,
                                                              guint length,
                                                              time_t expires,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

gboolean              seahorse_gpgme_key_op_generate_finish  (SeahorseGpgmeKeyring *keyring,
                                                              GAsyncResult *Result,
                                                              GError **error);

gpgme_error_t         seahorse_gpgme_key_op_delete           (SeahorseGpgmeKey *pkey);

gpgme_error_t         seahorse_gpgme_key_op_delete_pair      (SeahorseGpgmeKey *pkey);

gpgme_error_t         seahorse_gpgme_key_op_sign             (SeahorseGpgmeKey *key,
                                                              SeahorseGpgmeKey *signer,
                                                              SeahorseSignCheck check,
                                                              SeahorseSignOptions options);

gpgme_error_t         seahorse_gpgme_key_op_sign_uid         (SeahorseGpgmeUid *uid, 
                                                              SeahorseGpgmeKey *signer, 
                                                              SeahorseSignCheck check, 
                                                              SeahorseSignOptions options);

gpgme_error_t         seahorse_gpgme_key_op_change_pass      (SeahorseGpgmeKey *pkey);

gpgme_error_t         seahorse_gpgme_key_op_set_trust        (SeahorseGpgmeKey *pkey,
                                                              SeahorseValidity validity);

gpgme_error_t         seahorse_gpgme_key_op_set_disabled     (SeahorseGpgmeKey *pkey,
                                                              gboolean disabled);

gpgme_error_t         seahorse_gpgme_key_op_set_expires      (SeahorseGpgmeSubkey *subkey,
                                                              time_t expires);

gpgme_error_t         seahorse_gpgme_key_op_add_revoker      (SeahorseGpgmeKey *pkey, 
                                                              SeahorseGpgmeKey *revoker);

gpgme_error_t         seahorse_gpgme_key_op_add_uid          (SeahorseGpgmeKey *pkey,
                                                              const gchar *name,
                                                              const gchar *email,
                                                              const gchar *comment);

gpgme_error_t         seahorse_gpgme_key_op_primary_uid      (SeahorseGpgmeUid *uid);

gpgme_error_t         seahorse_gpgme_key_op_del_uid          (SeahorseGpgmeUid *uid);
                             
gpgme_error_t         seahorse_gpgme_key_op_add_subkey       (SeahorseGpgmeKey *pkey,
                                                              SeahorseKeyEncType type,
                                                              guint length,
                                                              time_t expires);

gpgme_error_t         seahorse_gpgme_key_op_del_subkey       (SeahorseGpgmeSubkey *subkey);

gpgme_error_t         seahorse_gpgme_key_op_revoke_subkey    (SeahorseGpgmeSubkey *subkey,
                                                              SeahorseRevokeReason reason,
                                                              const gchar *description);

gpgme_error_t         seahorse_gpgme_key_op_photo_add        (SeahorseGpgmeKey *pkey,
                                                              const gchar *filename);
 
gpgme_error_t         seahorse_gpgme_key_op_photo_delete     (SeahorseGpgmePhoto *photo);
                                                     
gpgme_error_t         seahorse_gpgme_key_op_photos_load      (SeahorseGpgmeKey *key);

gpgme_error_t         seahorse_gpgme_key_op_photo_primary    (SeahorseGpgmePhoto *photo);

#endif /* __SEAHORSE_GPGME_KEY_OP_H__ */
