/*
 * Seahorse
 *
 * Copyright (C) 2006 Nate Nielsen
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
#include <gnome.h>

#include "seahorse-unknown-key.h"
#include "seahorse-unknown-source.h"

enum {
    PROP_0,
    PROP_KEYID,
    PROP_DISPLAY_NAME,
    PROP_DISPLAY_ID,
    PROP_SIMPLE_NAME,
    PROP_FINGERPRINT,
    PROP_VALIDITY,
    PROP_TRUST,
    PROP_EXPIRES,
    PROP_STOCK_ID
};

G_DEFINE_TYPE (SeahorseUnknownKey, seahorse_unknown_key, SEAHORSE_TYPE_KEY);

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_unknown_key_init (SeahorseUnknownKey *ukey)
{

}

static GObject*  
seahorse_unknown_key_constructor (GType type, guint n_props, GObjectConstructParam* props)
{
    SeahorseKey *skey;
    GObject *obj;
    
    obj = G_OBJECT_CLASS (seahorse_unknown_key_parent_class)->constructor (type, n_props, props);
    skey = SEAHORSE_KEY (obj);
    
    if (skey->sksrc)
        skey->ktype = seahorse_key_source_get_ktype (skey->sksrc);
    skey->location = SKEY_LOC_UNKNOWN;
    skey->loaded = SKEY_INFO_NONE;
    skey->etype = SKEY_ETYPE_NONE;
    return obj;
}

static guint 
seahorse_unknown_key_get_num_names (SeahorseKey *key)
{
    return 1;
}

static gchar* 
seahorse_unknown_key_get_name (SeahorseKey *key, guint index)
{
    SeahorseUnknownKey *ukey;
    
    g_assert (SEAHORSE_IS_UNKNOWN_KEY (key));
    ukey = SEAHORSE_UNKNOWN_KEY (key);
    
    g_return_val_if_fail (index == 0, NULL);

    return g_strdup_printf (_("Unknown Key: %s"), seahorse_key_get_rawid (ukey->keyid));
}

static gchar* 
seahorse_unknown_key_get_name_cn (SeahorseKey *skey, guint index)
{
    g_return_val_if_fail (index == 0, NULL);
    return NULL;
}

static void
seahorse_unknown_key_get_property (GObject *object, guint prop_id,
                                   GValue *value, GParamSpec *pspec)
{
    SeahorseUnknownKey *ukey = SEAHORSE_UNKNOWN_KEY (object);
    
    switch (prop_id) {
    case PROP_DISPLAY_NAME:
        g_value_set_string_take_ownership (value, 
                seahorse_unknown_key_get_name (SEAHORSE_KEY (ukey), 0));
        break;
    case PROP_DISPLAY_ID:
        g_value_set_string (value, seahorse_key_get_rawid (ukey->keyid)); 
        break;
    case PROP_SIMPLE_NAME:
        g_value_set_string (value, _("Unknown Key"));
        break;
    case PROP_FINGERPRINT:
        g_value_set_string (value, NULL);
        break;
    case PROP_VALIDITY:
        g_value_set_uint (value, SEAHORSE_VALIDITY_UNKNOWN);
        break;
    case PROP_TRUST:
        g_value_set_uint (value, SEAHORSE_VALIDITY_UNKNOWN);
        break;
    case PROP_EXPIRES:
        g_value_set_ulong (value, 0);
        break;
    case PROP_STOCK_ID:
        /* We use a pointer so we don't copy the string every time */
        g_value_set_pointer (value, "");
        break;
    }
}

static void
seahorse_unknown_key_set_property (GObject *object, guint prop_id, 
                                   const GValue *value, GParamSpec *pspec)
{
    SeahorseUnknownKey *ukey = SEAHORSE_UNKNOWN_KEY (object);

    switch (prop_id) {
    case PROP_KEYID:
        ukey->keyid = g_value_get_uint (value);
        SEAHORSE_KEY (ukey)->keyid = ukey->keyid;
        break;
    }
}

static void
seahorse_unknown_key_finalize (GObject *gobject)
{
    G_OBJECT_CLASS (seahorse_unknown_key_parent_class)->finalize (gobject);
}

static void
seahorse_unknown_key_class_init (SeahorseUnknownKeyClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    SeahorseKeyClass *key_class = SEAHORSE_KEY_CLASS (klass);
    
    seahorse_unknown_key_parent_class = g_type_class_peek_parent (klass);
    
    gobject_class->constructor = seahorse_unknown_key_constructor;
    gobject_class->finalize = seahorse_unknown_key_finalize;
    gobject_class->set_property = seahorse_unknown_key_set_property;
    gobject_class->get_property = seahorse_unknown_key_get_property;

    key_class->get_num_names = seahorse_unknown_key_get_num_names;
    key_class->get_name = seahorse_unknown_key_get_name;
    key_class->get_name_cn = seahorse_unknown_key_get_name_cn;
    
    g_object_class_install_property (gobject_class, PROP_KEYID,
        g_param_spec_uint ("key-id", "The Key ID", "Key identifier",
                           0, G_MAXUINT, 0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class, PROP_DISPLAY_NAME,
        g_param_spec_string ("display-name", "Display Name", "User Displayable name for this key",
                             "", G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_DISPLAY_ID,
        g_param_spec_string ("display-id", "Display ID", "User Displayable id for this key",
                             "", G_PARAM_READABLE));
                      
    g_object_class_install_property (gobject_class, PROP_SIMPLE_NAME,
        g_param_spec_string ("simple-name", "Simple Name", "Simple name for this key",
                             "", G_PARAM_READABLE));
                      
    g_object_class_install_property (gobject_class, PROP_FINGERPRINT,
        g_param_spec_string ("fingerprint", "Fingerprint", "Unique fingerprint for this key",
                             "", G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_VALIDITY,
        g_param_spec_uint ("validity", "Validity", "Validity of this key",
                           0, G_MAXUINT, 0, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_TRUST,
        g_param_spec_uint ("trust", "Trust", "Trust in this key",
                           0, G_MAXUINT, 0, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_EXPIRES,
        g_param_spec_ulong ("expires", "Expires On", "Date this key expires on",
                           0, G_MAXULONG, 0, G_PARAM_READABLE));
                           
    g_object_class_install_property (gobject_class, PROP_STOCK_ID,
        g_param_spec_pointer ("stock-id", "The stock icon", "The stock icon id",
                              G_PARAM_READABLE));

}

/* -----------------------------------------------------------------------------
 * PUBLIC METHODS
 */

SeahorseUnknownKey* 
seahorse_unknown_key_new (SeahorseUnknownSource *sksrc, GQuark keyid)
{
    SeahorseUnknownKey *ukey;
    
    ukey = g_object_new (SEAHORSE_TYPE_UNKNOWN_KEY, "key-source", sksrc, 
                         "key-id", keyid, NULL);
    
    /* We don't care about this floating business */
    g_object_ref (GTK_OBJECT (ukey));
    gtk_object_sink (GTK_OBJECT (ukey));
    return ukey;
}
