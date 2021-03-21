/*
 * Seahorse
 *
 * Copyright (C) 2021 Niels De Graef
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

#include "seahorse-ldap-source.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-uid.h"

#include <glib.h>

static void
test_ldap_is_valid_uri (void)
{
    g_assert_true (seahorse_ldap_is_valid_uri ("ldap://keyserver.pgp.com"));

    /* Invalid URL */
    g_assert_false (seahorse_ldap_is_valid_uri ("test"));

    /* Missing scheme */
    g_assert_false (seahorse_ldap_is_valid_uri ("keyserver.pgp.com"));

    /* Wrong scheme */
    g_assert_false (seahorse_ldap_is_valid_uri ("hkp://keys.openpgp.org"));
}

int
main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/ldap/valid-uri", test_ldap_is_valid_uri);

    return g_test_run ();
}
