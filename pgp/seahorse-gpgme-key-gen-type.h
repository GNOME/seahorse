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
#include "seahorse-pgp-key-algorithm.h"

typedef enum {
    SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_RSA,
    SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_SIGN,
    SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_ENCRYPT,
    SEAHORSE_GPGME_KEY_GEN_TYPE_DSA,
    SEAHORSE_GPGME_KEY_GEN_TYPE_DSA_ELGAMAL,
    SEAHORSE_GPGME_KEY_GEN_TYPE_ELGAMAL,
} SeahorseGpgmeKeyGenType;

const char *             seahorse_gpgme_key_gen_type_to_string        (SeahorseGpgmeKeyGenType type);

SeahorsePgpKeyAlgorithm  seahorse_gpgme_key_gen_type_get_key_algo     (SeahorseGpgmeKeyGenType type);

gboolean                 seahorse_gpgme_key_gen_type_get_subkey_algo  (SeahorseGpgmeKeyGenType  type,
                                                                       SeahorsePgpKeyAlgorithm *subkey_algo);
