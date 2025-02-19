/*
 * Seahorse
 *
 * Copyright (C) 2025 Niels De Graef
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

#include <glib.h>

typedef enum {
    SEAHORSE_PGP_KEY_ALGORITHM_RSA,
    SEAHORSE_PGP_KEY_ALGORITHM_DSA,
    SEAHORSE_PGP_KEY_ALGORITHM_ELGAMAL,
    SEAHORSE_PGP_KEY_ALGORITHM_ECC,
    SEAHORSE_PGP_KEY_ALGORITHM_ECDSA,
    SEAHORSE_PGP_KEY_ALGORITHM_ECDH,
    SEAHORSE_PGP_KEY_ALGORITHM_EDDSA,
} SeahorsePgpKeyAlgorithm;

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

const char *    seahorse_pgp_key_algorithm_to_string          (SeahorsePgpKeyAlgorithm algo);

const char *    seahorse_pgp_key_algorithm_to_gpgme_string    (SeahorsePgpKeyAlgorithm algo);

gboolean        seahorse_pgp_key_algorithm_get_length_values  (SeahorsePgpKeyAlgorithm  algo,
                                                               unsigned int            *default_val,
                                                               unsigned int            *lower,
                                                               unsigned int            *upper);
