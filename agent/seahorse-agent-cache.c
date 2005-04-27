/*
 * Seahorse
 *
 * Copyright (C) 2004 Nate Nielsen
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

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <gnome.h>

#include "seahorse-gconf.h"
#include "seahorse-gpgmex.h"
#include "seahorse-agent.h"
#include "seahorse-agent-secmem.h"

/*
 * Implementation of the password cache. Note that only passwords
 * are stored in secure memory, all else is allocated normally.
 * 
 * Cache items can be locked which guarantees they'll stay around even
 * if their TTL expires. 
 */

#define UNPARSEABLE_KEY     _("Unparseable Key ID")
#define UNKNOWN_KEY         _("Unknown/Invalid Key")
#define TRANSIENT_ID        "TRANSIENT"

typedef struct sa_cache_t {
    gchar *id;                  /* The password id */
    gchar *pass;                /* The password itself (pointer to secure mem) */
    gchar *desc;                /* A description of the key (parsed below) */
    gboolean locked;            /* Whether this entry is locked in the cache */
    time_t stamp;               /* The time which this password was last accessed */
} sa_cache_t;

static GHashTable *g_cache = NULL;      /* Hash of ids to sa_cache_t */
static GMemChunk *g_memory = NULL;      /* Memory for sa_cache_t's */
static gpgme_ctx_t g_ctx = NULL;        /* Context for looking up ids */

/* -----------------------------------------------------------------------------
 */

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
    gint ttl = -1;

    if (!g_cache)
        return FALSE;

    if (g_hash_table_size (g_cache) > 0) {
        if (!seahorse_gconf_get_boolean (SETTING_CACHE)) {
            /* No caching */
            ttl = 0;
        }

        else if (seahorse_gconf_get_boolean (SETTING_EXPIRE)) {
            /* How long to cache. gconf has in minutes, we want seconds */
            ttl = seahorse_gconf_get_integer (SETTING_TTL) * 60;
        }

        /* negative means cache indefinitely */
        if (ttl != -1) {
            if (g_hash_table_foreach_remove (g_cache, cache_enumerator, &ttl) > 0)
                seahorse_agent_status_update ();
        }
    }

    return TRUE;
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
            secmem_free (it->pass);

        g_chunk_free (it, g_memory);
    }
}

/* Initialize the cache */
void
seahorse_agent_cache_init ()
{
    gpgme_protocol_t proto = GPGME_PROTOCOL_OpenPGP;
    gpgme_error_t err;
 
    g_assert (g_cache == NULL);
    g_assert (g_memory == NULL);

    g_cache =
        g_hash_table_new_full (g_str_hash, g_str_equal, NULL, destroy_cache_item);
    g_memory = g_mem_chunk_create (SeahorseAgentPassReq, 128, G_ALLOC_AND_FREE);

    /* Handle the cache timeouts */
    g_timeout_add (1000, seahorse_agent_cache_check, g_cache);
    
    err = gpgme_engine_check_version (proto);
    g_return_if_fail (GPG_IS_OK (err));
   
    err = gpgme_new (&g_ctx);
    g_return_if_fail (GPG_IS_OK (err));
   
    err = gpgme_set_protocol (g_ctx, proto);
    g_return_if_fail (GPG_IS_OK (err));
   
    gpgme_set_keylist_mode (g_ctx, GPGME_KEYLIST_MODE_LOCAL);
}

/* Uninitialize and free up cache memory */
void
seahorse_agent_cache_uninit ()
{
    if (g_cache);
    {
        g_idle_remove_by_data (g_cache);
        g_hash_table_destroy (g_cache);
        g_cache = NULL;
    }

    if (g_memory) {
        g_mem_chunk_destroy (g_memory);
        g_memory = NULL;
    }
    
    if (g_ctx) {
        gpgme_release (g_ctx);
        g_ctx = NULL;
    }
}

