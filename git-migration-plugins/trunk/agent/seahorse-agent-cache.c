/*
 * Seahorse
 *
 * Copyright (C) 2004 Stefan Walter
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

#include "config.h"
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <gnome.h>

#include <gnome-keyring.h>
#include <gnome-keyring-memory.h>

#include "seahorse-gconf.h"
#include "seahorse-gpgmex.h"
#include "seahorse-agent.h"
#include "seahorse-key-source.h"
#include "seahorse-pgp-key.h"
#include "seahorse-context.h"
#include "seahorse-unknown-source.h"

/* -----------------------------------------------------------------------------
 * INTERNAL PASSWORD CACHE
 */

/*
 * Implementation of the password cache. Note that only passwords
 * are stored in secure memory, all else is allocated normally.
 * 
 * Cache items can be locked which guarantees they'll stay around even
 * if their TTL expires. 
 */

/* Override the DEBUG_CACHE switch here */
/* #define DEBUG_CACHE_ENABLE 0 */

#ifndef DEBUG_CACHE_ENABLE
#if _DEBUG
#define DEBUG_CACHE_ENABLE 1
#else
#define DEBUG_CACHE_ENABLE 0
#endif
#endif

#if DEBUG_CACHE_ENABLE
#define DEBUG_CACHE(x)     g_printerr x
#else
#define DEBUG_CACHE(x)
#endif

#define UNPARSEABLE_KEY     _("Unparseable Key ID")
#define UNKNOWN_KEY         _("Unknown/Invalid Key")
#define TRANSIENT_ID        "TRANSIENTTRANSIENT"

typedef struct sa_cache_t {
    gchar *id;                  /* The password id */
    gchar *pass;                /* The password itself (pointer to secure mem) */
    gchar *desc;                /* A description of the key (parsed below) */
    gboolean locked;            /* Whether this entry is locked in the cache */
    time_t stamp;               /* The time which this password was last accessed */
} sa_cache_t;

static GHashTable *g_cache = NULL;      /* Hash of ids to sa_cache_t */
static GMemChunk *g_memory = NULL;      /* Memory for sa_cache_t's */
static guint g_notify_id = 0;           /* gconf notify id */
static guint g_timeout_id = 0;          /* timeout of next expire */

gboolean seahorse_agent_cache_check (gpointer);

/* -----------------------------------------------------------------------------
 */

static gint
calc_ttl ()
{
    if (!seahorse_gconf_get_boolean (SETTING_CACHE)) {
        /* No caching */
        return 0;
    }

    if (seahorse_gconf_get_boolean (SETTING_EXPIRE)) {
        /* How long to cache. gconf has in minutes, we want seconds */
        return seahorse_gconf_get_integer (SETTING_TTL) * 60;
    }

    return -1;
};

static void
find_next_expiry (gpointer key, gpointer value, gpointer user_data)
{
    sa_cache_t *it = (sa_cache_t *) value;
    time_t *next = (time_t*)user_data;

    if (*next < it->stamp)
        *next = it->stamp;
}

/* Called to calculate and setup next expiry timeout */
static void 
setup_next_expiry (gboolean nextonly)
{
    time_t now, first = 0;
    gint ms, ttl;

    if (g_hash_table_size (g_cache) == 0)
        return;
    ttl = calc_ttl ();
    if (ttl == -1) /* cache indefinitely */
        return;

    g_hash_table_foreach (g_cache, find_next_expiry, &first);
    if (!first)
        return;

    if (g_timeout_id)
        g_source_remove (g_timeout_id);
    now = time (NULL);
    first += ttl;

    /* Already expired, so clear in a second */
    if (!nextonly && first <= now)
        first = now;

    ms = ((first - now) + 1) * 1000;
    g_timeout_id = g_timeout_add (ms, seahorse_agent_cache_check, NULL);

    DEBUG_CACHE (("[cache] next expiry in %d seconds\n", ms / 1000));
}


/* Check each cache item for expiry */
static gboolean
cache_enumerator (gpointer key, gpointer value, gpointer user_data)
{
    sa_cache_t *it = (sa_cache_t *) value;
    gint ttl = *((gint *) user_data);

    if (!it->locked) {
     
        /* Clear all items? */
        if (ttl == 0)
            return TRUE;

        /* Is expired? */
        else if (time (NULL) > it->stamp + ttl)
            return TRUE;
            
        /* Is transient? */
        if (strcmp (it->id, TRANSIENT_ID) == 0)
            return TRUE;
    }

    return FALSE;
}

