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

#include <stdlib.h>
#include <libintl.h>
#include <gnome.h>

#include "config.h"
#include "seahorse-gpgmex.h"
#include "seahorse-operation.h"
#include "seahorse-ldap-source.h"
#include "seahorse-hkp-source.h"
#include "seahorse-server-source.h"
#include "seahorse-multi-source.h"
#include "seahorse-util.h"
#include "seahorse-key.h"
#include "seahorse-key-pair.h"

enum {
    PROP_0,
    PROP_PATTERN,
    PROP_KEY_SERVER,
    PROP_LOCAL_SOURCE
};

/* -----------------------------------------------------------------------------
 *  SERVER SOURCE
 */
 
struct _SeahorseServerSourcePrivate {
    SeahorseKeySource *local;
    GHashTable *keys;
    SeahorseOperation *operation;
    gchar *server;
    gchar *pattern;
};

/* GObject handlers */
static void seahorse_server_source_class_init (SeahorseServerSourceClass *klass);
static void seahorse_server_source_init       (SeahorseServerSource *ssrc);
static void seahorse_server_source_dispose    (GObject *gobject);
static void seahorse_server_source_finalize   (GObject *gobject);
static void seahorse_server_get_property      (GObject *object,
                                               guint prop_id,
                                               GValue *value,
                                               GParamSpec *pspec);
static void seahorse_server_set_property      (GObject *object,
                                               guint prop_id,
                                               const GValue *value,
                                               GParamSpec *pspec);
                                               
/* SeahorseKeySource methods */
static void         seahorse_server_source_stop        (SeahorseKeySource *src);
static guint        seahorse_server_source_get_state   (SeahorseKeySource *src);
SeahorseKey*        seahorse_server_source_get_key     (SeahorseKeySource *source,
                                                        const gchar *fpr);
static GList*       seahorse_server_source_get_keys    (SeahorseKeySource *src,
                                                        gboolean secret_only);
static guint        seahorse_server_source_get_count   (SeahorseKeySource *src,
                                                        gboolean secret_only);
static gpgme_ctx_t  seahorse_server_source_new_context (SeahorseKeySource *src);

static SeahorseOperation*  seahorse_server_source_get_operation     (SeahorseKeySource *sksrc);
static SeahorseOperation*  seahorse_server_source_refresh           (SeahorseKeySource *src,
                                                                     const gchar *fpr);

/* Other forward decls */
static gboolean release_key (const gchar* id, SeahorseKey *skey, SeahorseServerSource *ssrc);

static GObjectClass *parent_class = NULL;

GType
seahorse_server_source_get_type (void)
{
    static GType server_source_type = 0;
 
    if (!server_source_type) {
        
        static const GTypeInfo server_source_info = {
            sizeof (SeahorseServerSourceClass), NULL, NULL,
            (GClassInitFunc) seahorse_server_source_class_init, NULL, NULL,
            sizeof (SeahorseServerSource), 0, (GInstanceInitFunc) seahorse_server_source_init
        };
        
        server_source_type = g_type_register_static (SEAHORSE_TYPE_KEY_SOURCE, 
                                 "SeahorseServerSource", &server_source_info, 0);
    }
  
    return server_source_type;
}