/* Retrieve a password from the cache */
const gchar *
seahorse_agent_cache_get (const gchar *id)
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
seahorse_agent_cache_has (const gchar *id, gboolean lock)
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
seahorse_agent_cache_clear (const gchar *id)
{
    if (id == NULL)
        id = TRANSIENT_ID;
        
    g_assert (g_cache != NULL);

    /* Note that we ignore locks in this case, it was a specific request */
    g_hash_table_remove (g_cache, id);

    /* UI hooks */
    seahorse_agent_status_update ();
}

/* Callback for clearing all items */
static gboolean
remove_cache_item (gpointer key, gpointer value, gpointer user_data)
{
    /* 
     * This is a simple callback for removing all 
     * items from a GHashTable. returning TRUE removes. 
     */
    return TRUE;
}

/* Clear all items in the cache */
void
seahorse_agent_cache_clearall ()
{
    g_assert (g_cache != NULL);

    if (g_hash_table_foreach_remove (g_cache, remove_cache_item, NULL) > 0)
        seahorse_agent_status_update ();
}

/* Encode a password in hex */
static void
encode_password (gchar *dest, const gchar *src)
{
    static const char HEXC[] = "0123456789abcdef";
    int j;

    g_assert (dest && src);

    /* Simple hex encoding */

    while (*src) {
        j = *(src) >> 4 & 0xf;
        *(dest++) = HEXC[j];

        j = *(src++) & 0xf;
        *(dest++) = HEXC[j];
    }
}

/* Set a password in the cache. encode and lock if requested */
void
seahorse_agent_cache_set (const gchar *id, const gchar *pass,
                          gboolean encode, gboolean lock)
{
    int len;
    gboolean cache;
    sa_cache_t *it;

    if (id == NULL)
        id = TRANSIENT_ID;

    g_assert (pass && pass[0]);

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
    }

    g_assert (it->id != NULL);

    /* Work with the password */

    if (it->pass)
        secmem_free (it->pass);

    len = strlen (pass);

    if (encode) {
        it->pass = (gchar *) secmem_malloc (sizeof (gchar *) * ((len * 2) + 1));
        if (!it->pass) {
            g_critical ("out of secure memory");
            return;
        }

        encode_password (it->pass, pass);
    }

    else {
        it->pass = (gchar *) secmem_malloc (sizeof (gchar) * (len + 1));
        if (!it->pass) {
            g_critical ("out of secure memory");
            return;
        }

        strcpy (it->pass, pass);
    }

    /* If not caching set to the epoch which should always expire */
    it->stamp = cache ? time (NULL) : 0;
    it->locked = lock ? TRUE : FALSE;

    g_hash_table_replace (g_cache, it->id, it);

    /* UI hooks */
    seahorse_agent_status_update ();
}

/* Returns number of passwords in the cache */
guint
seahorse_agent_cache_count ()
{
    g_assert (g_cache != NULL);
    return g_hash_table_size (g_cache);
}

/* For enumerating through all ids in the cache */
void
seahorse_agent_cache_enum (GHFunc func, gpointer user_data)
{
    g_assert (g_cache != NULL);
    g_assert (func != NULL);
    g_hash_table_foreach (g_cache, func, user_data);
}

/* Get the displayable name of a given cache item */
gchar *
seahorse_agent_cache_getname (const gchar *id)
{
    gpgme_key_t key;
    gchar *userid;
    
    if (!g_ctx)
        return g_strdup (UNKNOWN_KEY);

    g_return_val_if_fail (g_ctx != NULL, g_strdup (UNKNOWN_KEY));
    g_return_val_if_fail (id != NULL, g_strdup (UNKNOWN_KEY));
        
    gpgme_get_key (g_ctx, id, &key, 1);
    if (key == NULL) {
        gpgme_get_key (g_ctx, id, &key, 0);
        if (key == NULL) 
            return g_strdup (UNKNOWN_KEY);
    }

    userid = (key->uids && key->uids->uid) ? key->uids->uid : UNKNOWN_KEY;
    
    /* If not utf8 valid, assume latin 1 */
    if (!g_utf8_validate (userid, -1, NULL))
        userid = g_convert (userid, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
    else
        userid = g_strdup (userid);

    gpgme_key_unref (key);
    return userid;
}
