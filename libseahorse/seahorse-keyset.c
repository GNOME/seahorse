/*
 * Seahorse
 *
 * Copyright (C) 2005 Nate Nielsen
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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "seahorse-keyset.h"
#include "seahorse-marshal.h"
#include "seahorse-gconf.h"
#include "seahorse-pgp-key.h"

enum {
    PROP_0,
    PROP_PREDICATE
};

enum {
    ADDED,
    REMOVED,
    CHANGED,
    SET_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _SeahorseKeysetPrivate {
    GHashTable *keys;
    SeahorseKeyPredicate *pred;
};

G_DEFINE_TYPE (SeahorseKeyset, seahorse_keyset, G_TYPE_OBJECT);

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static gboolean
remove_update (SeahorseKey *skey, gpointer closure, SeahorseKeyset *skset)
{
    if (closure == GINT_TO_POINTER (TRUE))
        closure = NULL;
    
    g_signal_emit (skset, signals[REMOVED], 0, skey, closure);
    g_signal_emit (skset, signals[SET_CHANGED], 0);
    return TRUE;
}

static void
remove_key  (SeahorseKey *skey, gpointer closure, SeahorseKeyset *skset)
{
    if (!closure)
        closure = g_hash_table_lookup (skset->pv->keys, skey);
    
    g_hash_table_remove (skset->pv->keys, skey);
    remove_update (skey, closure, skset);
}

static gboolean
maybe_add_key (SeahorseKeyset *skset, SeahorseKey *skey)
{
    if (!seahorse_context_owns_key (SCTX_APP (), skey))
        return FALSE;

    if (g_hash_table_lookup (skset->pv->keys, skey))
        return FALSE;
    
    if (!skset->pv->pred || !seahorse_key_predicate_match (skset->pv->pred, skey))
        return FALSE;
    
    g_hash_table_replace (skset->pv->keys, skey, GINT_TO_POINTER (TRUE));
    g_signal_emit (skset, signals[ADDED], 0, skey);
    g_signal_emit (skset, signals[SET_CHANGED], 0);
    return TRUE;
}

static gboolean
maybe_remove_key (SeahorseKeyset *skset, SeahorseKey *skey)
{
    if (!g_hash_table_lookup (skset->pv->keys, skey))
        return FALSE;
    
    if (skset->pv->pred && seahorse_key_predicate_match (skset->pv->pred, skey))
        return FALSE;
    
    remove_key (skey, NULL, skset);
    return TRUE;
}

static void
key_added (SeahorseContext *sctx, SeahorseKey *skey, SeahorseKeyset *skset)
{
    g_assert (SEAHORSE_IS_KEY (skey));
    g_assert (SEAHORSE_IS_KEYSET (skset));

    maybe_add_key (skset, skey);
}

static void 
key_removed (SeahorseContext *sctx, SeahorseKey *skey, SeahorseKeyset *skset)
{
    g_assert (SEAHORSE_IS_KEY (skey));
    g_assert (SEAHORSE_IS_KEYSET (skset));

    if (g_hash_table_lookup (skset->pv->keys, skey))
        remove_key (skey, NULL, skset);
}

static void
key_changed (SeahorseContext *sctx, SeahorseKey *skey, SeahorseKeyChange change,
             SeahorseKeyset *skset)
{
    gpointer closure = g_hash_table_lookup (skset->pv->keys, skey);

    g_assert (SEAHORSE_IS_KEY (skey));
    g_assert (SEAHORSE_IS_KEYSET (skset));

    if (closure) {
        
        /* See if needs to be removed, otherwise emit signal */
        if (!maybe_remove_key (skset, skey)) {
            
            if (closure == GINT_TO_POINTER (TRUE))
                closure = NULL;
            
            g_signal_emit (skset, signals[CHANGED], 0, skey, change, closure);
        }
        
    /* Not in our set yet */
    } else 
        maybe_add_key (skset, skey);
}