/* Initialize the basic class stuff */
static void
seahorse_server_source_class_init (SeahorseServerSourceClass *klass)
{
    GObjectClass *gobject_class;
    SeahorseKeySourceClass *key_class;
   
    parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    key_class = SEAHORSE_KEY_SOURCE_CLASS (klass);
        
    key_class->refresh = seahorse_server_source_refresh;
    key_class->stop = seahorse_server_source_stop;
    key_class->get_count = seahorse_server_source_get_count;
    key_class->get_key = seahorse_server_source_get_key;
    key_class->get_keys = seahorse_server_source_get_keys;
    key_class->get_state = seahorse_server_source_get_state;
    key_class->get_operation = seahorse_server_source_get_operation;
    key_class->new_context = seahorse_server_source_new_context;    
    
    gobject_class->dispose = seahorse_server_source_dispose;
    gobject_class->finalize = seahorse_server_source_finalize;
    gobject_class->set_property = seahorse_server_set_property;
    gobject_class->get_property = seahorse_server_get_property;
    
    g_object_class_install_property (gobject_class, PROP_LOCAL_SOURCE,
            g_param_spec_object ("local-source", "Local Source",
                                  "Local Source that this represents",
                                  SEAHORSE_TYPE_KEY_SOURCE,
                                  G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
                                  
    g_object_class_install_property (gobject_class, PROP_KEY_SERVER,
            g_param_spec_string ("key-server", "Key Server",
                                 "Key Server to search on", "",
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_PATTERN,
            g_param_spec_string ("pattern", "Search Pattern",
                                 "Key Server search pattern", "",
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
}

/* init context, private vars, set prefs, connect signals */
static void
seahorse_server_source_init (SeahorseServerSource *ssrc)
{
    /* init private vars */
    ssrc->priv = g_new0 (SeahorseServerSourcePrivate, 1);
    ssrc->priv->keys = g_hash_table_new (g_str_hash, g_str_equal);
    ssrc->priv->operation = seahorse_operation_new_complete (NULL);
}

/* dispose of all our internal references */
static void
seahorse_server_source_dispose (GObject *gobject)
{
    SeahorseServerSource *ssrc;
    SeahorseKeySource *sksrc;
    
    /*
     * Note that after this executes the rest of the object should
     * still work without a segfault. This basically nullifies the 
     * object, but doesn't free it.
     * 
     * This function should also be able to run multiple times.
     */
  
    ssrc = SEAHORSE_SERVER_SOURCE (gobject);
    sksrc = SEAHORSE_KEY_SOURCE (gobject);
    g_assert (ssrc->priv);
    
    /* Clear out all the operations */
    if (ssrc->priv->operation) {
        if(!seahorse_operation_is_done (ssrc->priv->operation))
            seahorse_operation_cancel (ssrc->priv->operation);
        g_object_unref (ssrc->priv->operation);
        ssrc->priv->operation = NULL;
    }

    /* Release our references on the keys */
    g_hash_table_foreach_remove (ssrc->priv->keys, (GHRFunc)release_key, ssrc);

    if (ssrc->priv->local) {
        g_object_unref(ssrc->priv->local);
        ssrc->priv->local = NULL;
        sksrc->ctx = NULL;
    }
 
    G_OBJECT_CLASS (parent_class)->dispose (gobject);
}

/* free private vars */
static void
seahorse_server_source_finalize (GObject *gobject)
{
    SeahorseServerSource *ssrc;
  
    ssrc = SEAHORSE_SERVER_SOURCE (gobject);
    g_assert (ssrc->priv);
    
    /* We should have no keys at this point */
    g_assert (g_hash_table_size (ssrc->priv->keys) == 0);
    
    g_free (ssrc->priv->server);
    g_free (ssrc->priv->pattern);
    g_hash_table_destroy (ssrc->priv->keys);
    g_assert (ssrc->priv->operation == NULL);
    g_assert (ssrc->priv->local == NULL);
    
    g_free (ssrc->priv);
 
    G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void 
seahorse_server_set_property (GObject *object, guint prop_id, 
                              const GValue *value, GParamSpec *pspec)
{
    SeahorseServerSource *ssrc;
    SeahorseKeySource *sksrc;
 
    ssrc = SEAHORSE_SERVER_SOURCE (object);
    sksrc = SEAHORSE_KEY_SOURCE (object);
  
    switch (prop_id) {
    case PROP_LOCAL_SOURCE:
        g_return_if_fail (ssrc->priv->local == NULL);
        ssrc->priv->local = g_value_get_object (value);
        g_object_ref (ssrc->priv->local);
        sksrc->ctx = ssrc->priv->local->ctx;
        g_return_if_fail (gpgme_get_protocol(sksrc->ctx) == GPGME_PROTOCOL_OpenPGP);
        break;
    case PROP_KEY_SERVER:
        g_return_if_fail (ssrc->priv->server == NULL);
        ssrc->priv->server = g_strdup (g_value_get_string (value));
        g_return_if_fail (ssrc->priv->server && ssrc->priv->server[0] != 0);
        break;
    case PROP_PATTERN:
        g_return_if_fail (ssrc->priv->pattern == NULL);
        ssrc->priv->pattern = g_strdup (g_value_get_string (value));
        g_return_if_fail (ssrc->priv->pattern && ssrc->priv->pattern[0] != 0);
        break;
    default:
        break;
    }  
}

static void 
seahorse_server_get_property (GObject *object, guint prop_id, GValue *value,
                              GParamSpec *pspec)
{
    SeahorseServerSource *ssrc;
 
    ssrc = SEAHORSE_SERVER_SOURCE (object);
  
    switch (prop_id) {
    case PROP_LOCAL_SOURCE:
        g_value_set_object (value, ssrc->priv->local);
        break;
    case PROP_KEY_SERVER:
        g_value_set_string (value, ssrc->priv->server);
        break;
    case PROP_PATTERN:
        g_value_set_string (value, ssrc->priv->pattern);
        break;
    default:
        break;
    }       
  
}


/* --------------------------------------------------------------------------
 * HELPERS 
 */
 
/* Release a key from our internal tables and let the world know about it */
static gboolean
release_key_notify (const gchar *id, SeahorseKey *skey, SeahorseServerSource *ssrc)
{
    seahorse_key_source_removed (SEAHORSE_KEY_SOURCE (ssrc), skey);
    release_key (id, skey, ssrc);
    return TRUE;
}

/* Remove the given key from our source */
static void
remove_key_from_source (const gchar *id, SeahorseKey *dummy, SeahorseServerSource *ssrc)
{
    /* This function gets called as a GHRFunc on the lctx->checks 
     * hashtable. That hash table doesn't contain any keys, only ids,
     * so we lookup the key in the main hashtable, and use that */
     
    SeahorseKey *skey = g_hash_table_lookup (ssrc->priv->keys, id);
    if (skey != NULL) {
        g_hash_table_remove (ssrc->priv->keys, id);
        release_key_notify (id, skey, ssrc);
    }
}

/* A #SeahorseKey has been destroyed. Remove it */
static void
key_destroyed (GObject *object, SeahorseServerSource *ssrc)
{
    SeahorseKey *skey;
    skey = SEAHORSE_KEY (object);

    remove_key_from_source (seahorse_key_get_id (skey->key), skey, ssrc);
}

/* Release a key from our internal tables */
static gboolean
release_key (const gchar* id, SeahorseKey *skey, SeahorseServerSource *ssrc)
{
    g_return_val_if_fail (SEAHORSE_IS_KEY (skey), TRUE);
    g_return_val_if_fail (SEAHORSE_IS_SERVER_SOURCE (ssrc), TRUE);
    
    g_signal_handlers_disconnect_by_func (skey, key_destroyed, ssrc);
    g_object_unref (skey);
    return TRUE;
}

/* Combine information from one key and tack onto others */
static void 
combine_keys (SeahorseServerSource *ssrc, SeahorseKey *skey, gpgme_key_t key)
{
    gpgme_user_id_t uid;
    gpgme_user_id_t u;
    gpgme_subkey_t subkey;
    gpgme_subkey_t s;
    gpgme_key_t k = skey->key; 
    gboolean found;
    
    g_return_if_fail (k != NULL);
    g_return_if_fail (key != NULL);
    
    /* Go through user ids */
    for (uid = key->uids; uid != NULL; uid = uid->next) {
        g_assert (uid->uid);
        found = FALSE;
        
        for (u = k->uids; u != NULL; u = u->next) {
            g_assert (u->uid);
            
            if (strcmp (u->uid, uid->uid) == 0) {
                found = TRUE;
                break;
            }
        }
        
        if (!found)
            gpgmex_key_copy_uid (k, uid);
    }
    
    /* Go through subkeys */
    for (subkey = key->subkeys; subkey != NULL; subkey = subkey->next) {
        g_assert (subkey->fpr);
        found = FALSE;
        
        for (s = k->subkeys; s != NULL; s = s->next) {
            g_assert (s->fpr);
            
            if (strcmp (s->fpr, subkey->fpr) == 0) {
                found = TRUE;
                break;
            }
        }
        
        if (!found)
            gpgmex_key_copy_subkey (k, subkey);
    }
}

/* Add a key to our internal tables, possibly overwriting or combining with other keys  */
void
seahorse_server_source_add_key (SeahorseServerSource *ssrc, gpgme_key_t key)
{
    SeahorseKey *prev;
    SeahorseKey *skey;
    const gchar *id;
       
    g_return_if_fail (SEAHORSE_IS_SERVER_SOURCE (ssrc));

    id = seahorse_key_get_id (key);
    prev = g_hash_table_lookup (ssrc->priv->keys, id);
    
    if (prev != NULL) {
        combine_keys (ssrc, prev, key);
        seahorse_key_changed (prev, SKEY_CHANGE_UIDS);
        return;    
    }

    /* A public key */
    skey = seahorse_key_new (SEAHORSE_KEY_SOURCE (ssrc), key);

    /* Add to lookups */ 
    g_hash_table_replace (ssrc->priv->keys, (gpointer)id, skey);     

    /* This stuff is 'undone' in release_key */
    g_object_ref (skey);
    g_signal_connect_after (skey, "destroy", G_CALLBACK (key_destroyed), ssrc);            
    
    /* notify observers */
    seahorse_key_source_added (SEAHORSE_KEY_SOURCE (ssrc), skey);
}
 
/* Callback for copying our internal key table to a list */
static void
keys_to_list (const gchar *id, SeahorseKey *skey, GList **l)
{
    *l = g_list_append (*l, skey);
}

void
seahorse_server_source_set_operation (SeahorseServerSource *ssrc, SeahorseOperation *op)
{
    g_return_if_fail (SEAHORSE_IS_SERVER_SOURCE (ssrc));
    g_return_if_fail (SEAHORSE_IS_OPERATION (op));
    
    if(ssrc->priv->operation)
        g_object_unref (ssrc->priv->operation);
    g_object_ref (op);
    ssrc->priv->operation = op;
}

/* --------------------------------------------------------------------------
 * METHODS
 */

static SeahorseOperation*
seahorse_server_source_refresh (SeahorseKeySource *src, const gchar *key)
{
    SeahorseServerSource *ssrc;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (src), NULL);
    ssrc = SEAHORSE_SERVER_SOURCE (src);

    if (g_str_equal (key, SEAHORSE_KEY_SOURCE_ALL)) {
        /* Stop all operations */
        seahorse_server_source_stop (src);

        /* Remove all keys */    
        g_hash_table_foreach_remove (ssrc->priv->keys, (GHRFunc)release_key_notify, ssrc);
    }
    
    /* We should never be called directly */
    return NULL;
}

static void
seahorse_server_source_stop (SeahorseKeySource *src)
{
    SeahorseServerSource *ssrc;
    
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (src));
    ssrc = SEAHORSE_SERVER_SOURCE (src);

    if(!seahorse_operation_is_done(ssrc->priv->operation))
        seahorse_operation_cancel (ssrc->priv->operation);
}

static guint
seahorse_server_source_get_count (SeahorseKeySource *src,
                                  gboolean secret_only)
{
    SeahorseServerSource *ssrc;
    guint n = 0;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (src), 0);
    ssrc = SEAHORSE_SERVER_SOURCE (src);

    /* Note that we don't deal with secret keys */    
    if (!secret_only)
        n = g_hash_table_size (ssrc->priv->keys);

    return n;
}

SeahorseKey*        
seahorse_server_source_get_key (SeahorseKeySource *src, const gchar *fpr)
{
    SeahorseServerSource *ssrc;

    g_return_val_if_fail (fpr != NULL && fpr[0] != 0, NULL);
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (src), NULL);
    ssrc = SEAHORSE_SERVER_SOURCE (src);
    
    return g_hash_table_lookup (ssrc->priv->keys, fpr);
}

static GList*
seahorse_server_source_get_keys (SeahorseKeySource *src,
                                 gboolean secret_only)  
{
    SeahorseServerSource *ssrc;
    GList *l = NULL;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (src), NULL);
    ssrc = SEAHORSE_SERVER_SOURCE (src);
    
    /* We never deal with secret keys */
    if (!secret_only)
        g_hash_table_foreach (ssrc->priv->keys, (GHFunc)keys_to_list, &l);
        
    return l;
}

static guint
seahorse_server_source_get_state (SeahorseKeySource *src)
{
    SeahorseServerSource *ssrc;
    
    g_return_val_if_fail (SEAHORSE_IS_SERVER_SOURCE (src), 0);
    ssrc = SEAHORSE_SERVER_SOURCE (src);
    
    guint state = SEAHORSE_KEY_SOURCE_REMOTE;
    if (!seahorse_operation_is_done (ssrc->priv->operation))
        state |= SEAHORSE_KEY_SOURCE_LOADING;
    return state;
}

SeahorseOperation*        
seahorse_server_source_get_operation (SeahorseKeySource *sksrc)
{
    SeahorseServerSource *ssrc;
    
    g_return_val_if_fail (SEAHORSE_IS_SERVER_SOURCE (sksrc), NULL);
    ssrc = SEAHORSE_SERVER_SOURCE (sksrc);
    
    g_object_ref (ssrc->priv->operation);
    return ssrc->priv->operation;
}

static gpgme_ctx_t  
seahorse_server_source_new_context (SeahorseKeySource *src)
{
    SeahorseServerSource *ssrc;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (src), 0);
    ssrc = SEAHORSE_SERVER_SOURCE (src);

    /* We generate contexts from our local source */
    return seahorse_key_source_new_context (ssrc->priv->local);
}

