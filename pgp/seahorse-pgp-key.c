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
#include "seahorse-util.h"

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
    PROP_VALIDITY_STR,
    PROP_TRUST,
    PROP_TRUST_STR,
    PROP_EXPIRES,
    PROP_EXPIRES_STR,
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
	g_return_val_if_fail (pkey->uids, SEAHORSE_VALIDITY_UNKNOWN);

	if (pkey->pubkey->revoked)
		return SEAHORSE_VALIDITY_REVOKED;
	if (pkey->pubkey->disabled)
		return SEAHORSE_VALIDITY_DISABLED;
	return seahorse_pgp_uid_get_validity (pkey->uids->data);
}

static SeahorseValidity 
calc_trust (SeahorsePGPKey *pkey)
{
    g_return_val_if_fail (pkey->pubkey, SEAHORSE_VALIDITY_UNKNOWN);
    return gpgmex_validity_to_seahorse (pkey->pubkey->owner_trust);
}

static gchar* 
calc_short_name (SeahorsePGPKey *pkey)
{
	SeahorsePGPUid *uid = pkey->uids->data;
	return uid ? seahorse_pgp_uid_get_name (uid) : NULL;
}

static void 
update_uids (SeahorsePGPKey *pkey)
{
	gpgme_user_id_t guid;
	SeahorsePGPUid *uid;
	SeahorseSource *source;
	GList *l;
	guint index = 1;
	
	source = seahorse_object_get_source (SEAHORSE_OBJECT (pkey));
	g_return_if_fail (SEAHORSE_IS_PGP_SOURCE (source));
	
	l = pkey->uids;
	guid = pkey->pubkey ? pkey->pubkey->uids : NULL;
	
	/* Look for out of sync or missing UIDs */
	while (l != NULL) {
		g_return_if_fail (SEAHORSE_IS_PGP_UID (l->data));
		uid = SEAHORSE_PGP_UID (l->data);
		l = g_list_next (l);
		
		/* Remove if no uid */
		if (!guid) {
			seahorse_object_set_parent (SEAHORSE_OBJECT (uid), NULL);			
			pkey->uids = g_list_remove (pkey->uids, uid);
			g_object_unref (uid);
		} else {
			/* Bring this UID up to date */
			g_object_set (uid, "userid", guid, "index", index, "source", source, NULL);
			++index;
		}

		if (guid)
			guid = guid->next; 
	}
	
	/* Add new UIDs */
	while (guid != NULL) {
		uid = seahorse_pgp_uid_new (pkey->pubkey, guid);
		g_object_set (uid, "index", index, "source", source, NULL);
		++index;
		pkey->uids = g_list_append (pkey->uids, uid);
		guid = guid->next;
	}
	
	/* Set 'parent' on all UIDs but the first one */
	l = pkey->uids;
	if (l != NULL)
		seahorse_object_set_parent (SEAHORSE_OBJECT (l->data), NULL);
	for (l = g_list_next (l); l; l = g_list_next (l))
		seahorse_object_set_parent (SEAHORSE_OBJECT (l->data), SEAHORSE_OBJECT (pkey));
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
    
        /* Update the sub UIDs */
        update_uids (pkey);
	
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
    	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (skey), 0);
    	pkey = SEAHORSE_PGP_KEY (skey);
    	return g_list_length (pkey->uids);
}

static gchar* 
seahorse_pgp_key_get_name (SeahorseKey *skey, guint index)
{
	SeahorsePGPKey *pkey;
	SeahorsePGPUid *uid;
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (skey), NULL);
	pkey = SEAHORSE_PGP_KEY (skey);
	uid = g_list_nth_data (pkey->uids, index);
	return uid ? seahorse_pgp_uid_get_display_name (uid) : NULL;
}

static gchar* 
seahorse_pgp_key_get_name_markup (SeahorseKey *skey, guint index)
{
	SeahorsePGPKey *pkey;
	SeahorsePGPUid *uid;
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (skey), NULL);
	pkey = SEAHORSE_PGP_KEY (skey);
	uid = g_list_nth_data (pkey->uids, index);
	return uid ? seahorse_pgp_uid_get_markup (uid, seahorse_key_get_flags (skey)) : NULL;
}

static gchar* 
seahorse_pgp_key_get_name_cn (SeahorseKey *skey, guint index)
{
	SeahorsePGPKey *pkey;
	SeahorsePGPUid *uid;
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (skey), NULL);
	pkey = SEAHORSE_PGP_KEY (skey);
	uid = g_list_nth_data (pkey->uids, index);
	return uid ? seahorse_pgp_uid_get_email (uid) : NULL;
}

