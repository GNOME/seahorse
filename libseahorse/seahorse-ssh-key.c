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

#include "config.h"

#include <gnome.h>
#include <string.h>

#include "seahorse-context.h"
#include "seahorse-key-source.h"
#include "seahorse-ssh-source.h"
#include "seahorse-ssh-key.h"
#include "seahorse-ssh-operation.h"
#include "seahorse-gtkstock.h"

enum {
    PROP_0,
    PROP_KEY_DATA,
    PROP_DISPLAY_NAME,
    PROP_DISPLAY_ID,
    PROP_SIMPLE_NAME,
    PROP_FINGERPRINT,
    PROP_VALIDITY,
    PROP_TRUST,
    PROP_EXPIRES,
    PROP_LENGTH,
    PROP_STOCK_ID
};

struct _SeahorseSSHKeyPrivate {
    gchar *displayname;
    gchar *simplename;
};

G_DEFINE_TYPE (SeahorseSSHKey, seahorse_ssh_key, SEAHORSE_TYPE_KEY);

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static gchar*
parse_first_word (const gchar *line) 
{
    int pos;
    
    #define PARSE_CHARS "\t \n@;,.\\?()[]{}+/"
    line += strspn (line, PARSE_CHARS);
    pos = strcspn (line, PARSE_CHARS);
    return pos == 0 ? NULL : g_strndup (line, pos);
}

static void
changed_key (SeahorseSSHKey *skey)
{
    SeahorseKey *key = SEAHORSE_KEY (skey);

    g_free (skey->priv->displayname);
    skey->priv->displayname = NULL;
    
    g_free (skey->priv->simplename);
    skey->priv->simplename = NULL;
    
    if (skey->keydata) {

        /* Try to make display and simple names */
        if (skey->keydata->comment) {
            skey->priv->displayname = g_strdup (skey->keydata->comment);
            skey->priv->simplename = parse_first_word (skey->keydata->comment);
            
        /* No names when not even the fingerpint loaded */
        } else if (!skey->keydata->fingerprint) {
            skey->priv->displayname = g_strdup (_("(Unreadable SSH Key)"));
    
        /* No comment, but loaded */        
        } else {
            skey->priv->displayname = g_strdup (_("SSH Key"));
        }
    
        if (skey->priv->simplename == NULL)
            skey->priv->simplename = g_strdup (_("SSH Key"));
        
    }
    
    /* Now start setting the main SeahorseKey fields */
    key->ktype = SKEY_SSH;
    key->keyid = 0;
    
    if (!skey->keydata || !skey->keydata->fingerprint) {
        
        key->location = SKEY_LOC_INVALID;
        key->etype = SKEY_ETYPE_NONE;
        key->loaded = SKEY_INFO_NONE;
        key->flags = SKEY_FLAG_DISABLED;
        
    } else {
    
        /* The key id */
        key->keyid = seahorse_ssh_key_get_cannonical_id (skey->keydata->fingerprint);
        key->location = SKEY_LOC_LOCAL;
        key->loaded = SKEY_INFO_COMPLETE;
        key->flags = skey->keydata->authorized ? SKEY_FLAG_TRUSTED : 0;
        
        if (skey->keydata->privfile) {
            key->etype = SKEY_PRIVATE;
            key->keydesc = _("Private SSH Key");
        } else {
            key->etype = SKEY_PUBLIC;
            key->keydesc = _("Public SSH Key");
        }
    }
    
    if (!key->keyid)
        key->keyid = g_quark_from_string (SKEY_SSH_STR ":UNKNOWN ");
    
    seahorse_key_changed (key, SKEY_CHANGE_ALL);
}

static guint 
calc_validity (SeahorseSSHKey *skey)
{
    if (skey->keydata->privfile)
        return SEAHORSE_VALIDITY_ULTIMATE;
    return 0;
}

