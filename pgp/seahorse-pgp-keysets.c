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

#include "seahorse-application.h"
#include "seahorse-collection.h"
#include "seahorse-object.h"
#include "seahorse-predicate.h"

#include "seahorse-gpgme-key.h"
#include "seahorse-pgp-backend.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-keysets.h"

/* -----------------------------------------------------------------------------
 * COMMON KEYSETS 
 */

static void
on_settings_default_key_changed (GSettings *settings, const gchar *key, gpointer user_data)
{
	/* Default key changed, refresh */
	seahorse_collection_refresh (SEAHORSE_COLLECTION (user_data));
}

static gboolean 
pgp_signers_match (GObject *obj,
                   gpointer data)
{
	SeahorsePgpKey *defkey;

	if (!SEAHORSE_IS_PGP_KEY (obj))
		return FALSE;

	defkey = seahorse_pgp_backend_get_default_key (NULL);

	/* Default key overrides all, and becomes the only signer available*/
	if (defkey != NULL &&
	    g_strcmp0 (seahorse_pgp_key_get_keyid (defkey),
	               seahorse_pgp_key_get_keyid (SEAHORSE_PGP_KEY (obj))) != 0)
		return FALSE;

	return TRUE;
}

GcrCollection *
seahorse_keyset_pgp_signers_new (void)
{
	SeahorsePredicate *predicate = g_new0 (SeahorsePredicate, 1);
	SeahorseCollection *collection;
	SeahorseGpgmeKeyring *keyring;

	predicate->type = SEAHORSE_TYPE_GPGME_KEY;
	predicate->usage = SEAHORSE_USAGE_PRIVATE_KEY;
	predicate->flags = SEAHORSE_FLAG_CAN_SIGN;
	predicate->nflags = SEAHORSE_FLAG_EXPIRED | SEAHORSE_FLAG_REVOKED | SEAHORSE_FLAG_DISABLED;
	predicate->custom = pgp_signers_match;

	keyring = seahorse_pgp_backend_get_default_keyring (NULL);
	collection = seahorse_collection_new_for_predicate (GCR_COLLECTION (keyring),
	                                                    predicate, g_free);

	g_signal_connect_object (seahorse_application_pgp_settings (NULL), "changed::default-key",
	                         G_CALLBACK (on_settings_default_key_changed), collection, 0);

	return GCR_COLLECTION (collection);
}
