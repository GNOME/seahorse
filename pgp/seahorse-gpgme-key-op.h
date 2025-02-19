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

#pragma once

#include <glib.h>
#include <gpgme.h>
#include <time.h>

#include "pgp/seahorse-gpgme-key.h"
#include "pgp/seahorse-gpgme-key-gen-type.h"
#include "pgp/seahorse-gpgme-key-parms.h"
#include "pgp/seahorse-gpgme-keyring.h"
#include "pgp/seahorse-gpgme-subkey.h"
#include "pgp/seahorse-gpgme-uid.h"
#include "pgp/seahorse-gpgme-photo.h"
#include "pgp/seahorse-pgp-key-algorithm.h"
#include "pgp/seahorse-pgp-subkey-usage.h"

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
    SEAHORSE_PGP_REVOKE_REASON_NONE = 0,
    /* Key compromised */
    SEAHORSE_PGP_REVOKE_REASON_COMPROMISED = 1,
    /* Key replaced */
    SEAHORSE_PGP_REVOKE_REASON_SUPERSEDED = 2,
    /* Key no longer used */
    SEAHORSE_PGP_REVOKE_REASON_NOT_USED = 3,
} SeahorsePgpRevokeReason;

void                  seahorse_gpgme_key_op_generate_async   (SeahorseGpgmeKeyring  *keyring,
                                                              SeahorseGpgmeKeyParms *parms,
                                                              GCancellable          *cancellable,
                                                              GAsyncReadyCallback    callback,
                                                              void                  *user_data);

gboolean              seahorse_gpgme_key_op_generate_finish  (SeahorseGpgmeKeyring *keyring,
                                                              GAsyncResult *Result,
                                                              GError **error);

gpgme_error_t         seahorse_gpgme_key_op_delete           (SeahorseGpgmeKey *pkey);

gpgme_error_t         seahorse_gpgme_key_op_delete_pair      (SeahorseGpgmeKey *pkey);

gpgme_error_t         seahorse_gpgme_key_op_sign             (SeahorseGpgmeKey *key,
                                                              SeahorseGpgmeKey *signer,
                                                              SeahorseSignCheck check,
                                                              SeahorseSignOptions options);

gpgme_error_t         seahorse_gpgme_key_op_sign_uid         (SeahorseGpgmeUid    *uid,
                                                              SeahorseGpgmeKey    *signer,
                                                              SeahorseSignCheck    check,
                                                              SeahorseSignOptions  options);

void                 seahorse_gpgme_key_op_change_pass_async (SeahorseGpgmeKey *pkey,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

gboolean            seahorse_gpgme_key_op_change_pass_finish (SeahorseGpgmeKey *pkey,
                                                              GAsyncResult *Result,
                                                              GError **error);

gpgme_error_t         seahorse_gpgme_key_op_set_trust        (SeahorseGpgmeKey *pkey,
                                                              SeahorseValidity validity);

gpgme_error_t         seahorse_gpgme_key_op_set_disabled     (SeahorseGpgmeKey *pkey,
                                                              gboolean disabled);

gpgme_error_t         seahorse_gpgme_key_op_set_expires      (SeahorseGpgmeSubkey *subkey,
                                                              GDateTime           *expires);

gpgme_error_t         seahorse_gpgme_key_op_add_revoker      (SeahorseGpgmeKey *pkey,
                                                              SeahorseGpgmeKey *revoker);

void                  seahorse_gpgme_key_op_add_uid_async    (SeahorseGpgmeKey    *pkey,
                                                              const char          *name,
                                                              const char          *email,
                                                              const char          *comment,
                                                              GCancellable        *cancellable,
                                                              GAsyncReadyCallback  callback,
                                                              void                *user_data);

gboolean              seahorse_gpgme_key_op_add_uid_finish  (SeahorseGpgmeKey *pkey,
                                                             GAsyncResult *result,
                                                             GError **error);

void              seahorse_gpgme_key_op_make_primary_async  (SeahorseGpgmeUid   *uid,
                                                             GCancellable       *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             void                *user_data);

gboolean          seahorse_gpgme_key_op_make_primary_finish (SeahorseGpgmeUid *uid,
                                                             GAsyncResult *result,
                                                             GError **error);

gpgme_error_t         seahorse_gpgme_key_op_del_uid          (SeahorseGpgmeUid *uid);

void              seahorse_gpgme_key_op_add_subkey_async    (SeahorseGpgmeKey        *pkey,
                                                             SeahorsePgpKeyAlgorithm  algo,
                                                             SeahorsePgpSubkeyUsage   usage,
                                                             unsigned int             length,
                                                             GDateTime               *expires,
                                                             GCancellable            *cancellable,
                                                             GAsyncReadyCallback      callback,
                                                             void                    *user_data);

gboolean          seahorse_gpgme_key_op_add_subkey_finish   (SeahorseGpgmeKey *pkey,
                                                             GAsyncResult *result,
                                                             GError **error);

gpgme_error_t         seahorse_gpgme_key_op_del_subkey       (SeahorseGpgmeSubkey *subkey);

gpgme_error_t         seahorse_gpgme_key_op_revoke_subkey    (SeahorseGpgmeSubkey    *subkey,
                                                              SeahorsePgpRevokeReason reason,
                                                              const char             *description);

gpgme_error_t         seahorse_gpgme_key_op_photo_add        (SeahorseGpgmeKey *pkey,
                                                              const char       *filename);

gpgme_error_t         seahorse_gpgme_key_op_photo_delete     (SeahorseGpgmePhoto *photo);

gpgme_error_t         seahorse_gpgme_key_op_photos_load      (SeahorseGpgmeKey *key);

gpgme_error_t         seahorse_gpgme_key_op_photo_primary    (SeahorseGpgmePhoto *photo);