/* Code adapted from GnuPG (file g10/keyserver.c) */
static gboolean 
parse_keyserver_uri (char *uri, const char **scheme, const char **host)
{
    int assume_ldap = 0;

    g_return_val_if_fail (uri != NULL, FALSE);
    g_return_val_if_fail (scheme != NULL && host != NULL, FALSE);

    *scheme = NULL;
    *host = NULL;

    /* Get the scheme */

    *scheme = strsep(&uri, ":");
    if (uri == NULL) {
        /* Assume LDAP if there is no scheme */
        assume_ldap = 1;
        uri = (char*)*scheme;
        *scheme = "ldap";
    }

    if (assume_ldap || (uri[0] == '/' && uri[1] == '/')) {
        /* Two slashes means network path. */

        /* Skip over the "//", if any */
        if (!assume_ldap)
            uri += 2;

        /* Get the host */
        *host = strsep (&uri, "/");
        if (*host[0] == '\0')
            return FALSE;
    }

    if (*scheme[0] == '\0')
        return FALSE;

    return TRUE;
}

SeahorseServerSource* 
seahorse_server_source_new (SeahorseKeySource *locsrc, const gchar *server,
                            const gchar *pattern)
{
    SeahorseServerSource *ssrc = NULL;
    const gchar *scheme;
    const gchar *host;
    gchar *uri;
    
    g_return_val_if_fail (server && server[0], NULL);

    if (!pattern || !pattern[0])
        pattern = "invalid-key-pattern-51109ebe-b276-4b1c-84b6-64586e603e68";
    
    uri = g_strdup (server);
        
    if (!parse_keyserver_uri (uri, &scheme, &host)) {
        g_warning ("invalid uri passed: %s", server);
    } else {
        
#ifdef WITH_LDAP        
        if (g_ascii_strcasecmp (scheme, "ldap") == 0) 
            ssrc = SEAHORSE_SERVER_SOURCE (seahorse_ldap_source_new (locsrc, host, pattern));
        else
#endif /* WITH_LDAP */
        
#ifdef WITH_HKP
        if (g_ascii_strcasecmp (scheme, "hkp") == 0 || 
            g_ascii_strcasecmp (scheme, "http") == 0 ||
            g_ascii_strcasecmp (scheme, "https") == 0)
            ssrc = SEAHORSE_SERVER_SOURCE (seahorse_hkp_source_new (locsrc, host, pattern));
        else
#endif /* WITH_HKP */
        
            g_warning ("unsupported keyserver uri scheme: %s", scheme);
    }
    
    g_free (uri);
    return ssrc;
}

