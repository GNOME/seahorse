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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include "config.h"

#include "seahorse-collection.h"
#include "seahorse-object.h"
#include "seahorse-predicate.h"

#include "seahorse-pgp-module.h"
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
pgp_signers_match (SeahorseObject *obj,
                   gpointer data)
{
    SeahorseObject *defkey;
    
    if (!SEAHORSE_IS_OBJECT (obj))
	    return FALSE;
    
    defkey = seahorse_context_get_default_key (SCTX_APP ());
    
    /* Default key overrides all, and becomes the only signer available*/
    if (defkey && seahorse_object_get_id (obj) != seahorse_object_get_id (defkey))
        return FALSE;
    
    return TRUE;
}

GcrCollection *
seahorse_keyset_pgp_signers_new (void)
{
	SeahorsePredicate *predicate = g_new0 (SeahorsePredicate, 1);
	SeahorseCollection *collection;

	predicate->location = SEAHORSE_LOCATION_LOCAL;
	predicate->tag = SEAHORSE_PGP;
	predicate->usage = SEAHORSE_USAGE_PRIVATE_KEY;
	predicate->flags = SEAHORSE_FLAG_CAN_SIGN;
	predicate->nflags = SEAHORSE_FLAG_EXPIRED | SEAHORSE_FLAG_REVOKED | SEAHORSE_FLAG_DISABLED;
	predicate->custom = pgp_signers_match;

	collection = seahorse_collection_new_for_predicate (predicate, g_free);

	g_signal_connect_object (seahorse_context_pgp_settings (NULL), "changed::default-key",
	                         G_CALLBACK (on_settings_default_key_changed), collection, 0);

	return GCR_COLLECTION (collection);
}
