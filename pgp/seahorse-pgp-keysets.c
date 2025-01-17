/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

#include "seahorse-pgp-keysets.h"

#include "seahorse-gpgme-key.h"
#include "seahorse-pgp-backend.h"
#include "seahorse-pgp-key.h"

#include "seahorse-common.h"

/* -----------------------------------------------------------------------------
 * COMMON KEYSETS
 */

static void
on_settings_default_key_changed (GSettings *settings, const gchar *key, gpointer user_data)
{
    GtkFilter *filter = GTK_FILTER (user_data);

    /* Default key changed, refresh */
    gtk_filter_changed (filter, GTK_FILTER_CHANGE_DIFFERENT);
}

static gboolean
pgp_signers_match (void *item, void *data)
{
    SeahorsePgpKey *key = SEAHORSE_PGP_KEY (item);
    SeahorsePgpKey *defkey;
    SeahorseUsage usage;
    SeahorseFlags flags;

    if (!SEAHORSE_GPGME_IS_KEY (key))
        return FALSE;

    usage = seahorse_item_get_usage (SEAHORSE_ITEM (item));
    if (usage != SEAHORSE_USAGE_PRIVATE_KEY)
        return FALSE;

    flags = seahorse_item_get_item_flags (SEAHORSE_ITEM (item));
    if (!(flags & SEAHORSE_FLAG_CAN_SIGN))
        return FALSE;
    if (flags & (SEAHORSE_FLAG_EXPIRED | SEAHORSE_FLAG_REVOKED | SEAHORSE_FLAG_DISABLED))
        return FALSE;

    defkey = seahorse_pgp_backend_get_default_key (NULL);

    /* Default key overrides all, and becomes the only signer available*/
    if (defkey != NULL &&
        g_strcmp0 (seahorse_pgp_key_get_keyid (defkey),
                   seahorse_pgp_key_get_keyid (key)) != 0)
        return FALSE;

    return TRUE;
}

GListModel *
seahorse_keyset_pgp_signers_new (void)
{
    SeahorseGpgmeKeyring *keyring;
    g_autoptr(GtkCustomFilter) filter = NULL;
    GtkFilterListModel *model;

    keyring = seahorse_pgp_backend_get_default_keyring (NULL);
    filter = gtk_custom_filter_new (pgp_signers_match, NULL, NULL);
    model = gtk_filter_list_model_new (G_LIST_MODEL (keyring),
                                       GTK_FILTER (g_steal_pointer (&filter)));

    g_signal_connect_object (seahorse_pgp_settings_instance (), "changed::default-key",
                             G_CALLBACK (on_settings_default_key_changed), filter, 0);

    return G_LIST_MODEL (model);
}