static void
keys_to_list (SeahorseKey *skey, gpointer *c, GList **l)
{
    *l = g_list_append (*l, skey);
}

static void
keys_to_hash (SeahorseKey *skey, gpointer *c, GHashTable *ht)
{
    g_hash_table_replace (ht, skey, NULL);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_keyset_dispose (GObject *gobject)
{
    SeahorseKeyset *skset = SEAHORSE_KEYSET (gobject);
    
    /* Release all our pointers and stuff */
    g_hash_table_foreach_remove (skset->pv->keys, (GHRFunc)remove_update, skset);
    skset->pv->pred = NULL;
    
    g_signal_handlers_disconnect_by_func (SCTX_APP (), key_added, skset);    
    g_signal_handlers_disconnect_by_func (SCTX_APP (), key_removed, skset);    
    g_signal_handlers_disconnect_by_func (SCTX_APP (), key_changed, skset);    
    
	G_OBJECT_CLASS (seahorse_keyset_parent_class)->dispose (gobject);
}

static void
seahorse_keyset_finalize (GObject *gobject)
{
    SeahorseKeyset *skset = SEAHORSE_KEYSET (gobject);

    g_hash_table_destroy (skset->pv->keys);
    g_assert (skset->pv->pred == NULL);
    g_free (skset->pv);
    
	G_OBJECT_CLASS (seahorse_keyset_parent_class)->finalize (gobject);
}

static void
seahorse_keyset_set_property (GObject *object, guint prop_id, const GValue *value, 
                              GParamSpec *pspec)
{
	SeahorseKeyset *skset = SEAHORSE_KEYSET (object);
	
    switch (prop_id) {
    case PROP_PREDICATE:
        skset->pv->pred = (SeahorseKeyPredicate*)g_value_get_pointer (value);        
        seahorse_keyset_refresh (skset);
        break;
    }
}

static void
seahorse_keyset_get_property (GObject *object, guint prop_id, GValue *value, 
                              GParamSpec *pspec)
{
    SeahorseKeyset *skset = SEAHORSE_KEYSET (object);
	
    switch (prop_id) {
    case PROP_PREDICATE:
        g_value_set_pointer (value, skset->pv->pred);
        break;
    }
}

static void
seahorse_keyset_init (SeahorseKeyset *skset)
{
	/* init private vars */
	skset->pv = g_new0 (SeahorseKeysetPrivate, 1);

    skset->pv->keys = g_hash_table_new (g_direct_hash, g_direct_equal);
    
    g_signal_connect (SCTX_APP (), "added", G_CALLBACK (key_added), skset);
    g_signal_connect (SCTX_APP (), "removed", G_CALLBACK (key_removed), skset);
    g_signal_connect (SCTX_APP (), "changed", G_CALLBACK (key_changed), skset);
}

static void
seahorse_keyset_class_init (SeahorseKeysetClass *klass)
{
    GObjectClass *gobject_class;
    
    seahorse_keyset_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->dispose = seahorse_keyset_dispose;
    gobject_class->finalize = seahorse_keyset_finalize;
    gobject_class->set_property = seahorse_keyset_set_property;
    gobject_class->get_property = seahorse_keyset_get_property;
    
    g_object_class_install_property (gobject_class, PROP_PREDICATE,
        g_param_spec_pointer ("predicate", "Predicate", "Predicate for matching keys into this set.", 
                              G_PARAM_READWRITE));
    
    signals[ADDED] = g_signal_new ("added", SEAHORSE_TYPE_KEYSET, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseKeysetClass, added),
                NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, SEAHORSE_TYPE_KEY);
    
    signals[REMOVED] = g_signal_new ("removed", SEAHORSE_TYPE_KEYSET, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseKeysetClass, removed),
                NULL, NULL, seahorse_marshal_VOID__OBJECT_POINTER, G_TYPE_NONE, 2, SEAHORSE_TYPE_KEY, G_TYPE_POINTER);    
    
    signals[CHANGED] = g_signal_new ("changed", SEAHORSE_TYPE_KEYSET, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseKeysetClass, changed),
                NULL, NULL, seahorse_marshal_VOID__OBJECT_UINT_POINTER, G_TYPE_NONE, 3, SEAHORSE_TYPE_KEY, G_TYPE_UINT, G_TYPE_POINTER);
                
    signals[SET_CHANGED] = g_signal_new ("set-changed", SEAHORSE_TYPE_KEYSET,
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseKeysetClass, set_changed),
                NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseKeyset*
