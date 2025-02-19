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

#include "seahorse-pgp-subkey-usage.h"

#include <glib/gi18n.h>

const char *
seahorse_pgp_subkey_usage_to_string (SeahorsePgpSubkeyUsage usage)
{
    switch (usage) {
        case SEAHORSE_PGP_SUBKEY_USAGE_DEFAULT:
            return _("All Usages");
        case SEAHORSE_PGP_SUBKEY_USAGE_SIGN_ONLY:
            return _("Sign Only");
        case SEAHORSE_PGP_SUBKEY_USAGE_ENCRYPT_ONLY:
            return _("Encrypt Only");
    }

    g_return_val_if_reached (NULL);
}
