/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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

#include "seahorse-context.h"
#include "seahorse-key-source.h"
#include "seahorse-gkeyring-item.h"
#include "seahorse-gtkstock.h"
#include "seahorse-util.h"
#include "seahorse-secure-memory.h"

/* For gnome-keyring secret type ids */
#include "seahorse-pgp-key.h"
#ifdef WITH_SSH
#include "seahorse-ssh-key.h"
#endif 

enum {
    PROP_0,
    PROP_ITEM_ID,
    PROP_ITEM_INFO,
    PROP_ITEM_ATTRIBUTES,
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

G_DEFINE_TYPE (SeahorseGKeyringItem, seahorse_gkeyring_item, SEAHORSE_TYPE_KEY);

/* -----------------------------------------------------------------------------
 * INTERNAL HELPERS
 */

static guint32
find_attribute_int (GnomeKeyringAttributeList *attrs, const gchar *name)
{
    guint i;
    
    if (!attrs)
        return 0;
    
    for (i = 0; i < attrs->len; i++) {
        GnomeKeyringAttribute *attr = &(gnome_keyring_attribute_list_index (attrs, i));
        if (g_strcasecmp (name, attr->name) == 0 && 
            attr->type == GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32)
            return attr->value.integer;
    }
    
    return 0;
}


static gboolean
is_network_item (SeahorseGKeyringItem *git, const gchar *match)
{
    const gchar *protocol;
    if(gnome_keyring_item_info_get_type (git->info) != GNOME_KEYRING_ITEM_NETWORK_PASSWORD)
        return FALSE;
    
    if (!match)
        return TRUE;
    
    protocol = seahorse_gkeyring_item_get_attribute (git, "protocol");
    return protocol && g_strncasecmp (protocol, match, strlen (match)) == 0;
}

static gboolean 
is_custom_display_name (SeahorseGKeyringItem *git, const gchar *display)
{
    const gchar *user;
    const gchar *server;
    const gchar *object;
    guint32 port;
    GString *generated;
    gboolean ret;
    
    if (gnome_keyring_item_info_get_type (git->info) != GNOME_KEYRING_ITEM_NETWORK_PASSWORD)
        return TRUE;
    
    if (!git->attributes || !display)
        return TRUE;
    
    /* 
     * For network passwords gnome-keyring generates in a funky looking display 
     * name that's generated from login credentials. We simulate that generating 
     * here and return FALSE if it matches. This allows us to display user 
     * customized display names and ignore the generated ones.
     */
    
    user = seahorse_gkeyring_item_get_attribute (git, "user");
    server = seahorse_gkeyring_item_get_attribute (git, "server");
    object = seahorse_gkeyring_item_get_attribute (git, "object");
    port = find_attribute_int (git->attributes, "port");
    
    if (!server)
        return TRUE;
    
    generated = g_string_new (NULL);
    if (user != NULL)
        g_string_append_printf (generated, "%s@", user);
    g_string_append (generated, server);
    if (port != 0)
        g_string_append_printf (generated, ":%d", port);
    if (object != NULL)
        g_string_append_printf (generated, "/%s", object);

    ret = strcmp (display, generated->str) != 0;
    g_string_free (generated, TRUE);
    
    return ret;
}

static gchar*
calc_display_name (SeahorseGKeyringItem *git, gboolean always)
{
    const gchar *val;
    gchar *display;
    
    display = gnome_keyring_item_info_get_display_name (git->info);
    
    /* If it's customized by the application or user then display that */
    if (is_custom_display_name (git, display))
        return display;
    
    /* If it's a network item ... */
    if (gnome_keyring_item_info_get_type (git->info) == GNOME_KEYRING_ITEM_NETWORK_PASSWORD) {
        
        /* HTTP usually has a the realm as the "object" display that */
        if (is_network_item (git, "http") && git->attributes) {
            val = seahorse_gkeyring_item_get_attribute (git, "object");
            if (val && val[0]) {
                g_free (display);
                return g_strdup (val);
            }
        }
        
        /* Display the server name as a last resort */
        if (always) {
            val = seahorse_gkeyring_item_get_attribute (git, "server");
            if (val && val[0]) {
                g_free (display);
                return g_strdup (val);
            }
        }
    }
    
    return always ? display : NULL;
}

static gchar*
calc_network_item_markup (SeahorseGKeyringItem *git)
{
    const gchar *object;
    const gchar *user;
    const gchar *protocol;
    const gchar *server;
    
    gchar *uri = NULL;
    gchar *display = NULL;
    gchar *ret;
    
    server = seahorse_gkeyring_item_get_attribute (git, "server");
    protocol = seahorse_gkeyring_item_get_attribute (git, "protocol");
    object = seahorse_gkeyring_item_get_attribute (git, "object");
    user = seahorse_gkeyring_item_get_attribute (git, "user");

    if (!protocol)
        return NULL;
    
    /* The object in HTTP often isn't a path at all */
    if (is_network_item (git, "http"))
        object = NULL;
    
    display = calc_display_name (git, TRUE);
    
    if (server && protocol) {
        uri = g_strdup_printf ("  %s://%s%s%s/%s", 
                               protocol, 
                               user ? user : "",
                               user ? "@" : "",
                               server,
                               object ? object : "");
    }
    
    ret = g_strdup_printf ("%s<span foreground='#555555' size='small' rise='0'>%s</span>",
                           display, uri ? uri : "");
    g_free (display);
    g_free (uri);
    
    return ret;
}

static void
changed_key (SeahorseGKeyringItem *git)
{
    SeahorseKey *skey = SEAHORSE_KEY (git);
    gchar *secret;

    if (!git->info) {
        
        skey->location = SKEY_LOC_INVALID;
        skey->etype = SKEY_ETYPE_NONE;
        skey->loaded = SKEY_INFO_NONE;
        skey->flags = SKEY_FLAG_DISABLED;
        skey->keydesc = _("Invalid");
        skey->rawid = NULL;
        skey->keyid = 0;
        return;
    } 

    WITH_SECURE_MEM ((secret = gnome_keyring_item_info_get_secret (git->info)));
    skey->loaded = (secret == NULL) ? SKEY_INFO_BASIC : SKEY_INFO_COMPLETE;
    g_free (secret);

    skey->ktype = SKEY_GKEYRING;
    skey->location = SKEY_LOC_LOCAL;
    skey->etype = SKEY_CREDENTIALS;
    skey->flags = 0;
    skey->keyid = seahorse_gkeyring_item_get_cannonical (git->item_id);

    if (is_network_item (git, "http")) 
        skey->keydesc = _("Web Password");
    else if (is_network_item (git, NULL)) 
        skey->keydesc = _("Network Password");
    else
        skey->keydesc = _("Password");
    
    seahorse_key_changed (skey, SKEY_CHANGE_ALL);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_gkeyring_item_init (SeahorseGKeyringItem *git)
{

}

static guint 
seahorse_gkeyring_item_get_num_names (SeahorseKey *git)
{
    /* Always one name */
    return 1;
}

static gchar* 
seahorse_gkeyring_item_get_name (SeahorseKey *skey, guint index)
{
    SeahorseGKeyringItem *git = SEAHORSE_GKEYRING_ITEM (skey);
    
    g_return_val_if_fail (index == 0, NULL);
    return calc_display_name (git, TRUE);
}

static gchar* 
seahorse_gkeyring_item_get_name_markup (SeahorseKey *skey, guint index)
{
    SeahorseGKeyringItem *git = SEAHORSE_GKEYRING_ITEM (skey);
    gchar *t, *markup = NULL;
    
    g_return_val_if_fail (index == 0, NULL);
    g_return_val_if_fail (git->info != NULL, NULL);
    
    /* Only do our special markup for network passwords */
    if (is_network_item (git, NULL))
        markup = calc_network_item_markup (git);
    
    if (!markup) {
        t = calc_display_name (git, TRUE);
        markup = g_markup_escape_text (t, -1);
        g_free (t);
    }

    return markup;
}

static gchar* 
seahorse_gkeyring_item_get_name_cn (SeahorseKey *skey, guint index)
{
    g_return_val_if_fail (index == 0, NULL);
    return NULL;
}

static SeahorseValidity  
seahorse_gkeyring_item_get_name_validity  (SeahorseKey *skey, guint index)
{
    g_return_val_if_fail (index == 0, SEAHORSE_VALIDITY_UNKNOWN);
    return SEAHORSE_VALIDITY_FULL;
}

static void
seahorse_gkeyring_item_get_property (GObject *object, guint prop_id,
                                     GValue *value, GParamSpec *pspec)
{
    SeahorseGKeyringItem *git = SEAHORSE_GKEYRING_ITEM (object);
    SeahorseKey *skey = SEAHORSE_KEY (object);
    
    switch (prop_id) {
    case PROP_ITEM_ID:
        g_value_set_uint (value, git->item_id);
        break;
    case PROP_ITEM_INFO:
        g_value_set_pointer (value, git->info);
        break;
    case PROP_ITEM_ATTRIBUTES:
        g_value_set_pointer (value, git->attributes);
        break;
    case PROP_DISPLAY_NAME:
        g_value_take_string (value, seahorse_gkeyring_item_get_name (skey, 0));
        break;
    case PROP_DISPLAY_ID:
        g_value_set_string (value, seahorse_key_get_short_keyid (skey));
        break;
    case PROP_SIMPLE_NAME:
        g_value_take_string (value, seahorse_gkeyring_item_get_name (skey, 0));
        break;
    case PROP_FINGERPRINT:
        g_value_set_string (value, seahorse_key_get_short_keyid (skey));
        break;
    case PROP_VALIDITY:
        g_value_set_uint (value, SEAHORSE_VALIDITY_FULL);
        break;
    case PROP_TRUST:
        g_value_set_uint (value, SEAHORSE_VALIDITY_UNKNOWN);
        break;
    case PROP_EXPIRES:
        g_value_set_ulong (value, 0);
        break;
    case PROP_LENGTH:
        g_value_set_uint (value, 0);
        break;

    case PROP_STOCK_ID:
        /* We use a pointer so we don't copy the string every time */
        switch (git->info ? gnome_keyring_item_info_get_type (git->info) : -1)
        {
        case GNOME_KEYRING_ITEM_GENERIC_SECRET:
            g_value_set_pointer (value, GNOME_STOCK_AUTHENTICATION);
            break;
        case GNOME_KEYRING_ITEM_NETWORK_PASSWORD:
            g_value_set_pointer (value, is_network_item (git, "http") ? 
                    SEAHORSE_THEMED_WEBBROWSER : GTK_STOCK_NETWORK);
            break;
        case GNOME_KEYRING_ITEM_NOTE:
            g_value_set_pointer (value, GNOME_STOCK_BOOK_OPEN);
            break;
        default:
            g_value_set_pointer (value, GNOME_STOCK_BLANK);
            break;
        }        
        break;
    }
}

static void
seahorse_gkeyring_item_set_property (GObject *object, guint prop_id, const GValue *value, 
                                     GParamSpec *pspec)
{
    SeahorseGKeyringItem *git = SEAHORSE_GKEYRING_ITEM (object);
    
    switch (prop_id) {
    case PROP_ITEM_ID:
        git->item_id = g_value_get_uint (value);
        break;
    case PROP_ITEM_INFO:
        if (git->info != g_value_get_pointer (value)) {
            if (git->info)
                gnome_keyring_item_info_free (git->info);
            git->info = g_value_get_pointer (value);
            changed_key (git);
        }
        break;
    case PROP_ITEM_ATTRIBUTES:
        if (git->attributes != g_value_get_pointer (value)) {
            if (git->attributes)
                gnome_keyring_attribute_list_free (git->attributes);
            git->attributes = g_value_get_pointer (value);
            changed_key (git);
        }
        break;
    }
}

static void
seahorse_gkeyring_item_object_finalize (GObject *gobject)
{
    SeahorseGKeyringItem *git = SEAHORSE_GKEYRING_ITEM (gobject);
    
    if (git->info)
        gnome_keyring_item_info_free (git->info);
    git->info = NULL;
    
    if (git->attributes)
        gnome_keyring_attribute_list_free (git->attributes);
    git->attributes = NULL;
    
    G_OBJECT_CLASS (seahorse_gkeyring_item_parent_class)->finalize (gobject);
}

static void
seahorse_gkeyring_item_class_init (SeahorseGKeyringItemClass *klass)
{
    GObjectClass *gobject_class;
    SeahorseKeyClass *key_class;
    
    seahorse_gkeyring_item_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->finalize = seahorse_gkeyring_item_object_finalize;
    gobject_class->set_property = seahorse_gkeyring_item_set_property;
    gobject_class->get_property = seahorse_gkeyring_item_get_property;
    
    key_class = SEAHORSE_KEY_CLASS (klass);
    
    key_class->get_num_names = seahorse_gkeyring_item_get_num_names;
    key_class->get_name = seahorse_gkeyring_item_get_name;
    key_class->get_name_markup = seahorse_gkeyring_item_get_name_markup;
    key_class->get_name_cn = seahorse_gkeyring_item_get_name_cn;
    key_class->get_name_validity = seahorse_gkeyring_item_get_name_validity;
    
    g_object_class_install_property (gobject_class, PROP_ITEM_ID,
        g_param_spec_uint ("item-id", "Item ID", "GNOME Keyring Item ID", 
                           0, G_MAXUINT, 0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class, PROP_ITEM_INFO,
        g_param_spec_pointer ("item-info", "Item Info", "GNOME Keyring Item Info",
                              G_PARAM_READWRITE));
                              
    g_object_class_install_property (gobject_class, PROP_ITEM_ATTRIBUTES,
        g_param_spec_pointer ("item-attributes", "Item Attributes", "GNOME Keyring Item Attributes",
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
        g_param_spec_uint ("length", "Length", "The length of this key.",
                           0, G_MAXUINT, 0, G_PARAM_READABLE));
                           
    g_object_class_install_property (gobject_class, PROP_STOCK_ID,
        g_param_spec_pointer ("stock-id", "The stock icon", "The stock icon id",
                              G_PARAM_READABLE));

}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseGKeyringItem* 
seahorse_gkeyring_item_new (SeahorseKeySource *sksrc, guint32 item_id, 
                            GnomeKeyringItemInfo *info, GnomeKeyringAttributeList *attributes)
{
    SeahorseGKeyringItem *git;
    git = g_object_new (SEAHORSE_TYPE_GKEYRING_ITEM, "key-source", sksrc, 
                        "item-id", item_id, "item-info", info, 
                        "item-attributes", attributes, NULL);
    
    /* We don't care about this floating business */
    g_object_ref (GTK_OBJECT (git));
    gtk_object_sink (GTK_OBJECT (git));
    return git;
}

gchar*
seahorse_gkeyring_item_get_description  (SeahorseGKeyringItem *git)
{
    return calc_display_name (git, FALSE);
}

const gchar*
seahorse_gkeyring_item_get_attribute (SeahorseGKeyringItem *git, const gchar *name)
{
    guint i;
    
    if (!git->attributes)
        return NULL;
    
    for (i = 0; i < git->attributes->len; i++) {
        GnomeKeyringAttribute *attr = &(gnome_keyring_attribute_list_index (git->attributes, i));
        if (g_strcasecmp (name, attr->name) == 0 && 
            attr->type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING)
            return attr->value.string;
    }
    
    return NULL;
}

SeahorseGKeyringUse
seahorse_gkeyring_item_get_use (SeahorseGKeyringItem *git)
{
    const gchar *val;
    
    /* Network passwords */
    if (gnome_keyring_item_info_get_type (git->info) == GNOME_KEYRING_ITEM_NETWORK_PASSWORD) {
        if (is_network_item (git, "http"))
            return SEAHORSE_GKEYRING_USE_WEB;
        return SEAHORSE_GKEYRING_USE_NETWORK;
    }
    
    if (git->attributes) {
        val = seahorse_gkeyring_item_get_attribute (git, "seahorse-key-type");
        if (val) {
            if (strcmp (val, SKEY_SSH_STR) == 0)
                return SEAHORSE_GKEYRING_USE_SSH;
            else if (strcmp (val, SKEY_PGP_STR) == 0)
                return SEAHORSE_GKEYRING_USE_PGP;
        }
    }
    
    return SEAHORSE_GKEYRING_USE_OTHER;
}

GQuark
seahorse_gkeyring_item_get_cannonical (guint32 item_id)
{
    #define BUF_LEN (G_N_ELEMENTS (SKEY_GKEYRING_STR) + 16)
    gchar buf[BUF_LEN];
    
    snprintf(buf, BUF_LEN - 1, "%s:%08X", SKEY_GKEYRING_STR, item_id);
    buf[BUF_LEN - 1] = 0;
    return g_quark_from_string (buf);
}
