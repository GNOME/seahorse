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

#include "seahorse-pgp-backend.h"

#include <glib.h>
#include <glib/gstdio.h>

typedef struct _PgpTestFixture {
    char *tmpdir;
} PgpTestFixture;

/* Basic sanity test: check if we don't crash on an empty keyring */
static void
test_pgp_check_empty_keyring (PgpTestFixture *fixture,
                              const void     *user_data)
{
    SeahorsePgpBackend *backend;
    SeahorseGpgmeKeyring *keyring;
    unsigned int n_keys;

    backend = seahorse_pgp_backend_get ();

    keyring = seahorse_pgp_backend_get_default_keyring (backend);
    g_assert_nonnull (keyring);
    g_assert_true (SEAHORSE_IS_GPGME_KEYRING (keyring));

    n_keys = g_list_model_get_n_items (G_LIST_MODEL (keyring));
    g_assert_cmpint (n_keys, ==, 0);
    g_assert_null (g_list_model_get_item (G_LIST_MODEL (keyring), 0));
}

static void
pgp_test_fixture_setup (PgpTestFixture *fixture,
                        const void     *user_data)
{
    g_autoptr(GError) error = NULL;

    fixture->tmpdir = g_dir_make_tmp ("seahorse-gpgme-test-XXXXXX.d", &error);
    g_assert_no_error (error);

    seahorse_pgp_backend_initialize (fixture->tmpdir);
    g_assert_nonnull (seahorse_pgp_backend_get ());
}

static void
pgp_test_fixture_teardown (PgpTestFixture *fixture,
                           const void     *user_data)
{
    g_rmdir (fixture->tmpdir);
    g_clear_pointer (&fixture->tmpdir, g_free);
}

int
main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add ("/pgp/list-empty-keyring", PgpTestFixture, NULL,
                pgp_test_fixture_setup,
                test_pgp_check_empty_keyring,
                pgp_test_fixture_teardown);

    return g_test_run ();
}
