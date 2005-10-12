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

#include "cryptui-keyset.h"
#include "cryptui-marshal.h"

#include <dbus/dbus-glib-bindings.h>

enum {
	PROP_0,
    PROP_KEYTYPE,
    PROP_ENCTYPE
};

enum {
    ADDED,
    REMOVED,
    CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _CryptUIKeysetPrivate {
    GHashTable *keys;
    gchar *keytype;
    CryptUIEncType enctype;
    DBusGProxy *remote_keyset;
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
    if (!g_hash_table_lookup (keyset->priv->keys, key)) {
        g_hash_table_replace (keyset->priv->keys, g_strdup (key), GINT_TO_POINTER (TRUE));
        g_signal_emit (keyset, signals[ADDED], 0, key);
    }
}

static void 
key_removed (DBusGProxy *proxy, const char *key, CryptUIKeyset *keyset)
{
    if (g_hash_table_lookup (keyset->priv->keys, key))
        remove_key (key, NULL, keyset);
}

static void
key_changed (DBusGProxy *proxy, const char *key, CryptUIKeyset *keyset)
{
    gpointer closure = g_hash_table_lookup (keyset->priv->keys, key);
    if (closure == GINT_TO_POINTER (TRUE))
        closure = NULL;
    g_signal_emit (keyset, signals[CHANGED], 0, key, closure);
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

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
cryptui_keyset_init (CryptUIKeyset *keyset)
{
	/* init private vars */
	keyset->priv = g_new0 (CryptUIKeysetPrivate, 1);
    keyset->priv->keys = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                g_free, NULL);

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
                    "org.gnome.seahorse.KeyService", path, "org.gnome.seahorse.Keys");
    
    g_free (path);
            
    if (!keyset->priv->remote_keyset) {
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
        g_return_if_fail (keyset->priv->keytype == NULL);
        keyset->priv->keytype = g_strdup (g_value_get_string (value));
        break;
    case PROP_ENCTYPE:
        keyset->priv->enctype = g_value_get_uint (value);
        cryptui_keyset_refresh (keyset);
        break;
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
    case PROP_ENCTYPE:
        g_value_set_uint (value, keyset->priv->enctype);
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
    g_object_class_install_property (gclass, PROP_ENCTYPE,
        g_param_spec_uint ("enctype", "Encryption Type", "Type of encryption provided by keys",
                           0, _CRYPTUI_ENCTYPE_MAXVALUE, 0, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
    
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

CryptUIKeyset*
cryptui_keyset_new (const gchar *keytype, CryptUIEncType enctype)
{
    return g_object_new (CRYPTUI_TYPE_KEYSET, "keytype", keytype, 
                         "enctype", enctype, NULL);
}

GList*             
cryptui_keyset_get_keys (CryptUIKeyset *keyset)
{
    GList *keys = NULL;
    g_hash_table_foreach (keyset->priv->keys, (GHFunc)keys_to_list, &keys);
    return keys;
}

guint
cryptui_keyset_get_count (CryptUIKeyset *keyset)
{
    return g_hash_table_size (keyset->priv->keys);
}

void
cryptui_keyset_refresh (CryptUIKeyset *keyset)
{
    GHashTable *check;
    gchar **k, **keys = NULL; 
    GError *error = NULL;
    
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
