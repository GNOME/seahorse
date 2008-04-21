
#include "config.h"

#include "seahorse-gconf.h"

#include "sea-pgp.h"
#include "sea-pgp-keysets.h"

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
    pred->ktype = SEA_PGP;
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