static SeahorseValidity  
seahorse_pgp_key_get_name_validity  (SeahorseKey *skey, guint index)
{
	SeahorsePGPKey *pkey;
	SeahorsePGPUid *uid;
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (skey), SEAHORSE_VALIDITY_UNKNOWN);
	pkey = SEAHORSE_PGP_KEY (skey);
	uid = g_list_nth_data (pkey->uids, index);

	if (pkey->pubkey->revoked)
		return SEAHORSE_VALIDITY_REVOKED;
	if (pkey->pubkey->disabled)
		return SEAHORSE_VALIDITY_DISABLED;
    
	return uid ? seahorse_pgp_uid_get_validity (uid) : SEAHORSE_VALIDITY_UNKNOWN;
}

static void
seahorse_pgp_key_get_property (GObject *object, guint prop_id,
                               GValue *value, GParamSpec *pspec)
{
    SeahorsePGPKey *pkey = SEAHORSE_PGP_KEY (object);
    SeahorseKey *skey = SEAHORSE_KEY (object);
    gchar *expires;
    
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
        g_value_take_string (value, calc_short_name(pkey));
        break;
    case PROP_FINGERPRINT:
        g_value_take_string (value, calc_fingerprint(pkey));
        break;
    case PROP_VALIDITY:
        g_value_set_uint (value, calc_validity (pkey));
        break;
    case PROP_VALIDITY_STR:
	g_value_set_string (value, seahorse_validity_get_string (calc_validity (pkey)));
	break;
    case PROP_TRUST:
        g_value_set_uint (value, calc_trust (pkey));
        break;
    case PROP_TRUST_STR:
	g_value_set_string (value, seahorse_validity_get_string (calc_trust (pkey)));
	break;
    case PROP_EXPIRES:
        if (pkey->pubkey)
            g_value_set_ulong (value, pkey->pubkey->subkeys->expires);
        break;
    case PROP_EXPIRES_STR:
	if (seahorse_key_get_flags (skey) & SKEY_FLAG_EXPIRED) {
		expires = g_strdup (_("Expired"));
	} else {
		if (pkey->pubkey->subkeys->expires == 0)
	                expires = g_strdup ("");
		else 
			expires = seahorse_util_get_date_string (pkey->pubkey->subkeys->expires);
	}
	g_value_take_string (value, expires);
	break;
    case PROP_LENGTH:
        if (pkey->pubkey)
            g_value_set_uint (value, pkey->pubkey->subkeys->length);
        break;
    case PROP_STOCK_ID:
        g_value_set_string (value, 
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
seahorse_pgp_key_object_dispose (GObject *gobject)
{
	SeahorsePGPKey *pkey = SEAHORSE_PGP_KEY (gobject);
	GList *l;
	
	/* Free all the attached UIDs */
	for (l = pkey->uids; l; l = g_list_next (l)) {
		seahorse_object_set_parent (l->data, NULL);
		g_object_unref (l->data);
	}
	
	g_list_free (pkey->uids);
	pkey->uids = NULL;

	G_OBJECT_CLASS (seahorse_pgp_key_parent_class)->dispose (gobject);
}

static void
seahorse_pgp_key_object_finalize (GObject *gobject)
{
    SeahorsePGPKey *skey = SEAHORSE_PGP_KEY (gobject);

    g_assert (skey->uids == NULL);
    
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
    
    gobject_class->dispose = seahorse_pgp_key_object_dispose;
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

    g_object_class_install_property (gobject_class, PROP_VALIDITY_STR,
        g_param_spec_string ("validity-str", "Validity String", "Validity of this key as a string",
                             "", G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_TRUST,
        g_param_spec_uint ("trust", "Trust", "Trust in this key",
                           0, G_MAXUINT, 0, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_TRUST_STR,
        g_param_spec_string ("trust-str", "Trust String", "Trust in this key as a string",
                             "", G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_EXPIRES,
        g_param_spec_ulong ("expires", "Expires On", "Date this key expires on",
                           0, G_MAXULONG, 0, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_EXPIRES_STR,
        g_param_spec_string ("expires-str", "Expires String", "Readable expiry date",
                             "", G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_LENGTH,
        g_param_spec_uint ("length", "Length", "The length of this key.",
                           0, G_MAXUINT, 0, G_PARAM_READABLE));
                           
    g_object_class_install_property (gobject_class, PROP_STOCK_ID,
        g_param_spec_string ("stock-id", "The stock icon", "The stock icon id",
                             NULL, G_PARAM_READABLE));

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

SeahorsePGPUid*
seahorse_pgp_key_get_uid (SeahorsePGPKey *pkey, guint index)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (pkey), NULL);
	return g_list_nth_data (pkey->uids, index);
}

guint           
seahorse_pgp_key_get_num_uids (SeahorsePGPKey *pkey)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (pkey), 0);
	return g_list_length (pkey->uids);
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

    num_uids = seahorse_pgp_key_get_num_uids (pkey);
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
