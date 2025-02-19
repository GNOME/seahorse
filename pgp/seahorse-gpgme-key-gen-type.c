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

#include "config.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "operation"

#include "seahorse-gpgme-key-gen-type.h"

#include <glib/gi18n.h>

const char *
seahorse_gpgme_key_gen_type_to_string (SeahorseGpgmeKeyGenType type)
{
    switch (type) {
        case SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_RSA:
            return _("RSA");
        case SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_SIGN:
            return _("RSA (sign only)");
        case SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_ENCRYPT:
            return _("RSA (encrypt only)");
        case SEAHORSE_GPGME_KEY_GEN_TYPE_DSA:
            return _("DSA (sign only)");
        case SEAHORSE_GPGME_KEY_GEN_TYPE_DSA_ELGAMAL:
            return _("DSA ElGamal");
        case SEAHORSE_GPGME_KEY_GEN_TYPE_ELGAMAL:
            return _("ElGamal (encrypt only)");
    }

    g_return_val_if_reached (NULL);
}

SeahorsePgpKeyAlgorithm
seahorse_gpgme_key_gen_type_get_key_algo (SeahorseGpgmeKeyGenType type)
{
    switch (type) {
        case SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_RSA:
        case SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_SIGN:
        case SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_ENCRYPT:
            return SEAHORSE_PGP_KEY_ALGORITHM_RSA;
        case SEAHORSE_GPGME_KEY_GEN_TYPE_DSA:
        case SEAHORSE_GPGME_KEY_GEN_TYPE_DSA_ELGAMAL:
            return SEAHORSE_PGP_KEY_ALGORITHM_DSA;
        case SEAHORSE_GPGME_KEY_GEN_TYPE_ELGAMAL:
            return SEAHORSE_PGP_KEY_ALGORITHM_ELGAMAL;
    }

    g_return_val_if_reached (0);
}

gboolean
seahorse_gpgme_key_gen_type_get_subkey_algo (SeahorseGpgmeKeyGenType  type,
                                             SeahorsePgpKeyAlgorithm *subkey_algo)
{
    switch (type) {
        case SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_SIGN:
        case SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_ENCRYPT:
        case SEAHORSE_GPGME_KEY_GEN_TYPE_DSA:
        case SEAHORSE_GPGME_KEY_GEN_TYPE_ELGAMAL:
            return FALSE;
        case SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_RSA:
            *subkey_algo = SEAHORSE_PGP_KEY_ALGORITHM_RSA;
            return TRUE;
        case SEAHORSE_GPGME_KEY_GEN_TYPE_DSA_ELGAMAL:
            *subkey_algo = SEAHORSE_GPGME_KEY_GEN_TYPE_ELGAMAL;
            return TRUE;
    }

    g_return_val_if_reached (FALSE);
}