/* Check the cache for expired items */
gboolean
seahorse_agent_cache_check (gpointer unused)
{
    g_timeout_id = 0;

    if (!g_cache)
        return FALSE;

    if (g_hash_table_size (g_cache) > 0) {
        gint ttl = calc_ttl ();
        /* negative means cache indefinitely */
        if (ttl != -1) {
            if (g_hash_table_foreach_remove (g_cache, cache_enumerator, &ttl) > 0) {
                DEBUG_CACHE (("[cache] expired cached secrets\n"));
                seahorse_agent_status_update ();
                setup_next_expiry (TRUE);
            }
        }
    }

    return FALSE;
}

/* Callback to free a cache item */
static void
destroy_cache_item (gpointer data)
{
    sa_cache_t *it = (sa_cache_t *) data;
    if (it) {
        if (it->id)
            g_free (it->id);

        if (it->desc)
            g_free (it->desc);

        if (it->pass)
            gnome_keyring_memory_free (it->pass);

        g_chunk_free (it, g_memory);
    }
}

/* Called when the AUTH gconf key changes */
static void
gconf_notify (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	/* Clear the cache when someone changes the gconf AUTH setting to false */
	if (g_str_equal (SETTING_AUTH, gconf_entry_get_key (entry))) {
        if (!gconf_value_get_bool (gconf_entry_get_value (entry)))
			seahorse_agent_cache_clearall (NULL);
	}

    /* TTL setting may have changed, so... */
    setup_next_expiry (FALSE);
}

/* Initialize the cache */
void
seahorse_agent_cache_init ()
{
    gpgme_protocol_t proto = GPGME_PROTOCOL_OpenPGP;
    gpgme_error_t err;
 
    if ((g_cache == NULL) && (g_memory == NULL)) {
        g_cache =
            g_hash_table_new_full (g_str_hash, g_str_equal, NULL, destroy_cache_item);
        g_memory = g_mem_chunk_create (SeahorseAgentPassReq, 128, G_ALLOC_AND_FREE);

        err = gpgme_engine_check_version (proto);
        g_return_if_fail (GPG_IS_OK (err));
       
        /* Listen for changes on the AUTH key */
        g_notify_id = seahorse_gconf_notify (SETTING_AUTH, gconf_notify, NULL);
    }
}

/* Uninitialize and free up cache memory */
void
seahorse_agent_cache_uninit ()
{
    if (g_cache) {
        g_idle_remove_by_data (g_cache);
        g_hash_table_destroy (g_cache);
        g_cache = NULL;
    }

    if (g_memory) {
        g_mem_chunk_destroy (g_memory);
        g_memory = NULL;
    }
    
    if (g_notify_id) {
        seahorse_gconf_unnotify (g_notify_id);
        g_notify_id = 0;
    }

    if (g_timeout_id) {
        g_source_remove (g_timeout_id);
        g_timeout_id = 0;
    }
}

/* Retrieve a password from the cache */
const gchar *
seahorse_agent_internal_get (const gchar *id)
{
    sa_cache_t *it;

    g_assert (g_cache != NULL);

    /* Always make sure the cache is properly purged before answering */    
    seahorse_agent_cache_check (NULL);

    if (id == NULL)
        id = TRANSIENT_ID;

    it = g_hash_table_lookup (g_cache, id);
    if (it) {
        /* Always updates the stamp when password retrieve */
        it->stamp = time (NULL);

        /* Locks are always one off, so unset */
        if (it->locked)
            it->locked = FALSE;
            
        return it->pass;
    }

    return NULL;
}

/* Check if a given id is in the cache, and lock if requested */
gboolean
seahorse_agent_internal_has (const gchar *id, gboolean lock)
{
    sa_cache_t *it;

    g_assert (g_cache != NULL);

    /* Always make sure the cache is properly purged before answering */    
    seahorse_agent_cache_check (NULL);
        
    if (id == NULL)
        id = TRANSIENT_ID;

    it = g_hash_table_lookup (g_cache, id);
    if (it) {
        if (lock)
            it->locked = TRUE;
        return TRUE;
    }

    return FALSE;
}

/* Remove given id from the cache */
void
seahorse_agent_internal_clear (const gchar *id)
{
    if (id == NULL)
        id = TRANSIENT_ID;
        
    g_assert (g_cache != NULL);

    /* Note that we ignore locks in this case, it was a specific request */
    g_hash_table_remove (g_cache, id);

    /* UI hooks */
    seahorse_agent_status_update ();
    setup_next_expiry (FALSE);
}

/* Callback for clearing all items */
static gboolean
remove_cache_item (gpointer key, gpointer value, gpointer user_data)
{
    sa_cache_t *it = (sa_cache_t*) value;

    /* 
     * This is a simple callback for removing all 
     * items from a GHashTable. returning TRUE removes. 
     */
    return !it->locked;
}

