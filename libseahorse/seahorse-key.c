/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#include <gnome.h>

#include "seahorse-key.h"
#include "seahorse-key-source.h"

gboolean 
seahorse_key_predicate_match (SeahorseKeyPredicate *kl, SeahorseKey *skey)
{
    /* Now go for all the fields */
    if (kl->ktype && (kl->ktype != skey->ktype))
        return FALSE;
    if (kl->keyid && (kl->keyid != skey->keyid))
        return FALSE;
    if (kl->location && (kl->location != skey->location))
        return FALSE;
    if (kl->etype && (kl->etype != skey->etype)) 
        return FALSE;
    if (kl->flags && !(kl->flags & skey->flags))
        return FALSE;
    if (kl->nflags && (kl->nflags & skey->flags))
        return FALSE;
    if (kl->sksrc && (kl->sksrc != skey->sksrc))
        return FALSE;
    /* Any custom stuff last */
    if (kl->custom && !((kl->custom)(skey, kl->custom_data)))
        return FALSE;
    return TRUE;
}

enum {
    PROP_0,
    PROP_KEY_SOURCE,
    PROP_KEY_ID,
    PROP_KTYPE,
    PROP_ETYPE,
    PROP_FLAGS,
    PROP_LOCATION,
    PROP_LOADED
};

/* Special fields */
enum {
    PROP_SPECIAL_DISPLAY_NAME,
    PROP_SPECIAL_CN,
    PROP_SPECIAL_MAX
};

GQuark special_properties[PROP_SPECIAL_MAX] = { 0 };

enum {
    CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (SeahorseKey, seahorse_key, GTK_TYPE_OBJECT);

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_key_init (SeahorseKey *skey)
{
    
}

static void
seahorse_key_set_property (GObject *object, guint prop_id, const GValue *value, 
                           GParamSpec *pspec)
{
    SeahorseKey *skey = SEAHORSE_KEY (object);
    
    switch (prop_id) {
    case PROP_KEY_SOURCE:
        g_assert (!skey->sksrc);
        skey->sksrc = g_value_get_object (value);
        g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (skey->sksrc));
        g_object_add_weak_pointer (G_OBJECT (skey->sksrc), (gpointer*)&(skey->sksrc));
        break;
    }
}

static void
seahorse_key_get_property (GObject *object, guint prop_id, GValue *value, 
                           GParamSpec *pspec)
{
    SeahorseKey *skey = SEAHORSE_KEY (object);
    
    switch (prop_id) {
    case PROP_KEY_SOURCE:
        g_value_set_object (value, skey->sksrc);
        break;
    case PROP_KEY_ID:
        g_value_set_uint (value, skey->keyid);
        break;
    case PROP_KTYPE:
        g_value_set_uint (value, skey->ktype);
        break;
    case PROP_ETYPE:
        g_value_set_uint (value, skey->etype);
    case PROP_FLAGS:
        g_value_set_uint (value, skey->flags);
        break;
    case PROP_LOCATION:
        g_value_set_uint (value, skey->location);
        break;
    case PROP_LOADED:
        g_value_set_uint (value, skey->loaded);
        break;
    }
}

static void
seahorse_key_object_finalize (GObject *gobject)
{
    G_OBJECT_CLASS (seahorse_key_parent_class)->finalize (gobject);
}

