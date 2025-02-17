/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "seahorse-gpgme.h"
#include "seahorse-common.h"

typedef enum {
    SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_RSA,
    SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_SIGN,
    SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_ENCRYPT,
    SEAHORSE_GPGME_KEY_GEN_TYPE_DSA,
    SEAHORSE_GPGME_KEY_GEN_TYPE_DSA_ELGAMAL,
    SEAHORSE_GPGME_KEY_GEN_TYPE_ELGAMAL,
} SeahorseGpgmeKeyGenType;

/* Length ranges for key types */
typedef enum {
    /* Minimum length for #DSA. */
    DSA_MIN = 768,
    /* Maximum length for #DSA. */
    DSA_MAX = 3072,
    /* Minimum length for #ELGAMAL. Maximum length is #LENGTH_MAX. */
    ELGAMAL_MIN = 768,
    /* Minimum length of #RSA_SIGN and #RSA_ENCRYPT. Maximum length is
     * #LENGTH_MAX.
     */
    RSA_MIN = 1024,
    /* Maximum length for #ELGAMAL, #RSA_SIGN, and #RSA_ENCRYPT. */
    LENGTH_MAX = 4096,
    /* Default length for #ELGAMAL, #RSA_SIGN, #RSA_ENCRYPT, and #DSA. */
    LENGTH_DEFAULT = 2048,
} SeahorseKeyLength;

const char *    seahorse_gpgme_key_enc_type_to_string        (SeahorseGpgmeKeyGenType encoding);

const char *    seahorse_gpgme_key_enc_type_to_gpgme_string  (SeahorseGpgmeKeyGenType encoding);