seahorse_keyset_new (GQuark ktype, SeahorseKeyEType etype, SeahorseKeyLoc location,
                     guint flags, guint nflags)
{
    SeahorseKeyset *skset;
    SeahorseKeyPredicate *pred = g_new0(SeahorseKeyPredicate, 1);
    
    pred->location = location;
    pred->ktype = ktype;
    pred->etype = etype;
    pred->flags = flags;
    pred->nflags = nflags;
    
    skset = seahorse_keyset_new_full (pred);
    g_object_set_data_full (G_OBJECT (skset), "quick-predicate", pred, g_free);
    
    return skset;
}

SeahorseKeyset*     
seahorse_keyset_new_full (SeahorseKeyPredicate *pred)
{
    return g_object_new (SEAHORSE_TYPE_KEYSET, "predicate", pred, NULL);
}

gboolean
seahorse_keyset_has_key (SeahorseKeyset *skset, SeahorseKey *skey)
{
    if (g_hash_table_lookup (skset->pv->keys, skey))
        return TRUE;
    
    /* 
     * This happens when the key has changed state, but we have 
     * not yet received the signal. 
     */
    if (maybe_add_key (skset, skey))
        return TRUE;
    
    return FALSE;
}

GList*             
seahorse_keyset_get_keys (SeahorseKeyset *skset)
{
    GList *keys = NULL;
    g_hash_table_foreach (skset->pv->keys, (GHFunc)keys_to_list, &keys);
    return keys;
}

guint
seahorse_keyset_get_count (SeahorseKeyset *skset)
{
    return g_hash_table_size (skset->pv->keys);
}

void
seahorse_keyset_refresh (SeahorseKeyset *skset)
{
    GHashTable *check = g_hash_table_new (g_direct_hash, g_direct_equal);
    GList *l, *keys = NULL;
    SeahorseKey *skey;
    
    /* Make note of all the keys we had prior to refresh */
    g_hash_table_foreach (skset->pv->keys, (GHFunc)keys_to_hash, check);
    
    if (skset->pv->pred)
        keys = seahorse_context_find_keys_full (SCTX_APP (), skset->pv->pred);
    
    for (l = keys; l; l = g_list_next (l)) {
        skey = SEAHORSE_KEY (l->data);
        
        /* Make note that we've seen this key */
        g_hash_table_remove (check, skey);

        /* This will add to keyset */
        maybe_add_key (skset, skey);
    }
    
    g_list_free (keys);
    
    g_hash_table_foreach (check, (GHFunc)remove_key, skset);
    g_hash_table_destroy (check);
}

gpointer
seahorse_keyset_get_closure (SeahorseKeyset *skset, SeahorseKey *skey)
{
    gpointer closure = g_hash_table_lookup (skset->pv->keys, skey);
    g_return_val_if_fail (closure != NULL, NULL);

    /* |TRUE| means no closure has been set */
    if (closure == GINT_TO_POINTER (TRUE))
        return NULL;
    
    return closure;
}

void
seahorse_keyset_set_closure (SeahorseKeyset *skset, SeahorseKey *skey, 
                             gpointer closure)
{
    /* Make sure we have the key */
    g_return_if_fail (g_hash_table_lookup (skset->pv->keys, skey) != NULL);
    
    /* |TRUE| means no closure has been set */
    if (closure == NULL)
        closure = GINT_TO_POINTER (TRUE);

    g_hash_table_insert (skset->pv->keys, skey, closure);    
}

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
    pred->ktype = SKEY_PGP;
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