static guint
calc_trust (SeahorseSSHKey *skey)
{
    if (skey->keydata->authorized)
        return SEAHORSE_VALIDITY_FULL;
    return 0;
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static guint 
seahorse_ssh_key_get_num_names (SeahorseKey *key)
{
    return 1;
}

static gchar* 
seahorse_ssh_key_get_name (SeahorseKey *key, guint index)
{
    SeahorseSSHKey *skey;
    
    g_assert (SEAHORSE_IS_SSH_KEY (key));
    skey = SEAHORSE_SSH_KEY (key);
    
    g_assert (index == 0);

    return g_strdup (skey->priv->displayname);
}

static gchar* 
seahorse_ssh_key_get_name_cn (SeahorseKey *skey, guint index)
{
    g_assert (index != 0);
    return NULL;
}

static SeahorseValidity  
seahorse_ssh_key_get_name_validity  (SeahorseKey *skey, guint index)
{
    g_return_val_if_fail (index == 0, SEAHORSE_VALIDITY_UNKNOWN);
    return calc_validity (SEAHORSE_SSH_KEY (skey));
}

static void
seahorse_ssh_key_get_property (GObject *object, guint prop_id,
                               GValue *value, GParamSpec *pspec)
{
    SeahorseSSHKey *skey = SEAHORSE_SSH_KEY (object);
    
    switch (prop_id) {
    case PROP_KEY_DATA:
        g_value_set_pointer (value, skey->keydata);
        break;
    case PROP_DISPLAY_NAME:
        g_value_set_string (value, skey->priv->displayname);
        break;
    case PROP_DISPLAY_ID:
        g_value_set_string (value, seahorse_key_get_short_keyid (SEAHORSE_KEY (skey))); 
        break;
    case PROP_SIMPLE_NAME:        
        g_value_set_string (value, skey->priv->simplename);
        break;
    case PROP_FINGERPRINT:
        g_value_set_string (value, skey->keydata ? skey->keydata->fingerprint : NULL);
        break;
    case PROP_VALIDITY:
        g_value_set_uint (value, calc_validity (skey));
        break;
    case PROP_TRUST:
        g_value_set_uint (value, calc_trust (skey));
        break;
    case PROP_EXPIRES:
        g_value_set_ulong (value, 0);
        break;
    case PROP_LENGTH:
        g_value_set_uint (value, skey->keydata ? skey->keydata->length : 0);
        break;
    case PROP_STOCK_ID:
        /* We use a pointer so we don't copy the string every time */
        g_value_set_pointer (value, SEAHORSE_STOCK_KEY_SSH);
        break;
    }
}

static void
seahorse_ssh_key_set_property (GObject *object, guint prop_id, 
                               const GValue *value, GParamSpec *pspec)
{
    SeahorseSSHKey *skey = SEAHORSE_SSH_KEY (object);
    SeahorseSSHKeyData *keydata;
    
    switch (prop_id) {
    case PROP_KEY_DATA:
        keydata = (SeahorseSSHKeyData*)g_value_get_pointer (value);
        if (skey->keydata != keydata) {
            seahorse_ssh_key_data_free (skey->keydata);
            skey->keydata = keydata;
        }
        changed_key (skey);
        break;
    }
}

/* Unrefs gpgme key and frees data */
static void
seahorse_ssh_key_finalize (GObject *gobject)
{
    SeahorseSSHKey *skey = SEAHORSE_SSH_KEY (gobject);
    
    g_free (skey->priv->displayname);
    g_free (skey->priv->simplename);
    g_free (skey->priv);
    skey->priv = NULL;
    
    seahorse_ssh_key_data_free (skey->keydata);
    
    G_OBJECT_CLASS (seahorse_ssh_key_parent_class)->finalize (gobject);
}

static void
seahorse_ssh_key_init (SeahorseSSHKey *skey)
{
    /* init private vars */
    skey->priv = g_new0 (SeahorseSSHKeyPrivate, 1);
}

static void
seahorse_ssh_key_class_init (SeahorseSSHKeyClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    SeahorseKeyClass *key_class = SEAHORSE_KEY_CLASS (klass);
    
    seahorse_ssh_key_parent_class = g_type_class_peek_parent (klass);
    
    gobject_class->finalize = seahorse_ssh_key_finalize;
    gobject_class->set_property = seahorse_ssh_key_set_property;
    gobject_class->get_property = seahorse_ssh_key_get_property;

    key_class->get_num_names = seahorse_ssh_key_get_num_names;
    key_class->get_name = seahorse_ssh_key_get_name;
    key_class->get_name_cn = seahorse_ssh_key_get_name_cn;
    key_class->get_name_validity = seahorse_ssh_key_get_name_validity;
    
    g_object_class_install_property (gobject_class, PROP_KEY_DATA,
        g_param_spec_pointer ("key-data", "SSH Key Data", "SSH key data pointer",
                              G_PARAM_READWRITE));

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

    g_object_class_install_property (gobject_class, PROP_LENGTH,
        g_param_spec_uint ("length", "Length of", "The length of this key",
                           0, G_MAXUINT, 0, G_PARAM_READABLE));
                           
    g_object_class_install_property (gobject_class, PROP_STOCK_ID,
        g_param_spec_pointer ("stock-id", "The stock icon", "The stock icon id",
                              G_PARAM_READABLE));
}

/* -----------------------------------------------------------------------------
 * PUBLIC METHODS
 */

SeahorseSSHKey* 
seahorse_ssh_key_new (SeahorseKeySource *sksrc, SeahorseSSHKeyData *data)
{
    SeahorseSSHKey *skey;
    skey = g_object_new (SEAHORSE_TYPE_SSH_KEY, "key-source", sksrc, 
                         "key-data", data, NULL);
    
    /* We don't care about this floating business */
    g_object_ref (GTK_OBJECT (skey));
    gtk_object_sink (GTK_OBJECT (skey));
    return skey;
}

guint 
seahorse_ssh_key_get_algo (SeahorseSSHKey *skey)
{
    g_return_val_if_fail (SEAHORSE_IS_SSH_KEY (skey), SSH_ALGO_UNK);
    return skey->keydata->algo;
}

const gchar*
seahorse_ssh_key_get_algo_str (SeahorseSSHKey *skey)
{
    g_return_val_if_fail (SEAHORSE_IS_SSH_KEY (skey), "");
    
    switch(skey->keydata->algo) {
    case SSH_ALGO_UNK:
        return "";
    case SSH_ALGO_RSA:
        return "RSA";
    case SSH_ALGO_DSA:
        return "DSA";
    default:
        g_assert_not_reached ();
        return NULL;
    };
}

guint           
seahorse_ssh_key_get_strength (SeahorseSSHKey *skey)
{
    g_return_val_if_fail (SEAHORSE_IS_SSH_KEY (skey), 0);
    return skey->keydata ? skey->keydata->length : 0;
}

const gchar*    
seahorse_ssh_key_get_location (SeahorseSSHKey *skey)
{
    g_return_val_if_fail (SEAHORSE_IS_SSH_KEY (skey), NULL);
    if (!skey->keydata)
        return NULL;
    return skey->keydata->privfile ? 
                    skey->keydata->privfile : skey->keydata->pubfile;
}

GQuark
seahorse_ssh_key_get_cannonical_id (const gchar *id)
{
    #define SSH_ID_SIZE 16
    gchar *hex, *canonical_id = g_malloc0 (SSH_ID_SIZE + 1);
    gint i, off, len = strlen (id);
    GQuark ret;

    /* Strip out all non alpha numeric chars and limit length to SSH_ID_SIZE */
    for (i = len, off = SSH_ID_SIZE; i >= 0 && off > 0; --i) {
         if (g_ascii_isalnum (id[i]))
             canonical_id[--off] = g_ascii_toupper (id[i]);
    }
    
    /* Not enough characters */
    g_return_val_if_fail (off == 0, 0);

    hex = g_strdup_printf ("%s:%s", SKEY_SSH_STR, canonical_id);
    ret = g_quark_from_string (hex);
    
    g_free (canonical_id);
    g_free (hex);
    
    return ret;
}

