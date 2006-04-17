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

enum {
    PROP_0,
    PROP_KEY_DATA,
    PROP_DISPLAY_NAME,
    PROP_DISPLAY_ID,
    PROP_SIMPLE_NAME,
    PROP_FINGERPRINT,
    PROP_VALIDITY,
    PROP_TRUST,
    PROP_EXPIRES
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

static guint 
parse_ssh_algo (gchar *type)
{
    gchar *t;

    /* Lower case */
    for (t = type; *t; t++)
        *t = g_ascii_tolower (*t);
    
    if (strstr (type, "rsa1"))
        return SSH_ALGO_RSA1;
    if (strstr (type, "rsa"))
        return SSH_ALGO_RSA;
    if (strstr (type, "dsa"))
        return SSH_ALGO_DSA;
    return SSH_ALGO_UNK;
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
    
    if (!skey->keydata || !skey->keydata->keyid) {
        
        key->keyid = "UNKNOWN ";
        key->location = SKEY_LOC_INVALID;
        key->etype = SKEY_INVALID;
        key->loaded = SKEY_INFO_NONE;
        key->flags = SKEY_FLAG_DISABLED;
        
    } else {
    
        /* The key id */
        key->keyid = skey->keydata->keyid ? skey->keydata->keyid : "UNKNOWN ";
        key->location = SKEY_LOC_LOCAL;
        key->etype = SKEY_PRIVATE;
        key->loaded = SKEY_INFO_COMPLETE;
        key->flags = SKEY_FLAG_TRUSTED;
    }
    
    seahorse_key_changed (key, SKEY_CHANGE_ALL);
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
        g_value_set_uint (value, SEAHORSE_VALIDITY_ULTIMATE);
        break;
    case PROP_TRUST:
        g_value_set_uint (value, SEAHORSE_VALIDITY_ULTIMATE);
        break;
    case PROP_EXPIRES:
        g_value_set_ulong (value, 0);
        break;
    }
}

static void
seahorse_ssh_key_set_property (GObject *object, guint prop_id, 
                               const GValue *value, GParamSpec *pspec)
{
    SeahorseSSHKey *skey = SEAHORSE_SSH_KEY (object);

    switch (prop_id) {
    case PROP_KEY_DATA:
        seahorse_ssh_key_data_free (skey->keydata);
        skey->keydata = (SeahorseSSHKeyData*)g_value_get_pointer (value);
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

const gchar*
seahorse_ssh_key_get_algo (SeahorseSSHKey *skey)
{
    g_return_val_if_fail (SEAHORSE_IS_SSH_KEY (skey), "");
    
    switch(skey->keydata->algo) {
    case SSH_ALGO_UNK:
        return "";
    case SSH_ALGO_RSA:
        return "RSA";
    case SSH_ALGO_DSA:
        return "DSA";
    case SSH_ALGO_RSA1:
        return "RSA1";
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
seahorse_ssh_key_get_filename (SeahorseSSHKey *skey, gboolean private)
{
    g_return_val_if_fail (SEAHORSE_IS_SSH_KEY (skey), NULL);
    if (!skey->keydata)
        return NULL;
    return private ? skey->keydata->filename : skey->keydata->filepub;
}

gchar*
seahorse_ssh_key_get_cannonical_id (const gchar *id)
{
    /* TODO: Implement this properly */
    return g_strdup (id);
}

/* -----------------------------------------------------------------------------
 * SSH KEY DATA 
 */

SeahorseSSHKeyData*
seahorse_ssh_key_data_read (SeahorseSSHSource *ssrc, const gchar *filename)
{
    SeahorseSSHKeyData *data;
    GError *error = NULL;
    gchar **values;
    gchar *t, *results;
    
    data = g_new0 (SeahorseSSHKeyData, 1);
    data->filename = g_strdup (filename);
    
    /* Lookup length, fingerprint and public key filename */
    t = g_strdup_printf (SSH_KEYGEN_PATH " -l -f %s", data->filename);
    results = seahorse_ssh_operation_sync (ssrc, t, NULL);
    g_free (t);
    
    /* 
     * Parse the length, fingerprint, and public key from a string like: 
     * 1024 a0:f3:2a:b8:00:57:47:7e:03:f6:de:35:77:2d:a0:6a /home/nate/.ssh/id_rsa.pub
     */
    if (results) {
        values = g_strsplit_set (results, " ", 3);
        g_free (results);

        /* Key length */
        if (values[0]) {
            if (values[0][0]) {
                data->length = strtol (values[0], NULL, 10);
                data->length = data->length < 0 ? 0 : data->length;
            }
            
            /* Fingerprint */
            if (values[1]) {
                if (values[1][0]) {
                    data->fingerprint = g_strdup (values[1]);
                    g_strstrip (data->fingerprint);
                }
            
                /* Public name */
                if (values[2] && !data->filepub) {
                    data->filepub = g_strdup (values[2]);
                    g_strstrip (data->filepub);
                }
            }
        }
        
        g_strfreev (values);
    }
    
    /* Read in the public key */
    results = NULL;
    if (data->filepub) {
        if(!g_file_get_contents (data->filepub, &results, NULL, &error)) {
            g_warning ("couldn't read public SSH file: %s (%s)", data->filepub, error->message);
            results = NULL;
            g_error_free(error);
        }
    } 
    
    /* 
     * Parse the key type and comment from a string like: 
     * ssh-rsa AAAAB3NzaC1yc2EAAAABIwAzE1/iHkfHMk= nielsen@memberwebs.com
     */
    if (results) {
        values = g_strsplit_set (results, " ", 3);
        g_free (results);
        
        /* Key type */
        if (values[0]) {
            if (values[0][0])
                data->algo = parse_ssh_algo (values[0]);
            
            /* Key Comment */
            if (values[1] && values[2] && values[2][0]) {
                g_strstrip (values[2]);
                
                /* If not utf8 valid, assume latin 1 */
                if (!g_utf8_validate (values[2], -1, NULL))
                    data->comment = g_convert (values[2], -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
                else
                    data->comment = g_strdup (values[2]);
            }
        }
        
        g_strfreev (values);
    }

    /* Make a key id */
    if (data->fingerprint)
        data->keyid = g_strndup (data->fingerprint, 11);
    
    return data;
}

gboolean
seahorse_ssh_key_data_is_valid (SeahorseSSHKeyData *data)
{
    g_return_val_if_fail (data != NULL, FALSE);
    return data->fingerprint != NULL;
}

void 
seahorse_ssh_key_data_free (SeahorseSSHKeyData *data)
{
    if (!data)
        return;
    
    g_free (data->filename);
    g_free (data->filepub);
    g_free (data->comment);
    g_free (data->keyid);
    g_free (data->fingerprint);
    g_free (data);
}
