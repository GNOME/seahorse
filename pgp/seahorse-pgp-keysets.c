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

#include "seahorse-gconf.h"

#include "seahorse-pgp.h"
#include "seahorse-pgp-keysets.h"

/* -----------------------------------------------------------------------------
 * COMMON KEYSETS 
 */

static void
pgp_signers_gconf_notify (GConfClient *client, guint id, GConfEntry *entry, 
                          SeahorseKeyset *skset)
{
    /* Default key changed, refresh */
    seahorse_keyset_refresh (skset);
}

static gboolean 
pgp_signers_match (SeahorseKey *key, gpointer data)
{
    SeahorseKey *defkey = seahorse_context_get_default_key (SCTX_APP ());
    
    /* Default key overrides all, and becomes the only signer available*/
    if (defkey && seahorse_key_get_keyid (key) != seahorse_key_get_keyid (defkey))
        return FALSE;
    
    return TRUE;
}

SeahorseKeyset*     
seahorse_keyset_pgp_signers_new ()
{
    SeahorseKeyPredicate *pred = g_new0(SeahorseKeyPredicate, 1);
    SeahorseKeyset *skset;
    
    pred->location = SKEY_LOC_LOCAL;
    pred->ktype = SEAHORSE_PGP;
    pred->etype = SKEY_PRIVATE;
    pred->flags = SKEY_FLAG_CAN_SIGN;
    pred->nflags = SKEY_FLAG_EXPIRED | SKEY_FLAG_REVOKED | SKEY_FLAG_DISABLED;
    pred->custom = pgp_signers_match;
    
    skset = seahorse_keyset_new_full (pred);
    g_object_set_data_full (G_OBJECT (skset), "pgp-signers-predicate", pred, g_free);
    
    seahorse_gconf_notify_lazy (SEAHORSE_DEFAULT_KEY, 
                                (GConfClientNotifyFunc)pgp_signers_gconf_notify, 
                                skset, skset);
    return skset;
}
