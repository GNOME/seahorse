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

typedef enum {
    SEAHORSE_PGP_SUBKEY_USAGE_DEFAULT,
    SEAHORSE_PGP_SUBKEY_USAGE_SIGN_ONLY,
    SEAHORSE_PGP_SUBKEY_USAGE_ENCRYPT_ONLY,
} SeahorsePgpSubkeyUsage;

const char * seahorse_pgp_subkey_usage_to_string (SeahorsePgpSubkeyUsage usage);