static void
seahorse_key_class_init (SeahorseKeyClass *klass)
{
    GObjectClass *gobject_class;
    
    seahorse_key_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->finalize = seahorse_key_object_finalize;
    gobject_class->set_property = seahorse_key_set_property;
    gobject_class->get_property = seahorse_key_get_property;
    
    klass->changed = NULL;
    
    g_object_class_install_property (gobject_class, PROP_KEY_SOURCE,
        g_param_spec_object ("key-source", "Key Source", "Key Source that this key belongs to", 
                             SEAHORSE_TYPE_KEY_SOURCE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class, PROP_KEY_ID,
        g_param_spec_uint ("key-id", "Key ID", "Key identifier", 
                           0, G_MAXUINT, SKEY_UNKNOWN, G_PARAM_READABLE));
    
    g_object_class_install_property (gobject_class, PROP_KTYPE,
        g_param_spec_uint ("ktype", "Key Type", "Key type", 
                           0, G_MAXUINT, SKEY_UNKNOWN, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_ETYPE,
        g_param_spec_uint ("etype", "Encrpyption Type", "Key encryption type", 
                           0, G_MAXUINT, SKEY_ETYPE_NONE, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_FLAGS,
        g_param_spec_uint ("flags", "Key Flags", "Flags on capabilities of key. See SeahorseKeyFlags", 
                           0, G_MAXUINT, 0, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_LOCATION,
        g_param_spec_uint ("location", "Key Location", "Where the key is stored. See SeahorseKeyLoc", 
                           0, G_MAXUINT, SKEY_LOC_INVALID, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_LOADED,
        g_param_spec_uint ("loaded", "Loaded Information", "Which parts of the key are loaded. See SeahorseKeyLoaded", 
                           0, G_MAXUINT, SKEY_INFO_NONE, G_PARAM_READABLE));

    signals[CHANGED] = g_signal_new ("changed", G_OBJECT_CLASS_TYPE (gobject_class),
        G_SIGNAL_RUN_LAST,  G_STRUCT_OFFSET (SeahorseKeyClass, changed),
        NULL, NULL, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
        
    /* Some special fields */
    special_properties[PROP_SPECIAL_DISPLAY_NAME] = g_quark_from_static_string ("display-name");
    special_properties[PROP_SPECIAL_CN] = g_quark_from_static_string ("cn");
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

/**
 * seahorse_key_destroy:
 * @skey: #SeahorseKey to destroy
 *
 * Conveniance wrapper for gtk_object_destroy(). Emits destroy signal for @skey.
 **/
void
seahorse_key_destroy (SeahorseKey *skey)
{
    g_return_if_fail (skey != NULL && GTK_IS_OBJECT (skey));
    gtk_object_destroy (GTK_OBJECT (skey));
}

/**
 * seahorse_key_changed:
 * @skey: #SeahorseKey that changed
 * @change: #SeahorseKeyChange type
 *
 * Emits the changed signal for @skey with @change.
 **/
void
seahorse_key_changed (SeahorseKey *skey, SeahorseKeyChange change)
{
    g_return_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey));
    g_signal_emit (G_OBJECT (skey), signals[CHANGED], 0, change);
}

GQuark
seahorse_key_get_keyid (SeahorseKey *skey)
{
    g_return_val_if_fail (skey && SEAHORSE_IS_KEY (skey), 0);
    return skey->keyid;
}    

const gchar*      
seahorse_key_get_short_keyid (SeahorseKey *skey)
{
    const gchar *id;
    guint l;
    
    g_return_val_if_fail (skey && SEAHORSE_IS_KEY (skey), NULL);
    id = g_quark_to_string (skey->keyid);
    
    l = strlen (id);
    if (l > 8)
        return id + l - 8;
    return id;
}


/**
 * seahorse_key_get_source
 * @skey: The #SeahorseKey object
 * 
 * Gets the key source for the given key
 * 
 * Returns: A #SeahorseKeySource
 **/
struct _SeahorseKeySource* 
seahorse_key_get_source  (SeahorseKey *skey)
{
    g_return_val_if_fail (SEAHORSE_IS_KEY (skey), NULL);
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (skey->sksrc), NULL);
    return skey->sksrc;
}

GQuark
seahorse_key_get_ktype (SeahorseKey *skey)
{
    g_return_val_if_fail (SEAHORSE_IS_KEY (skey), SKEY_UNKNOWN);
    return skey->ktype;
}

SeahorseKeyEType
seahorse_key_get_etype (SeahorseKey *skey)
{
    g_return_val_if_fail (SEAHORSE_IS_KEY (skey), SKEY_ETYPE_NONE);
    return skey->etype;
}
/**
 * seahorse_key_get_loaded_info
 * @skey: The #SeahorseKey object
 * 
 * Determine the amount of info loaded in a key
 * 
 * Returns: A SeahorseKeyInfo value which determines what's loaded.
 **/
SeahorseKeyInfo 
seahorse_key_get_loaded (SeahorseKey *skey)
{
    g_return_val_if_fail (SEAHORSE_IS_KEY (skey), SKEY_INFO_NONE);
    return skey->loaded;
}

SeahorseKeyLoc 
seahorse_key_get_location (SeahorseKey *skey)
{
    g_return_val_if_fail (SEAHORSE_IS_KEY (skey), SKEY_LOC_INVALID);
    return skey->location;
}

guint
seahorse_key_get_flags (SeahorseKey *skey)
{
    g_return_val_if_fail (SEAHORSE_IS_KEY (skey), 0);
    return skey->flags;
}

guint
seahorse_key_get_num_names (SeahorseKey *skey)
{
    SeahorseKeyClass *klass;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY (skey), 0);
    klass = SEAHORSE_KEY_GET_CLASS (skey);
    g_return_val_if_fail (klass->get_num_names != NULL, 0);
    
    return (*klass->get_num_names) (skey);
}

gchar*
seahorse_key_get_name (SeahorseKey *skey, guint uid)
{
    SeahorseKeyClass *klass;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY (skey), NULL);
    klass = SEAHORSE_KEY_GET_CLASS (skey);
    g_return_val_if_fail (klass->get_name, NULL);
    
    return (*klass->get_name) (skey, uid);
}

gchar*
seahorse_key_get_name_cn (SeahorseKey *skey, guint uid)
{
    SeahorseKeyClass *klass;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY (skey), NULL);
    klass = SEAHORSE_KEY_GET_CLASS (skey);
    g_return_val_if_fail (klass->get_name_cn, NULL);
    
    return (*klass->get_name_cn) (skey, uid);
}

SeahorseValidity  
seahorse_key_get_name_validity  (SeahorseKey *skey, guint uid)
{
    SeahorseKeyClass *klass;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY (skey), SEAHORSE_VALIDITY_UNKNOWN);
    klass = SEAHORSE_KEY_GET_CLASS (skey);
    g_return_val_if_fail (klass->get_name_validity, SEAHORSE_VALIDITY_UNKNOWN);
    
    return (*klass->get_name_validity) (skey, uid);
}

