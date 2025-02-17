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
seahorse_gpgme_key_enc_type_to_string (SeahorseGpgmeKeyGenType type)
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

/**
 * seahorse_gpgme_key_enc_type_to_gpgme_string:
 * @type: The algo type
 *
 * Returns: (transfer none): A string version of the algorithm, which can be
 * used for GPGME functions like gpgme_op_create(sub)key.
 */
const char *
seahorse_gpgme_key_enc_type_to_gpgme_string (SeahorseGpgmeKeyGenType algo)
{
    switch (algo) {
        case SEAHORSE_GPGME_KEY_GEN_TYPE_DSA:
            return "dsa";
        case SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_RSA:
        case SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_SIGN:
        case SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_ENCRYPT:
            return "rsa";
        case SEAHORSE_GPGME_KEY_GEN_TYPE_ELGAMAL:
            return "elg";
        default:
            return NULL;
    }

    g_return_val_if_reached (NULL);
}
