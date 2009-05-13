/* 
 * Seahorse
 * 
 * Copyright (C) 2005 Stefan Walter
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

#include "cryptui-keyset.h"
#include "cryptui-marshal.h"

#include <dbus/dbus-glib-bindings.h>

#define TYPE_G_STRING_VALUE_HASHTABLE \
    (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE))

enum {
    PROP_0,
    PROP_KEYTYPE,
    PROP_EXPAND_KEYS
};

enum {
    ADDED,
    REMOVED,
    CHANGED,
    LAST_SIGNAL
};

/* TODO: Make these cached key properties be customizeable */
static const gchar* cached_key_props[] = {
    "display-name",
    "display-id",
    "enc-type", 
    "flags",
    NULL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _CryptUIKeysetPrivate {
    GHashTable *keys;
    GHashTable *key_props;
    gchar *keytype;
    DBusGProxy *remote_keyset;
    DBusGProxy *remote_service;
    gboolean expand_keys;
};

G_DEFINE_TYPE (CryptUIKeyset, cryptui_keyset, G_TYPE_OBJECT);

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static gboolean
remove_update (const gchar *key, gpointer closure, CryptUIKeyset *keyset)
{
    if (closure == GINT_TO_POINTER (TRUE))
        closure = NULL;
    
    g_signal_emit (keyset, signals[REMOVED], 0, key, closure);     
    return TRUE;
}

static void
remove_key  (const gchar *key, gpointer closure, CryptUIKeyset *keyset)
{
    /* This gets freed when removed from hashtable... */
    gchar *k = g_strdup (key);
    
    if (!closure)
        closure = g_hash_table_lookup (keyset->priv->keys, k);
    
    g_hash_table_remove (keyset->priv->keys, k);
    remove_update (k, closure, keyset);
    
    g_free (k);
}

static void
key_added (DBusGProxy *proxy, const char *key, CryptUIKeyset *keyset)
{
    gchar *k = NULL;
    
    if (!keyset->priv->expand_keys)
        key = k = cryptui_key_get_base (key);
        
    if (!g_hash_table_lookup (keyset->priv->keys, key)) {
        g_hash_table_replace (keyset->priv->keys, g_strdup (key), GINT_TO_POINTER (TRUE));
        g_signal_emit (keyset, signals[ADDED], 0, key);
    }
    
    g_free (k);
}

static void 
key_removed (DBusGProxy *proxy, const char *key, CryptUIKeyset *keyset)
{
    gchar *k = NULL;
    
    if (!keyset->priv->expand_keys)
        key = k = cryptui_key_get_base (key);

    if (g_hash_table_lookup (keyset->priv->keys, key)) {
        
        /* Remove all cached properties for this key */
        g_hash_table_remove (keyset->priv->key_props, key);
        
        remove_key (key, NULL, keyset);
    }
    
    g_free (k);
}

static void
key_changed (DBusGProxy *proxy, const char *key, CryptUIKeyset *keyset)
{
    gpointer closure;
    gchar *k = NULL;
    
    if (!keyset->priv->expand_keys)
        key = k = cryptui_key_get_base (key);
    
    /* Remove all cached properties for this key */
    g_hash_table_remove (keyset->priv->key_props, key);

    closure = g_hash_table_lookup (keyset->priv->keys, key);
    if (closure == GINT_TO_POINTER (TRUE))
        closure = NULL;
    g_signal_emit (keyset, signals[CHANGED], 0, key, closure);
    
    g_free (k);
}

static void
keys_to_list (const gchar *key, gpointer *c, GList **l)
{
    *l = g_list_append (*l, (gpointer)key);
}

static void
keys_to_hash (const gchar *key, gpointer *c, GHashTable *ht)
{
    g_hash_table_replace (ht, (gpointer)key, NULL);
}

static GValue*
lookup_key_property (CryptUIKeyset *keyset, const gchar *key, const gchar *prop,
                     gboolean *allocated)
{
    GHashTable *cached_props;
    GError *error = NULL;
    GValue *value;
    
    *allocated = FALSE;
    
    cached_props = g_hash_table_lookup (keyset->priv->key_props, key);
    if (cached_props) {
        value = g_hash_table_lookup (cached_props, prop);
        if (value)
            return value;
    }
    
    value = g_new0 (GValue, 1);
    if (!dbus_g_proxy_call (keyset->priv->remote_keyset, "GetKeyField", &error,
                            G_TYPE_STRING, key, G_TYPE_STRING, prop, G_TYPE_INVALID, 
                            G_TYPE_BOOLEAN, allocated, G_TYPE_VALUE, value, G_TYPE_INVALID)) {
        g_warning ("dbus call to get '%s' failed: %s", prop, error ? error->message : "");
        g_clear_error (&error);
        g_free (value);
        return NULL;
    }

    return value;
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
cryptui_keyset_init (CryptUIKeyset *keyset)
{
    /* init private vars */
    keyset->priv = g_new0 (CryptUIKeysetPrivate, 1);
    
    /* Map of keyid to closure */
    keyset->priv->keys = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                g_free, NULL);
    
    /* Map of keyid to cached key properties */
    keyset->priv->key_props = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                g_free, (GDestroyNotify)g_hash_table_destroy);

    /* The DBUS connection gets initialized in the constructor */
}

