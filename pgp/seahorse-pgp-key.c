/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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
#include <config.h>

#include <string.h>

#include <glib/gi18n.h>

#include "seahorse-context.h"
#include "seahorse-source.h"
#include "seahorse-gtkstock.h"

#include "pgp/seahorse-gpgmex.h"
#include "pgp/seahorse-pgp-key.h"

enum {
    PROP_0,
    PROP_PUBKEY,
    PROP_SECKEY,
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

G_DEFINE_TYPE (SeahorsePGPKey, seahorse_pgp_key, SEAHORSE_TYPE_KEY);

/* -----------------------------------------------------------------------------
 * INTERNAL HELPERS
 */

static gchar*
convert_string (const gchar *str, gboolean escape)
{
    gchar *t, *ret;
    
    if (!str)
        return NULL;
    
    /* If not utf8 valid, assume latin 1 */
    if (!g_utf8_validate (str, -1, NULL))
    {
        ret = g_convert (str, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
        if (escape) {
            t = ret;
            ret = g_markup_escape_text (t, -1);
            g_free (t);
        }
        
        return ret;
    }

    if (escape)
        return g_markup_escape_text (str, -1);
    else
        return g_strdup (str);
}

static gchar* 
calc_fingerprint (SeahorsePGPKey *pkey)
{
    const gchar *raw;
    GString *string;
    guint index, len;
    gchar *fpr;
    
    g_assert (pkey->pubkey && pkey->pubkey->subkeys);

    raw = pkey->pubkey->subkeys->fpr;
    g_return_val_if_fail (raw != NULL, NULL);

    string = g_string_new ("");
    len = strlen (raw);
    
    for (index = 0; index < len; index++) {
        if (index > 0 && index % 4 == 0)
            g_string_append (string, " ");
        g_string_append_c (string, raw[index]);
    }
    
    fpr = string->str;
    g_string_free (string, FALSE);
    
    return fpr;
}

static SeahorseValidity 
calc_validity (SeahorsePGPKey *pkey)
{
    g_return_val_if_fail (pkey->pubkey, SEAHORSE_VALIDITY_UNKNOWN);
    g_return_val_if_fail (pkey->pubkey->uids, SEAHORSE_VALIDITY_UNKNOWN);

    if (pkey->pubkey->revoked)
        return SEAHORSE_VALIDITY_REVOKED;
    if (pkey->pubkey->disabled)
        return SEAHORSE_VALIDITY_DISABLED;
    return gpgmex_validity_to_seahorse (pkey->pubkey->uids->validity);
}

static SeahorseValidity 
calc_trust (SeahorsePGPKey *pkey)
{
    g_return_val_if_fail (pkey->pubkey, SEAHORSE_VALIDITY_UNKNOWN);
    return gpgmex_validity_to_seahorse (pkey->pubkey->owner_trust);
}

static void
changed_key (SeahorsePGPKey *pkey)
{
    SeahorseKey *skey = SEAHORSE_KEY (pkey);
    SeahorseObject *obj = SEAHORSE_OBJECT (pkey);
    
    obj->_id = 0;
    obj->_tag = SEAHORSE_PGP;
    
    if (!pkey->pubkey) {
        
        obj->_location = SEAHORSE_LOCATION_INVALID;
        obj->_usage = SEAHORSE_USAGE_NONE;
        skey->loaded = SKEY_INFO_NONE;
        obj->_flags = SKEY_FLAG_DISABLED;
        skey->keydesc = _("Invalid");
        skey->rawid = NULL;
        
    } else {
    
        /* The key id */
        if (pkey->pubkey->subkeys) {
            obj->_id = seahorse_pgp_key_get_cannonical_id (pkey->pubkey->subkeys->keyid);
            skey->rawid = pkey->pubkey->subkeys->keyid;
        }
        
        /* The location */
        if (pkey->pubkey->keylist_mode & GPGME_KEYLIST_MODE_EXTERN && 
            obj->_location <= SEAHORSE_LOCATION_REMOTE)
            obj->_location = SEAHORSE_LOCATION_REMOTE;
        
        else if (obj->_location <= SEAHORSE_LOCATION_LOCAL)
            obj->_location = SEAHORSE_LOCATION_LOCAL;
        
        /* The type */
        if (pkey->seckey) {
            obj->_usage = SEAHORSE_USAGE_PRIVATE_KEY;
            skey->keydesc = _("Private PGP Key");
        } else {
            obj->_usage = SEAHORSE_USAGE_PUBLIC_KEY;
            skey->keydesc = _("Public PGP Key");
        }

        /* The info loaded */
        if (pkey->pubkey->keylist_mode & GPGME_KEYLIST_MODE_SIGS && 
            skey->loaded < SKEY_INFO_COMPLETE)
            skey->loaded = SKEY_INFO_COMPLETE;
        else if (skey->loaded < SKEY_INFO_BASIC)
            skey->loaded = SKEY_INFO_BASIC;
        
        /* The flags */
        obj->_flags = 0;
    
        if (!pkey->pubkey->disabled && !pkey->pubkey->expired && 
            !pkey->pubkey->revoked && !pkey->pubkey->invalid)
        {
            if (calc_validity (pkey) >= SEAHORSE_VALIDITY_MARGINAL)
                obj->_flags |= SKEY_FLAG_IS_VALID;
            if (pkey->pubkey->can_encrypt)
                obj->_flags |= SKEY_FLAG_CAN_ENCRYPT;
            if (pkey->seckey && pkey->pubkey->can_sign)
                obj->_flags |= SKEY_FLAG_CAN_SIGN;
        }
        
        if (pkey->pubkey->expired)
            obj->_flags |= SKEY_FLAG_EXPIRED;
        
        if (pkey->pubkey->revoked)
            obj->_flags |= SKEY_FLAG_REVOKED;
        
        if (pkey->pubkey->disabled)
            obj->_flags |= SKEY_FLAG_DISABLED;
        
        if (calc_trust (pkey) >= SEAHORSE_VALIDITY_MARGINAL && 
            !pkey->pubkey->revoked && !pkey->pubkey->disabled && 
            !pkey->pubkey->expired)
            obj->_flags |= SKEY_FLAG_TRUSTED;
    }
    
    if (!obj->_id)
	    obj->_id = g_quark_from_string (SEAHORSE_PGP_STR ":UNKNOWN UNKNOWN ");
    
    seahorse_object_fire_changed (obj, SEAHORSE_OBJECT_CHANGE_ALL);
}


/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_pgp_key_init (SeahorsePGPKey *pkey)
{
    
}

static guint 
seahorse_pgp_key_get_num_names (SeahorseKey *skey)
{
    SeahorsePGPKey *pkey;
    gint index = 0;
    gpgme_user_id_t uid;

    g_assert (SEAHORSE_IS_PGP_KEY (skey));
    
    pkey = SEAHORSE_PGP_KEY (skey);
    g_assert (pkey->pubkey != NULL);
    
    uid = pkey->pubkey->uids;
    while (uid) {
       uid = uid->next;
       index++;
    }

    return index;        
}

static gchar* 
seahorse_pgp_key_get_name (SeahorseKey *skey, guint index)
{
    SeahorsePGPKey *pkey;
    gpgme_user_id_t uid;
    
    g_assert (SEAHORSE_IS_PGP_KEY (skey));
    pkey = SEAHORSE_PGP_KEY (skey);

    uid = seahorse_pgp_key_get_nth_userid (pkey, index);
    return uid ? convert_string (uid->uid, FALSE) : NULL;
}

static gchar* 
seahorse_pgp_key_get_name_cn (SeahorseKey *skey, guint index)
{
    SeahorsePGPKey *pkey;
    gpgme_user_id_t uid;
    
    g_assert (SEAHORSE_IS_PGP_KEY (skey));
    pkey = SEAHORSE_PGP_KEY (skey);

    uid = seahorse_pgp_key_get_nth_userid (pkey, index);
    return uid && uid->email ? g_strdup (uid->email) : NULL;
}

static gchar*
seahorse_pgp_key_get_name_markup (SeahorseKey *skey, guint index)
{
    SeahorsePGPKey *pkey;
    gpgme_user_id_t uid;
    gchar *email, *name, *comment, *ret;
    gboolean strike = FALSE;
    guint flags;

    g_assert (SEAHORSE_IS_PGP_KEY (skey));
    pkey = SEAHORSE_PGP_KEY (skey);

    uid = seahorse_pgp_key_get_nth_userid (pkey, index);
    g_return_val_if_fail (uid != NULL, NULL);
    
    name = convert_string (uid->name, TRUE);
    email = convert_string (uid->email, TRUE);
    comment = convert_string (uid->comment, TRUE);

    flags = seahorse_key_get_flags (skey);
    if (uid->revoked || flags & CRYPTUI_FLAG_EXPIRED || 
        flags & CRYPTUI_FLAG_REVOKED || flags & CRYPTUI_FLAG_DISABLED)
        strike = TRUE;
    
    ret = g_strconcat (strike ? "<span strikethrough='true'>" : "",
            name,
            "<span foreground='#555555' size='small' rise='0'>",
            email && email[0] ? "  " : "",
            email && email[0] ? email : "",
            comment && comment[0] ? "  '" : "",
            comment && comment[0] ? comment : "",
            comment && comment[0] ? "'" : "",
            "</span>", 
            strike ? "</span>" : "",
            NULL);
    
    g_free (name);
    g_free (comment);
    g_free (email);
    
    return ret;
}

static SeahorseValidity  
seahorse_pgp_key_get_name_validity  (SeahorseKey *skey, guint index)
{
    SeahorsePGPKey *pkey;
    gpgme_user_id_t uid;

    g_assert (SEAHORSE_IS_PGP_KEY (skey));
    pkey = SEAHORSE_PGP_KEY (skey);
    g_return_val_if_fail (pkey->pubkey, SEAHORSE_VALIDITY_UNKNOWN);

    if (pkey->pubkey->revoked)
        return SEAHORSE_VALIDITY_REVOKED;
    if (pkey->pubkey->disabled)
        return SEAHORSE_VALIDITY_DISABLED;
    
    uid = seahorse_pgp_key_get_nth_userid (pkey, index);
    return uid ? gpgmex_validity_to_seahorse (uid->validity) : SEAHORSE_VALIDITY_UNKNOWN;
}

static void
seahorse_pgp_key_get_property (GObject *object, guint prop_id,
                               GValue *value, GParamSpec *pspec)
{
    SeahorsePGPKey *pkey = SEAHORSE_PGP_KEY (object);
    SeahorseKey *skey = SEAHORSE_KEY (object);
    
    switch (prop_id) {
    case PROP_PUBKEY:
        g_value_set_pointer (value, pkey->pubkey);
        break;
    case PROP_SECKEY:
        g_value_set_pointer (value, pkey->seckey);
        break;
    case PROP_DISPLAY_NAME:
        g_value_take_string (value, seahorse_pgp_key_get_name (SEAHORSE_KEY (pkey), 0));
        break;
    case PROP_DISPLAY_ID:
        g_value_set_string (value, seahorse_key_get_short_keyid (SEAHORSE_KEY (pkey)));
        break;
    case PROP_SIMPLE_NAME:        
        g_value_take_string (value, seahorse_pgp_key_get_userid_name (pkey, 0));
        break;
    case PROP_FINGERPRINT:
        g_value_take_string (value, calc_fingerprint(pkey));
        break;
    case PROP_VALIDITY:
        g_value_set_uint (value, calc_validity (pkey));
        break;
    case PROP_TRUST:
        g_value_set_uint (value, calc_trust (pkey));
        break;
    case PROP_EXPIRES:
        if (pkey->pubkey)
            g_value_set_ulong (value, pkey->pubkey->subkeys->expires);
        break;
    case PROP_LENGTH:
        if (pkey->pubkey)
            g_value_set_uint (value, pkey->pubkey->subkeys->length);
        break;
    case PROP_STOCK_ID:
        /* We use a pointer so we don't copy the string every time */
        g_value_set_pointer (value, 
            SEAHORSE_OBJECT (skey)->_usage == SEAHORSE_USAGE_PRIVATE_KEY ? SEAHORSE_STOCK_SECRET : SEAHORSE_STOCK_KEY);
        break;
    }
}

static void
seahorse_pgp_key_set_property (GObject *object, guint prop_id, const GValue *value, 
                               GParamSpec *pspec)
{
    SeahorsePGPKey *skey = SEAHORSE_PGP_KEY (object);

    switch (prop_id) {
    case PROP_PUBKEY:
        if (skey->pubkey)
            gpgmex_key_unref (skey->pubkey);
        skey->pubkey = g_value_get_pointer (value);
        if (skey->pubkey)
            gpgmex_key_ref (skey->pubkey);
        changed_key (skey);
        break;
    case PROP_SECKEY:
        if (skey->seckey)
            gpgmex_key_unref (skey->seckey);
        skey->seckey = g_value_get_pointer (value);
        if (skey->seckey)
            gpgmex_key_ref (skey->seckey);
        changed_key (skey);
        break;
    }
}

static void
seahorse_pgp_key_object_finalize (GObject *gobject)
{
    SeahorsePGPKey *skey = SEAHORSE_PGP_KEY (gobject);
    
    if (skey->pubkey)
        gpgmex_key_unref (skey->pubkey);
    if (skey->seckey)
        gpgmex_key_unref (skey->seckey);
    skey->pubkey = skey->seckey = NULL;
    
    if (skey->photoids)
            gpgmex_photo_id_free_all (skey->photoids);
    skey->photoids = NULL;
    
    G_OBJECT_CLASS (seahorse_pgp_key_parent_class)->finalize (gobject);
}

static void
seahorse_pgp_key_class_init (SeahorsePGPKeyClass *klass)
{
    GObjectClass *gobject_class;
    SeahorseKeyClass *key_class;
    
    seahorse_pgp_key_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->finalize = seahorse_pgp_key_object_finalize;
    gobject_class->set_property = seahorse_pgp_key_set_property;
    gobject_class->get_property = seahorse_pgp_key_get_property;
    
    key_class = SEAHORSE_KEY_CLASS (klass);
    
    key_class->get_num_names = seahorse_pgp_key_get_num_names;
    key_class->get_name = seahorse_pgp_key_get_name;
    key_class->get_name_cn = seahorse_pgp_key_get_name_cn;
    key_class->get_name_markup = seahorse_pgp_key_get_name_markup;
    key_class->get_name_validity = seahorse_pgp_key_get_name_validity;
    
    g_object_class_install_property (gobject_class, PROP_PUBKEY,
        g_param_spec_pointer ("pubkey", "Gpgme Public Key", "Gpgme Public Key that this object represents",
                              G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_SECKEY,
        g_param_spec_pointer ("seckey", "Gpgme Secret Key", "Gpgme Secret Key that this object represents",
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

/**
 * seahorse_pgp_key_new:
 * @pubkey: Public PGP key
 * @pubkey: Optional Private PGP key
 *
 * Creates a new #SeahorsePGPKey 
 *
 * Returns: A new #SeahorsePGPKey
 **/
SeahorsePGPKey* 
seahorse_pgp_key_new (SeahorseSource *sksrc, gpgme_key_t pubkey, 
                      gpgme_key_t seckey)
{
    SeahorsePGPKey *pkey;
    pkey = g_object_new (SEAHORSE_TYPE_PGP_KEY, "key-source", sksrc,
                         "pubkey", pubkey, "seckey", seckey, NULL);
    return pkey;
}

/**
 * seahorse_key_get_num_subkeys:
 * @skey: #SeahorseKey
 *
 * Counts the number of subkeys for @skey.
 *
 * Returns: The number of subkeys for @skey, or -1 if error.
 **/
guint
seahorse_pgp_key_get_num_subkeys (SeahorsePGPKey *pkey)
{
    gint index = 0;
    gpgme_subkey_t subkey;

    g_return_val_if_fail (pkey != NULL && SEAHORSE_IS_PGP_KEY (pkey), -1);
    g_return_val_if_fail (pkey->pubkey != NULL, -1);

    subkey = pkey->pubkey->subkeys;
    while (subkey) {
        subkey = subkey->next;
        index++;
    }
    
    return index;
}

/**
 * seahorse_key_get_nth_subkey:
 * @skey: #SeahorseKey
 * @index: Which subkey
 *
 * Gets the the subkey at @index of @skey.
 *
 * Returns: subkey of @skey at @index, or NULL if @index is out of bounds
 */
gpgme_subkey_t
seahorse_pgp_key_get_nth_subkey (SeahorsePGPKey *pkey, guint index)
{
    gpgme_subkey_t subkey;
    guint n;

    g_return_val_if_fail (pkey != NULL && SEAHORSE_IS_PGP_KEY (pkey), NULL);
    g_return_val_if_fail (pkey->pubkey != NULL, NULL);

    subkey = pkey->pubkey->subkeys;
    for (n = index; subkey && n; n--)
        subkey = subkey->next;

    return subkey;
}

/**
 * seahorse_key_get_userid:
 * @skey: #SeahorseKey
 * @index: Which user ID
 *
 * Gets the formatted user ID of @skey at @index.
 *
 * Returns: UTF8 valid name of @skey at @index,
 * or NULL if @index is out of bounds.
 **/
gchar*
seahorse_pgp_key_get_userid (SeahorsePGPKey *pkey, guint index)
{
    gpgme_user_id_t uid = seahorse_pgp_key_get_nth_userid (pkey, index);
    return uid ? convert_string (uid->uid, FALSE) : NULL;
}

/**
 * seahorse_key_get_userid_name:
 * @skey: #SeahorseKey
 * @index: Which user ID
 *
 * Gets the formatted user ID name of @skey at @index.
 *
 * Returns: UTF8 valid name of @skey at @index,
 * or NULL if @index is out of bounds.
 **/
gchar*
seahorse_pgp_key_get_userid_name (SeahorsePGPKey *pkey, guint index)
{
    gpgme_user_id_t uid = seahorse_pgp_key_get_nth_userid (pkey, index);
    return uid ? convert_string (uid->name, FALSE) : NULL;
}

/**
 * seahorse_key_get_userid_email:
 * @skey: #SeahorseKey
 * @index: Which user ID
 *
 * Gets the formatted email of @skey at @index.
 *
 * Returns: UTF8 valid email of @skey at @index,
 * or NULL if @index is out of bounds.
 **/
gchar*
seahorse_pgp_key_get_userid_email (SeahorsePGPKey *pkey, guint index)
{
    gpgme_user_id_t uid = seahorse_pgp_key_get_nth_userid (pkey, index);
    return uid ? convert_string (uid->email, FALSE) : NULL;
}

/**
 * seahorse_key_get_userid_comment:
 * @skey: #SeahorseKey
 * @index: Which user ID
 *
 * Gets the formatted comment of @skey at @index.
 *
 * Returns: UTF8 valid comment of @skey at @index,
 * or NULL if @index is out of bounds.
 **/
gchar*
seahorse_pgp_key_get_userid_comment (SeahorsePGPKey *pkey, guint index)
{
    gpgme_user_id_t uid = seahorse_pgp_key_get_nth_userid (pkey, index);
    return uid ? convert_string (uid->comment, FALSE) : NULL;
}

guint           
seahorse_pgp_key_get_num_userids (SeahorsePGPKey *pkey)
{
    gint index = 0;
    gpgme_user_id_t uid;

    g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (pkey), 0);    
    g_return_val_if_fail (pkey->pubkey != NULL, 0);
    
    uid = pkey->pubkey->uids;
    while (uid) {
        uid = uid->next;
        index++;
    }
    
    return index;
}

/**
 * seahorse_key_get_nth_userid:
 * @skey: #SeahorseKey
 * @index: Which userid
 *
 * Gets the the subkey at @index of @skey.
 *
 * Returns: subkey of @skey at @index, or NULL if @index is out of bounds
 */
gpgme_user_id_t  
seahorse_pgp_key_get_nth_userid (SeahorsePGPKey *pkey, guint index)
{
    gpgme_user_id_t uid;
    guint n;

    g_return_val_if_fail (pkey != NULL && SEAHORSE_IS_PGP_KEY (pkey), NULL);
    g_return_val_if_fail (pkey->pubkey != NULL, NULL);

    uid = pkey->pubkey->uids;
    for (n = index; uid && n; n--)
        uid = uid->next;

    return uid;
}    

const gchar*
seahorse_pgp_key_get_algo (SeahorsePGPKey *pkey, guint index)
{
    const gchar* algo_type;
    gpgme_subkey_t subkey;
    
    subkey = pkey->pubkey->subkeys;
    while (0 < index) {
        subkey = subkey->next;
        index--;
    }
    
    algo_type = gpgme_pubkey_algo_name (subkey->pubkey_algo);

    if (algo_type == NULL)
        algo_type = _("Unknown");
    else if (g_str_equal ("Elg", algo_type) || g_str_equal("ELG-E", algo_type))
        algo_type = _("ElGamal");

    return algo_type;
}

const gchar*
seahorse_pgp_key_get_id (gpgme_key_t key, guint index)
{
    gpgme_subkey_t subkey = subkey = key->subkeys;
    guint n;

    for (n = index; subkey && n; n--)
        subkey = subkey->next;
    
    g_return_val_if_fail (subkey, "");
    return subkey->keyid;
}

guint           
seahorse_pgp_key_get_num_photoids (SeahorsePGPKey *pkey)
{
    gint index = 0;
    gpgmex_photo_id_t photoid;

    g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (pkey), 0);    
    
    if (pkey->photoids != NULL) {
        photoid = pkey->photoids;
        while (photoid) {
            photoid = photoid->next;
            index++;
        }
    }
    
    return index;
}

/**
 * seahorse_pgp_key_get_nth_photoid:
 * @skey: #SeahorseKey
 * @index: Which photo id
 *
 * Gets the the photo id at @index of @skey.
 *
 * Returns: subkey of @skey at @index, or NULL if @index is out of bounds
 */
gpgmex_photo_id_t  
seahorse_pgp_key_get_nth_photoid (SeahorsePGPKey *pkey, guint index)
{
    gpgmex_photo_id_t photoid;
    guint n;

    g_return_val_if_fail (pkey != NULL && SEAHORSE_IS_PGP_KEY (pkey), NULL);
    g_return_val_if_fail (pkey->photoids != NULL, NULL);
    
    photoid = pkey->photoids;
    for (n = index; photoid && n; n--)
        photoid = photoid->next;

    return photoid;
}    

gpgmex_photo_id_t 
seahorse_pgp_key_get_photoid_n      (SeahorsePGPKey   *pkey,
                                     guint            uid)
{
    gpgmex_photo_id_t photoid;
    
    g_return_val_if_fail (pkey != NULL && SEAHORSE_IS_PGP_KEY (pkey), NULL);
    g_return_val_if_fail (pkey->photoids != NULL, NULL);

    photoid = pkey->photoids;
    while((photoid->uid != uid) && (photoid != NULL))
        photoid = photoid->next;
        
    return photoid;
}

/* 
 * This function is necessary because the uid stored in a gpgme_user_id_t
 * struct is only usable with gpgme functions.  Problems will be caused if 
 * that uid is used with functions found in seahorse-pgp-key-op.h.  This 
 * function is only to be called with uids from gpgme_user_id_t structs.
 */
guint 
seahorse_pgp_key_get_actual_uid       (SeahorsePGPKey   *pkey,
                                       guint            uid)
{
    guint num_uids, num_photoids, uids, uid_count, i;
    gpgmex_photo_id_t photoid;
    gchar *ids;
    
    g_return_val_if_fail (pkey != NULL && SEAHORSE_IS_PGP_KEY (pkey), 0);

    num_uids = seahorse_pgp_key_get_num_userids(pkey);
    num_photoids = seahorse_pgp_key_get_num_photoids(pkey);
    uids = num_uids + num_photoids;

    ids = g_malloc0(uids + 1);
    
    photoid = pkey->photoids;
    while (photoid != NULL){
        ids[photoid->uid - 1] = 'x';
        photoid = photoid->next;
    }
    
    uid_count = 0;
    
    for(i = 0; i < uids; i++){
        if (ids[i] == '\0'){
            uid_count++;
           
            if (uid == uid_count)
                break;
        }
    }
    
    g_free(ids);
    return i + 1;
}

void
seahorse_pgp_key_get_signature_text (SeahorsePGPKey *pkey, gpgme_key_sig_t signature,
                                     gchar **name, gchar **email, gchar **comment)
{
    g_return_if_fail (signature != NULL);
    
    if (name)
        *name = signature->name ? convert_string (signature->name, FALSE) : NULL;
    if (email)
        *email = signature->email ? convert_string (signature->email, FALSE) : NULL;
    if (comment)
        *comment = signature->comment ? convert_string (signature->comment, FALSE) : NULL;
}

guint         
seahorse_pgp_key_get_sigtype (SeahorsePGPKey *pkey, gpgme_key_sig_t signature)
{
    SeahorseObject *sobj;
    GQuark id;
    
    id = seahorse_pgp_key_get_cannonical_id (signature->keyid);
    sobj = seahorse_context_find_object (SCTX_APP (), id, SEAHORSE_LOCATION_LOCAL);
    
    if (sobj) {
        if (seahorse_object_get_usage (sobj) == SEAHORSE_USAGE_PRIVATE_KEY) 
            return SKEY_PGPSIG_TRUSTED | SKEY_PGPSIG_PERSONAL;
        if (seahorse_object_get_flags (sobj) & SKEY_FLAG_TRUSTED)
            return SKEY_PGPSIG_TRUSTED;
    }

    return 0;
}

gboolean        
seahorse_pgp_key_have_signatures (SeahorsePGPKey *pkey, guint types)
{
    gpgme_user_id_t uid;
    gpgme_key_sig_t sig;
    
    for (uid = pkey->pubkey->uids; uid; uid = uid->next) {
        for (sig = uid->signatures; sig; sig = sig->next) {
            if (seahorse_pgp_key_get_sigtype (pkey, sig) & types)
                return TRUE;
        }
    }
    
    return FALSE;
}

GQuark
seahorse_pgp_key_get_cannonical_id (const gchar *id)
{
    guint len = strlen (id);
    GQuark keyid;
    gchar *t;
    
    if (len < 16) {
        g_warning ("invalid keyid (less than 16 chars): %s", id);
        return 0;
    }
    
    if (len > 16)
        id += len - 16;
    
    t = g_strdup_printf ("%s:%s", SEAHORSE_PGP_STR, id);
    keyid = g_quark_from_string (t);
    g_free (t);
    
    return keyid;
}