GSList*
seahorse_server_source_get_types()
{
    GSList *types = NULL;
#ifdef WITH_LDAP
    types = g_slist_prepend(types, g_strdup("ldap"));
#endif
#ifdef WITH_HKP
    types = g_slist_prepend(types, g_strdup("hkp"));
#endif
    return types;
}

GSList*
seahorse_server_source_get_descriptions()
{
    GSList *descriptions = NULL;
#ifdef WITH_LDAP
    descriptions = g_slist_prepend(descriptions, g_strdup(_("LDAP Key Server")));
#endif
#ifdef WITH_HKP
    descriptions = g_slist_prepend(descriptions, g_strdup(_("HTTP Key Server")));
#endif
    return descriptions;
}

gboolean
seahorse_server_source_valid_uri (const gchar *uri)
{
#ifdef WITH_LDAP
    if (seahorse_ldap_is_valid_uri (uri))
        return TRUE;
#endif
#ifdef WITH_HKP
    if (seahorse_hkp_is_valid_uri (uri))
        return TRUE;
#endif
    return FALSE;
}

GSList*		
seahorse_server_source_purge_keyservers	(GSList *keyservers)
{
	GSList *l;
	gchar *t;
	
	for (l = keyservers; l; l = g_slist_next (l)) {
		t = strchr ((gchar*)l->data, ' ');
		if (t != NULL)
			*t = 0;
	}
	
	return keyservers;
}