/* Clear all items in the cache */
void
seahorse_agent_cache_clearall ()
{
    g_assert (g_cache != NULL);

    if (g_hash_table_foreach_remove (g_cache, remove_cache_item, NULL) > 0) {
        seahorse_agent_status_update ();
        setup_next_expiry (FALSE);
    }
}


/* Set a password in the cache. encode and lock if requested */
void
seahorse_agent_internal_set (const gchar *id, const gchar *pass, gboolean lock)
{
    gboolean allocated = FALSE;
    gboolean cache;
    sa_cache_t *it;

    if (id == NULL)
        id = TRANSIENT_ID;

    g_return_if_fail (pass != NULL);

    /* Whether to cache passwords or not */
    cache = seahorse_gconf_get_boolean (SETTING_CACHE);

    /* No need to even bother the cache in this case */
    if (!cache && !lock)
        return;

    g_assert (g_cache != NULL);

    it = g_hash_table_lookup (g_cache, id);

    if (!it) {
        /* Allocate and initialize a new cache item */
        it = g_chunk_new (sa_cache_t, g_memory);
        memset (it, 0, sizeof (*it));
        it->id = g_strdup (id);
        allocated = TRUE;
    }

    g_assert (it->id != NULL);

    /* Work with the password */
    if (it->pass)
        gnome_keyring_memory_free (it->pass);
    it->pass = gnome_keyring_memory_alloc (strlen (pass) + 1);
    strcpy (it->pass, pass);

    /* If not caching set to the epoch which should always expire */
    it->stamp = cache ? time (NULL) : 0;
    it->locked = lock ? TRUE : FALSE;

    if(allocated)
        g_hash_table_replace (g_cache, it->id, it);

    /* UI hooks */
    seahorse_agent_status_update ();
    setup_next_expiry (FALSE);
}

/* Returns number of passwords in the cache */
guint
seahorse_agent_cache_count ()
{
    g_assert (g_cache != NULL);
    return g_hash_table_size (g_cache);
}

static void 
build_key_list (gpointer key, gpointer value, GList **keys)
{
    sa_cache_t *it = (sa_cache_t*)value;
    SeahorseKeySource *sksrc;
    SeahorseKey *skey;
    GQuark keyid;
    
    keyid = seahorse_pgp_key_get_cannonical_id (it->id);
    g_return_if_fail (keyid);

    skey = seahorse_context_find_key (SCTX_APP (), keyid, SKEY_LOC_LOCAL);
    if (!skey) {
        sksrc = seahorse_context_find_key_source (SCTX_APP (), SKEY_PGP, SKEY_LOC_UNKNOWN);
        g_return_if_fail (sksrc != NULL);
        skey = seahorse_unknown_source_add_key (SEAHORSE_UNKNOWN_SOURCE (sksrc), keyid, NULL);
    }
    
    if (skey) 
        *keys = g_list_prepend (*keys, skey);
}

/* Get list of all SeahorseKey's cached */
GList* 
seahorse_agent_cache_get_keys ()
{
    GList *keys = NULL;
    g_assert (g_cache != NULL);
    g_hash_table_foreach (g_cache, (GHFunc)build_key_list, &keys);
    return keys;
}


/* -----------------------------------------------------------------------------
 * GENERIC CACHE FUNCTIONS
 */

#define KEYRING_ATTR_TYPE "seahorse-key-type"
#define KEYRING_ATTR_KEYID "openpgp-keyid"
#define KEYRING_VAL_GPG "openpgp"

static gboolean 
only_internal_cache ()
{
    gboolean internal = TRUE;
    
    /* No cache, internal must still work though */
    if (!seahorse_gconf_get_boolean (SETTING_CACHE))
        internal = TRUE;
    
    else {
        gchar *method = seahorse_gconf_get_string (SETTING_METHOD);
        if (method && strcmp (method, METHOD_GNOME) == 0)
            internal = FALSE;
        g_free (method);
    }
    
    return internal;
}

static const gchar*
gkeyring_keyid (const gchar *fullid)
{
    size_t len = strlen(fullid);
    return fullid + (len > 16 ? len - 16 : 0);
}