static GObject*  
cryptui_keyset_constructor (GType type, guint n_props, GObjectConstructParam* props)
{
    DBusGConnection *bus;
    CryptUIKeyset *keyset;
    GError *error = NULL;
    GObject *obj;
    gchar *path;
    
    obj = G_OBJECT_CLASS (cryptui_keyset_parent_class)->constructor (type, n_props, props);
    keyset = CRYPTUI_KEYSET (obj);

    if (!keyset->priv->keytype) {
        g_warning ("no keytype was set on the keyset");
        goto finally;
    }
    
    bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
    if (!bus) {
        g_critical ("couldn't get the session bus: %s", error->message);
        g_clear_error (&error);
        goto finally;
    }
    
    path = g_strdup_printf("/org/gnome/seahorse/keys/%s", keyset->priv->keytype);

    keyset->priv->remote_keyset = dbus_g_proxy_new_for_name (bus,
                    "org.gnome.seahorse", path, "org.gnome.seahorse.Keys");
    keyset->priv->remote_service = dbus_g_proxy_new_for_name (bus,
                    "org.gnome.seahorse", "/org/gnome/seahorse/keys", "org.gnome.seahorse.KeyService");
    
    g_free (path);
            
    if (!keyset->priv->remote_keyset || !keyset->priv->remote_service) {
        g_critical ("couldn't connect to the dbus service");
        goto finally;
    }
    
    cryptui_keyset_refresh (keyset);

    dbus_g_proxy_add_signal (keyset->priv->remote_keyset, "KeyAdded", 
                             G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (keyset->priv->remote_keyset, "KeyRemoved", 
                             G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (keyset->priv->remote_keyset, "KeyChanged", 
                             G_TYPE_STRING, G_TYPE_INVALID);
    
    dbus_g_proxy_connect_signal (keyset->priv->remote_keyset, "KeyAdded", 
                                 G_CALLBACK (key_added), keyset, NULL);
    dbus_g_proxy_connect_signal (keyset->priv->remote_keyset, "KeyRemoved", 
                                 G_CALLBACK (key_removed), keyset, NULL);
    dbus_g_proxy_connect_signal (keyset->priv->remote_keyset, "KeyChanged", 
                                 G_CALLBACK (key_changed), keyset, NULL);

finally:    
    return obj;
}

static void
cryptui_keyset_set_property (GObject *object, guint prop_id, const GValue *value, 
                              GParamSpec *pspec)
{
    CryptUIKeyset *keyset = CRYPTUI_KEYSET (object);

    switch (prop_id) {
    case PROP_KEYTYPE:
        g_assert (keyset->priv->keytype == NULL);
        keyset->priv->keytype = g_strdup (g_value_get_string (value));
        break;
    case PROP_EXPAND_KEYS:
        keyset->priv->expand_keys = g_value_get_boolean (value);
        cryptui_keyset_refresh (keyset);
    };
}

static void
cryptui_keyset_get_property (GObject *object, guint prop_id, GValue *value, 
                              GParamSpec *pspec)
{
    CryptUIKeyset *keyset = CRYPTUI_KEYSET (object);

    switch (prop_id) {
    case PROP_KEYTYPE:
        g_value_set_string (value, keyset->priv->keytype);
        break;
    case PROP_EXPAND_KEYS:
        g_value_set_boolean (value, keyset->priv->expand_keys);
        break;
    }
}

static void
cryptui_keyset_dispose (GObject *gobject)
{
    CryptUIKeyset *keyset = CRYPTUI_KEYSET (gobject);
    
    g_hash_table_foreach_remove (keyset->priv->keys, (GHRFunc)remove_update, keyset);
       
    if(keyset->priv->remote_keyset) {
        dbus_g_proxy_disconnect_signal (keyset->priv->remote_keyset, "KeyAdded", 
                                        G_CALLBACK (key_added), keyset);
        dbus_g_proxy_disconnect_signal (keyset->priv->remote_keyset, "KeyRemoved", 
                                        G_CALLBACK (key_removed), keyset);
        dbus_g_proxy_disconnect_signal (keyset->priv->remote_keyset, "KeyChanged", 
                                        G_CALLBACK (key_changed), keyset);
        
        g_object_unref (keyset->priv->remote_keyset);
        keyset->priv->remote_keyset = NULL;
        g_object_unref (keyset->priv->remote_service);
        keyset->priv->remote_service = NULL;
    }
    
    G_OBJECT_CLASS (cryptui_keyset_parent_class)->dispose (gobject);
}

static void
cryptui_keyset_finalize (GObject *gobject)
{
    CryptUIKeyset *keyset = CRYPTUI_KEYSET (gobject);

    g_hash_table_destroy (keyset->priv->keys);
    g_assert (keyset->priv->remote_keyset == NULL);
    g_free (keyset->priv);
    
    G_OBJECT_CLASS (cryptui_keyset_parent_class)->finalize (gobject);
}

static void
cryptui_keyset_class_init (CryptUIKeysetClass *klass)
{
    GObjectClass *gclass;

    cryptui_keyset_parent_class = g_type_class_peek_parent (klass);
    gclass = G_OBJECT_CLASS (klass);

    gclass->constructor = cryptui_keyset_constructor;
    gclass->dispose = cryptui_keyset_dispose;
    gclass->finalize = cryptui_keyset_finalize;
    gclass->set_property = cryptui_keyset_set_property;
    gclass->get_property = cryptui_keyset_get_property;
    
    g_object_class_install_property (gclass, PROP_KEYTYPE,
        g_param_spec_string ("keytype", "Key Type", "Type of keys to be listed", 
                             NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
    g_object_class_install_property (gclass, PROP_EXPAND_KEYS, 
        g_param_spec_boolean ("expand-keys", "Expand Keys", "Expand all names in keys", 
                              TRUE, G_PARAM_READWRITE));
    
    signals[ADDED] = g_signal_new ("added", CRYPTUI_TYPE_KEYSET, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (CryptUIKeysetClass, added),
                NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
    
    signals[REMOVED] = g_signal_new ("removed", CRYPTUI_TYPE_KEYSET, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (CryptUIKeysetClass, removed),
                NULL, NULL, cryptui_marshal_VOID__STRING_POINTER, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_POINTER);    
    
    signals[CHANGED] = g_signal_new ("changed", CRYPTUI_TYPE_KEYSET, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (CryptUIKeysetClass, changed),
                NULL, NULL, cryptui_marshal_VOID__STRING_POINTER, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_POINTER);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

/**
 * cryptui_keyset_new:
 * @keytype: libcryptui key prefix
 * @expand_keys: whether key's non-primary uids are part of the set
 *
 * Creates a new keylist
 *
 * Returns: the new keyset
 */
CryptUIKeyset*
cryptui_keyset_new (const gchar *keytype, gboolean expand_keys)
{
    return g_object_new (CRYPTUI_TYPE_KEYSET, "keytype", keytype, 
                         "expand-keys", expand_keys, NULL);
}

/**
 * cryptui_keyset_get_keytype:
 * @keyset: a libcryptui keyset
 *
 * Gets the libcryptui key prefix for the keyset
 *
 * Returns: a libcryptui key prefix
 */
const gchar*
cryptui_keyset_get_keytype (CryptUIKeyset *keyset)
{
    return keyset->priv->keytype;
}

/**
 * cryptui_keyset_get_keys:
 * @keyset: a libcryptui keyset
 *
 * Gets a list of keys contained in the key set
 *
 * Returns: a doubly linked list of keys
 */
GList*             
cryptui_keyset_get_keys (CryptUIKeyset *keyset)
{
    GList *keys = NULL;
    g_hash_table_foreach (keyset->priv->keys, (GHFunc)keys_to_list, &keys);
    return keys;
}

/**
 * cryptui_keyset_get_count:
 * @keyset: a libcryptui keyset
 *
 * Gets the number of keys stored in the keyset
 *
 * Returns: the number of keys
 */
guint
cryptui_keyset_get_count (CryptUIKeyset *keyset)
{
    return g_hash_table_size (keyset->priv->keys);
}

/**
 * cryptui_keyset_refresh:
 * @keyset: a libcryptui keyset
 *
 * Checks the remote keyset to see which keys have been added or removed
 */
void
cryptui_keyset_refresh (CryptUIKeyset *keyset)
{
    GHashTable *check;
    gchar **k, **keys = NULL; 
    GError *error = NULL;
    
    g_assert (keyset != NULL);
    
    /* Make note of all the keys we had prior to refresh */
    check = g_hash_table_new (g_str_hash, g_str_equal);
    g_hash_table_foreach (keyset->priv->keys, (GHFunc)keys_to_hash, check);

    if (!keyset->priv->remote_keyset)
        goto finally;
        
    if (!dbus_g_proxy_call (keyset->priv->remote_keyset, "ListKeys", &error,
                            G_TYPE_INVALID, G_TYPE_STRV, &keys, G_TYPE_INVALID)) {
        g_warning ("dbus call to list keys failed: %s", error ? error->message : "");
        g_clear_error (&error);
        goto finally;
    }
    
    for (k = keys; *k; k++) {
        /* Make note that we've seen this key */
        g_hash_table_remove (check, *k);

        /* This will add to keyset */
        key_added(NULL, *k, keyset);
    }
    
finally:       
    /* Remove all keys not seen */
    g_hash_table_foreach (check, (GHFunc)remove_key, keyset);
    g_hash_table_destroy (check);
    
    g_strfreev (keys);
}

/**
 * cryptui_keyset_get_closure:
 * @keyset: a libcryptui keyset
 * @key: a libcryptui key
 *
 * TODO: Find out what closure is and document this function
 *
 * Returns: closure associated with key
 */
gpointer
cryptui_keyset_get_closure (CryptUIKeyset *keyset, const gchar *key)
{
    gpointer closure = g_hash_table_lookup (keyset->priv->keys, key);
    g_return_val_if_fail (closure != NULL, NULL);

    /* |TRUE| means no closure has been set */
    if (closure == GINT_TO_POINTER (TRUE))
        return NULL;
    
    return closure;
}

/**
 * cryptui_keyset_set_closure:
 * @keyset: a libcryptui keyset
 * @key: a libcryptui key
 * @closure: TODO
 *
 * TODO: Find out what closure is and document this function
 */
void
cryptui_keyset_set_closure (CryptUIKeyset *keyset, const gchar *key, 
                             gpointer closure)
{
    /* Make sure we have the key */
    g_return_if_fail (g_hash_table_lookup (keyset->priv->keys, key) != NULL);
    
    /* |TRUE| means no closure has been set */
    if (closure == NULL)
        closure = GINT_TO_POINTER (TRUE);

    g_hash_table_insert (keyset->priv->keys, g_strdup (key), closure);    
}

/**
 * cryptui_keyset_get_expand_keys:
 * @keyset: a libcryptui keyset
 *
 * Gets whether or not non-primary key UIDs are included in the keyset
 *
 * Returns: TRUE if non-primary key UIDs are included in the keyset
 */
gboolean
cryptui_keyset_get_expand_keys (CryptUIKeyset *keyset)
{
    gboolean expand;
    g_object_get(keyset, "expand-keys", &expand, NULL);
    return expand;
}

/**
 * cryptui_keyset_set_expand_keys:
 * @keyset: a libcryptui keyset
 * @expand_keys: a gboolean
 *
 * Sets whether or not non-primary key UIDs are included in the keyset
 */
void
cryptui_keyset_set_expand_keys (CryptUIKeyset *keyset, gboolean expand_keys)
{
    g_object_set(keyset, "expand-keys", expand_keys, NULL);
}

/**
 * cryptui_keyset_cache_key:
 * @keyset: a libcryptui keyset
 * @key: libcryptui key to cache
 *
 * Stores the key's fields returned by the DBus method GetKeyFields in the
 * keyset.
 */
void
cryptui_keyset_cache_key (CryptUIKeyset *keyset, const gchar *key)
{
    GError *error = NULL;
    GHashTable *props;
    
    /* We don't recache a key until it's been changed */
    props = (GHashTable*)g_hash_table_lookup (keyset->priv->key_props, key);
    if (props)
        return;

    if (!dbus_g_proxy_call (keyset->priv->remote_keyset, "GetKeyFields", &error,
                            G_TYPE_STRING, key, G_TYPE_STRV, cached_key_props, G_TYPE_INVALID, 
                            TYPE_G_STRING_VALUE_HASHTABLE, &props, G_TYPE_INVALID)) {
        g_warning ("dbus call to cache key failed: %s", error ? error->message : "");
        g_clear_error (&error);
        return;
    }
    
    if (props) 
        g_hash_table_insert (keyset->priv->key_props, g_strdup (key), props);
    else
        g_hash_table_remove (keyset->priv->key_props, key);
}

/**
 * cryptui_keyset_key_get_string:
 * @keyset: a libcryptui keyset
 * @key: libcryptui key to fetch a property of
 * @prop: string property to get
 *
 * Gets the given property of the key in the keyset.
 *
 * Returns: a string containing the property's value
 */
gchar*
cryptui_keyset_key_get_string (CryptUIKeyset *keyset, const gchar *key, 
                               const gchar *prop)
{
    GValue *value;
    gboolean allocated;
    gchar *str;
    
    value = lookup_key_property (keyset, key, prop, &allocated);
    if (!value)
        return NULL;
        
    g_return_val_if_fail (G_VALUE_TYPE (value) == G_TYPE_STRING, NULL);
    str = g_value_dup_string (value);
    
    if (allocated) {
        g_value_unset (value);
        g_free (value);
    }
    
    return str;
}

/**
 * cryptui_keyset_key_get_uint:
 * @keyset: a libcryptui keyset
 * @key: libcryptui key to fetch a property of
 * @prop: uint property to get
 *
 * Gets the given property of the key in the keyset.
 *
 * Returns: a uint containing the property's value
 */
guint
cryptui_keyset_key_get_uint (CryptUIKeyset *keyset, const gchar *key,
                             const gchar *prop)
{
    GValue *value;
    gboolean allocated;
    guint val;
    
    value = lookup_key_property (keyset, key, prop, &allocated);
    if (!value)
        return 0;
        
    g_return_val_if_fail (G_VALUE_TYPE (value) == G_TYPE_UINT, 0);
    val = g_value_get_uint (value);
    
    if (allocated) {
        g_value_unset (value);
        g_free (value);
    }
    
    return val;
}


/**
 * cryptui_keyset_key_display_name:
 * @keyset: a libcryptui keyset
 * @key: a libcryptui key
 *
 * Gets the "display-name" property of the given key
 *
 * Returns: the display name of the key
 */
gchar*
cryptui_keyset_key_display_name (CryptUIKeyset *keyset, const gchar *key)
{
    return cryptui_keyset_key_get_string (keyset, key, "display-name");
}

/**
 * cryptui_keyset_key_display_id:
 * @keyset: a libcryptui keyset
 * @key: a libcryptui key
 *
 * Gets the "display-id" property of the given key
 *
 * Returns: the display id of the key
 */
gchar*
cryptui_keyset_key_display_id (CryptUIKeyset *keyset, const gchar *key)
{
    return cryptui_keyset_key_get_string (keyset, key, "display-id");
}

/**
 * cryptui_keyset_key_flags:
 * @keyset: a libcryptui keyset
 * @key: a libcryptui key
 *
 * Gets the key's flags
 *
 * Returns: a uint containing the flags
 */
guint
cryptui_keyset_key_flags (CryptUIKeyset *keyset, const gchar *key)
{
    return cryptui_keyset_key_get_uint (keyset, key, "flags");
}

/**
 * cryptui_keyset_key_raw_keyid:
 * @keyset: a libcryptui keyset
 * @key: a libcryptui key
 *
 * Gets the key's raw key id
 *
 * Returns: a string with the raw key id
 */
gchar*
cryptui_keyset_key_raw_keyid (CryptUIKeyset *keyset, const gchar *key)
{
    return cryptui_keyset_key_get_string (keyset, key, "raw-id");
}

/**
 * cryptui_keyset_keys_raw_keyids:
 * @keyset: a libcryptui keyset
 * @keys: an array of libcryptui keys
 *
 * Gets the keys' raw key ids
 *
 * Returns: an array of raw key ids
 */
gchar**
cryptui_keyset_keys_raw_keyids (CryptUIKeyset *keyset, const gchar **keys)
{
    guint nkeys;
    const gchar **k;
    gchar **ids;
    int i;
    
    /* TODO: This could get inefficient */
    
    for (k = keys, nkeys = 0; *k; k++)
        nkeys++;
    ids = g_new0 (gchar*, nkeys + 1);
    for (k = keys, i = 0; *k; k++, i++)
        ids[i] = cryptui_keyset_key_get_string (keyset, *k, "raw-id");
    
    return ids;
}

/* -----------------------------------------------------------------------------
 * INTERNAL
 */

const gchar*
_cryptui_keyset_get_internal_keyid (CryptUIKeyset *keyset, const gchar *keyid)
{
    gpointer orig_key, value;
    if (!g_hash_table_lookup_extended (keyset->priv->keys, keyid, &orig_key, &value))
        return NULL;
    return (const gchar*)orig_key;
}
