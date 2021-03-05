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

#include "seahorse-hkp-source.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-uid.h"

#include <glib.h>

static void
test_hkp_lookup_response_simple_no_uid (void)
{
    g_autolist(SeahorsePgpKey) keys = NULL;
    SeahorsePgpKey *key;
    g_autoptr(GDateTime) created = NULL;
    GListModel *uids;

    keys = seahorse_hkp_parse_lookup_response (
        "info:1:1\n"
        "pub:0123456789ABCDEF0123456789ABCDEF01234567:1:4096:712627200::\n"
    );

    g_assert_nonnull (keys);
    g_assert_nonnull (keys->data);
    g_assert_cmpuint (g_list_length (keys), ==, 1);

    key = keys->data;

    g_assert_cmpstr (seahorse_pgp_key_get_fingerprint (key), ==,
                     "0123 4567 89AB CDEF 0123 4567 89AB CDEF 0123 4567");
    g_assert_cmpstr (seahorse_pgp_key_get_algo (key), ==, "RSA");
    g_assert_cmpuint (seahorse_pgp_key_get_length (key), ==, 4096);

    /* Happy birthday emails always welcome */
    created = g_date_time_new_utc (1992, 8, 1, 0, 0, 0);
    g_assert_true (g_date_time_equal (seahorse_pgp_key_get_created (key),
                                      created));

    g_assert_null (seahorse_pgp_key_get_expires (key));

    uids = seahorse_pgp_key_get_uids (key);
    g_assert_nonnull (uids);
    g_assert_cmpuint (g_list_model_get_n_items (uids), ==, 0);
}

static void
test_hkp_lookup_response_simple (void)
{
    g_autolist(SeahorsePgpKey) keys = NULL;
    SeahorsePgpKey *key;
    g_autoptr(GDateTime) created = NULL;
    GListModel *uids;
    g_autoptr(SeahorsePgpUid) uid = NULL;

    keys = seahorse_hkp_parse_lookup_response (
        "info:1:1\n"
        "pub:0123456789ABCDEF0123456789ABCDEF01234567:1:4096:712627200::\n"
        "uid:Niels De Graef <nielsdegraef@gmail.com>:::\n"
    );

    g_assert_nonnull (keys);
    g_assert_nonnull (keys->data);
    g_assert_cmpuint (g_list_length (keys), ==, 1);

    key = keys->data;

    g_assert_cmpstr (seahorse_pgp_key_get_fingerprint (key), ==,
                     "0123 4567 89AB CDEF 0123 4567 89AB CDEF 0123 4567");
    g_assert_cmpstr (seahorse_pgp_key_get_algo (key), ==, "RSA");
    g_assert_cmpuint (seahorse_pgp_key_get_length (key), ==, 4096);

    /* Happy birthday emails always welcome */
    created = g_date_time_new_utc (1992, 8, 1, 0, 0, 0);
    g_assert_true (g_date_time_equal (seahorse_pgp_key_get_created (key),
                                      created));

    g_assert_null (seahorse_pgp_key_get_expires (key));

    /* UID */
    uids = seahorse_pgp_key_get_uids (key);
    g_assert_nonnull (uids);
    g_assert_cmpuint (g_list_model_get_n_items (uids), ==, 1);

    uid = g_list_model_get_item (uids, 0);
    g_assert_nonnull (uid);
    g_assert_cmpstr (seahorse_pgp_uid_get_name (uid), ==, "Niels De Graef");
    g_assert_cmpstr (seahorse_pgp_uid_get_email (uid), ==, "nielsdegraef@gmail.com");
    g_assert_cmpstr (seahorse_pgp_uid_get_comment (uid), ==, "");
}

static void
test_hkp_lookup_response_empty (void)
{
    g_autolist(SeahorsePgpKey) keys = NULL;

    keys = seahorse_hkp_parse_lookup_response ("info:1:0\n");

    g_assert_null (keys);
    g_assert_cmpuint (g_list_length (keys), ==, 0);
}

static void
test_hkp_is_valid_uri (void)
{
    g_assert_true (seahorse_hkp_is_valid_uri ("hkp://keys.openpgp.org"));
    g_assert_true (seahorse_hkp_is_valid_uri ("hkps://keys.openpgp.org"));

    /* Invalid URL */
    g_assert_false (seahorse_hkp_is_valid_uri ("test"));

    /* Missing scheme */
    g_assert_false (seahorse_hkp_is_valid_uri ("keys.openpgp.org"));

    /* Wrong scheme */
    g_assert_false (seahorse_hkp_is_valid_uri ("ldap://keys.openpgp.org"));
}

int
main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/hkp/valid-uri", test_hkp_is_valid_uri);
    g_test_add_func ("/hkp/lookup-response-empty", test_hkp_lookup_response_empty);
    g_test_add_func ("/hkp/lookup-response-simple", test_hkp_lookup_response_simple);
    g_test_add_func ("/hkp/lookup-response-simple-no-uid", test_hkp_lookup_response_simple_no_uid);

    return g_test_run ();
}