void
seahorse_agent_cache_set (const gchar *id, const gchar *pass, gboolean lock)
{
    /* Store in our internal cache */
    seahorse_agent_internal_set (id, pass, lock);

    /* Store in gnome-keyring */
    if (id && !only_internal_cache ()) {
        
        GnomeKeyringResult res;
        GnomeKeyringAttributeList *attributes = NULL;
        guint item_id;
        gchar *desc = NULL, *name;
        SeahorseKey *skey;
        GQuark keyid;
        
        keyid = seahorse_pgp_key_get_cannonical_id (id);
        skey = seahorse_context_find_key (SCTX_APP (), keyid, SKEY_LOC_LOCAL);
        
        if (skey) {
            name = seahorse_key_get_display_name (skey);
            desc = g_strdup_printf (_("PGP Key: %s"), name);
            g_free (name);
        }
        
        attributes = gnome_keyring_attribute_list_new ();
        gnome_keyring_attribute_list_append_string (attributes, KEYRING_ATTR_TYPE, 
                                                    KEYRING_VAL_GPG);
        gnome_keyring_attribute_list_append_string (attributes, KEYRING_ATTR_KEYID, 
                                                    gkeyring_keyid (id));
        res = gnome_keyring_item_create_sync (NULL, GNOME_KEYRING_ITEM_GENERIC_SECRET, 
                                              desc, attributes, pass, TRUE, &item_id);
        gnome_keyring_attribute_list_free (attributes);
        g_free (desc);
        
        if (res != GNOME_KEYRING_RESULT_OK)
            g_warning ("Couldn't store password in keyring: (code %d)", res);
    }
}

void
seahorse_agent_cache_clear (const gchar *id)
{
    /* Clear from our internal cache */
    seahorse_agent_internal_clear (id);
    
    /* Clear from gnome-keyring */
    if (id && !only_internal_cache ()) {
        
        GnomeKeyringResult res;
        GnomeKeyringAttributeList *attributes = NULL;
        GList *found_items, *l;
        
        attributes = gnome_keyring_attribute_list_new ();
        gnome_keyring_attribute_list_append_string (attributes, KEYRING_ATTR_KEYID, 
                                                    gkeyring_keyid (id));
        res = gnome_keyring_find_items_sync (GNOME_KEYRING_ITEM_GENERIC_SECRET,
                                             attributes, &found_items);
        gnome_keyring_attribute_list_free (attributes);
        
        if (res != GNOME_KEYRING_RESULT_OK) {
            g_warning ("couldn't search keyring: (code %d)", res);
            
        } else {
            for (l = found_items; l; l = g_list_next (l)) {
                /* TODO: Can we use async here? */
                res = gnome_keyring_item_delete_sync (NULL, 
                                    ((GnomeKeyringFound*)(l->data))->item_id);
                if (res != GNOME_KEYRING_RESULT_OK)
                    g_warning ("Couldn't clear password from keyring: (code %d)", res);
            }
            
            gnome_keyring_found_list_free (found_items);
        }
        
    }
}

const gchar* 
seahorse_agent_cache_get (const gchar *id)
{
    const gchar *ret = NULL;
    
    /* Always look in our own keyring first */
    ret = seahorse_agent_internal_get (id);
    
    /* Clear from gnome-keyring */
    if (!ret && id && !only_internal_cache ()) {
        
        GnomeKeyringResult res;
        GnomeKeyringAttributeList *attributes = NULL;
        GList *found_items;
        GnomeKeyringFound *found;
        
        attributes = gnome_keyring_attribute_list_new ();
        gnome_keyring_attribute_list_append_string (attributes, KEYRING_ATTR_KEYID, 
                                                    gkeyring_keyid (id));
        res = gnome_keyring_find_items_sync (GNOME_KEYRING_ITEM_GENERIC_SECRET,
                                             attributes, &found_items);
        gnome_keyring_attribute_list_free (attributes);
        
        if (res != GNOME_KEYRING_RESULT_OK) {
            if (res != GNOME_KEYRING_RESULT_DENIED)
                g_warning ("couldn't search keyring: (code %d)", res);
            
        } else {
            
            if (found_items && found_items->data) {
                found = (GnomeKeyringFound*)found_items->data;
                if (found->secret) {
                    
                    /* Store it temporarily in our loving hands */
                    seahorse_agent_internal_set (NULL, found->secret, TRUE);
                    ret = seahorse_agent_internal_get (NULL);
                
                }
            }
            
            gnome_keyring_found_list_free (found_items);
        }
        
    }
    
    return ret;
}

/* Check if a given id is in the cache, and lock if requested */
gboolean
seahorse_agent_cache_has (const gchar *id, gboolean lock)
{
    if (seahorse_agent_internal_has (id, lock))
        return TRUE;
    
    /* Retrieve from keyring and lock in local */
    if (id && !only_internal_cache ()) {
        
        const gchar *pass = seahorse_agent_cache_get (id);
        if (!pass)
            return FALSE;
        
        /* Store it in our loving hands */
        seahorse_agent_internal_set (id, pass, TRUE);
        return TRUE;
    }
    
    return FALSE;
}