gchar*
seahorse_key_get_display_name (SeahorseKey *skey)
{
    gchar *name;
    g_object_get (skey, "display-name", &name, NULL);
    return name;    
}

gchar*
seahorse_key_get_simple_name (SeahorseKey *skey)
{
    gchar *name;
    g_object_get (skey, "simple-name", &name, NULL);
    return name;
}

gchar*
seahorse_key_get_fingerprint (SeahorseKey *skey)
{
    gchar *fpr;
    g_object_get (skey, "fingerprint", &fpr, NULL);
    return fpr;
}

SeahorseValidity
seahorse_key_get_validity (SeahorseKey *skey)
{
    guint validity;
    g_object_get (skey, "validity", &validity, NULL);
    return validity;
}

SeahorseValidity
seahorse_key_get_trust (SeahorseKey *skey)
{
    guint validity;
    g_object_get (skey, "trust", &validity, NULL);
    return validity;
}

gulong
seahorse_key_get_expires (SeahorseKey *skey)
{
    gulong expires;
    g_object_get (skey, "expires", &expires, NULL);
    return expires;
}

guint
seahorse_key_get_length (SeahorseKey *skey)
{
    guint length;
    g_object_get (skey, "length", &length, NULL);
    return length;
}

const gchar*
seahorse_key_get_stock_id (SeahorseKey *skey)
{
    const gchar* stock_id;
    g_object_get (skey, "stock-id", &stock_id, NULL);
    return stock_id;
}

void
seahorse_key_set_preferred (SeahorseKey *skey, SeahorseKey *preferred)
{
    /* 
     * We don't ref, because the SeahorseContext takes care of 
     * keeping things sane. 
     */
    
    if (preferred != skey->preferred) {
        skey->preferred = preferred;
        seahorse_key_changed (skey, SKEY_CHANGE_PREFERRED);
    }
}


gboolean
seahorse_key_lookup_property (SeahorseKey *skey, guint uid, const gchar *field, GValue *value)
{
    GParamSpec *spec;
    GQuark qfield;
    gchar *name;
    
    /* Special UID fields */
    if (uid > 0) {

        qfield = g_quark_from_string (field);
        if (qfield == special_properties[PROP_SPECIAL_DISPLAY_NAME]) {
            name = seahorse_key_get_name (skey, uid);
            g_value_init (value, G_TYPE_STRING);
            g_value_take_string (value, name ? name : g_strdup (""));
            return TRUE;

        } else if (qfield == special_properties[PROP_SPECIAL_CN]) {
            name = seahorse_key_get_name_cn (skey, uid);
            g_value_init (value, G_TYPE_STRING);
            g_value_take_string (value, name ? name : g_strdup (""));
            return TRUE;
        }
    }

    spec = g_object_class_find_property (G_OBJECT_GET_CLASS (skey), field);
    if (!spec) 
        return FALSE;

    g_value_init (value, spec->value_type);
    g_object_get_property (G_OBJECT (skey), field, value);
    return TRUE; 
}

const gchar* 
seahorse_key_get_rawid (GQuark keyid)
{
    const gchar* id = g_quark_to_string (keyid);
    const gchar* t = strchr (id, ':');
    return t ? t + 1 : id;
}
