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

#include "config.h"

#include "seahorse-pgp-key-algorithm.h"

#include <glib/gi18n.h>

const char *
seahorse_pgp_key_algorithm_to_string (SeahorsePgpKeyAlgorithm type)
{
    switch (type) {
        case SEAHORSE_PGP_KEY_ALGORITHM_RSA:
            return _("RSA");
        case SEAHORSE_PGP_KEY_ALGORITHM_DSA:
            return _("DSA");
        case SEAHORSE_PGP_KEY_ALGORITHM_ELGAMAL:
            return _("ElGamal");
        case SEAHORSE_PGP_KEY_ALGORITHM_ECC:
            return _("ECC");
        case SEAHORSE_PGP_KEY_ALGORITHM_ECDSA:
            return _("ECDSA");
        case SEAHORSE_PGP_KEY_ALGORITHM_ECDH:
            return _("ECDH");
        case SEAHORSE_PGP_KEY_ALGORITHM_EDDSA:
            return _("EdDSA");
    }

    g_return_val_if_reached (NULL);
}

/**
 * seahorse_pgp_key_algorithm_to_gpgme_string:
 * @type: The algo type
 *
 * Returns: (transfer none): A string version of the algorithm, which can be
 * used for GPGME functions like gpgme_op_create(sub)key.
 */
const char *
seahorse_pgp_key_algorithm_to_gpgme_string (SeahorsePgpKeyAlgorithm algo)
{
    switch (algo) {
        case SEAHORSE_PGP_KEY_ALGORITHM_RSA:
            return "rsa";
        case SEAHORSE_PGP_KEY_ALGORITHM_DSA:
            return "dsa";
        case SEAHORSE_PGP_KEY_ALGORITHM_ELGAMAL:
            return "elg";
        default: /* For EC curves, we need the curve name */
            return NULL;
    }

    g_return_val_if_reached (NULL);
}

gboolean
seahorse_pgp_key_algorithm_get_length_values (SeahorsePgpKeyAlgorithm  algo,
                                              unsigned int            *default_val,
                                              unsigned int            *lower,
                                              unsigned int            *upper)
{
    switch (algo) {
        case SEAHORSE_PGP_KEY_ALGORITHM_RSA:
            if (default_val) *default_val = 4096;
            if (lower) *lower = 1024;
            if (upper) *upper = 4096;
            return TRUE;
        case SEAHORSE_PGP_KEY_ALGORITHM_DSA:
            if (default_val) *default_val = 2048;
            if (lower) *lower = 768;
            if (upper) *upper = 3072;
            return TRUE;
        case SEAHORSE_PGP_KEY_ALGORITHM_ELGAMAL:
            if (default_val) *default_val = 2048;
            if (lower) *lower = 768;
            if (upper) *upper = 4096;
            return TRUE;
        /* Length not configurable (or fixed length) */
        case SEAHORSE_PGP_KEY_ALGORITHM_ECC:
        case SEAHORSE_PGP_KEY_ALGORITHM_ECDSA:
        case SEAHORSE_PGP_KEY_ALGORITHM_ECDH:
        case SEAHORSE_PGP_KEY_ALGORITHM_EDDSA:
            return FALSE;
    }

    g_return_val_if_reached (FALSE);
}
