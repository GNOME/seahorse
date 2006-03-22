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

#include <gnome.h>

#include "seahorse-gpgmex.h"
#include "seahorse-context.h"
#include "seahorse-key-source.h"
#include "seahorse-pgp-key.h"

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
    PROP_EXPIRES
};

static void class_init (SeahorsePGPKeyClass *klass);
static void object_finalize (GObject *gobject);

static void	set_property (GObject *object, guint prop_id, 
                          const GValue *value, GParamSpec *pspec);
static void	get_property (GObject *object, guint prop_id, 
                          GValue *value, GParamSpec *pspec);

/* SeahorseKey methods */
static guint key_get_num_names (SeahorseKey *skey);
static gchar* key_get_name (SeahorseKey *skey, guint uid);
static gchar* key_get_name_cn (SeahorseKey *skey, guint uid);

/* Other forward declarations */
static gchar* calc_fingerprint (SeahorsePGPKey *skey);
static SeahorseValidity calc_validity (SeahorsePGPKey *skey);
static SeahorseValidity calc_trust (SeahorsePGPKey *skey);
static void changed_key (SeahorsePGPKey *pkey);

static SeahorseKey *parent_class = NULL;

GType
seahorse_pgp_key_get_type (void)
{
	static GType gtype = 0;
	
	if (!gtype) {
		static const GTypeInfo gtinfo = {
			sizeof (SeahorsePGPKeyClass), NULL, NULL,
			(GClassInitFunc) class_init, NULL, NULL,
			sizeof (SeahorsePGPKey), 0, NULL
		};
		
		gtype = g_type_register_static (SEAHORSE_TYPE_KEY, "SeahorsePGPKey", 
                                        &gtinfo, 0);
	}
	
	return gtype;
}

static void
class_init (SeahorsePGPKeyClass *klass)
{
    GObjectClass *gobject_class;
    SeahorseKeyClass *key_class;
    
    parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->finalize = object_finalize;
    gobject_class->set_property = set_property;
    gobject_class->get_property = get_property;
    
    key_class = SEAHORSE_KEY_CLASS (klass);
    
    key_class->get_num_names = key_get_num_names;
    key_class->get_name = key_get_name;
    key_class->get_name_cn = key_get_name_cn;
    
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

}

/* Unrefs gpgme key and frees data */
static void
object_finalize (GObject *gobject)
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
	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
set_property (GObject *object, guint prop_id, const GValue *value, 
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
get_property (GObject *object, guint prop_id,
              GValue *value, GParamSpec *pspec)
{
    SeahorsePGPKey *pkey = SEAHORSE_PGP_KEY (object);
    
    switch (prop_id) {
    case PROP_PUBKEY:
        g_value_set_pointer (value, pkey->pubkey);
        break;
    case PROP_SECKEY:
        g_value_set_pointer (value, pkey->seckey);
        break;
    case PROP_DISPLAY_NAME:
        g_value_take_string (value, key_get_name (SEAHORSE_KEY (pkey), 0));
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
    }
}

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
seahorse_pgp_key_new (SeahorseKeySource *sksrc, gpgme_key_t pubkey, 
                      gpgme_key_t seckey)
{
    SeahorsePGPKey *pkey;
    pkey = g_object_new (SEAHORSE_TYPE_PGP_KEY, "key-source", sksrc,
                         "pubkey", pubkey, "seckey", seckey, NULL);
    
    /* We don't care about this floating business */
    g_object_ref (GTK_OBJECT (pkey));
    gtk_object_sink (GTK_OBJECT (pkey));
    return pkey;
}

static gchar*
convert_string (const gchar *str)
{
    if (!str)
        return NULL;
    
	/* If not utf8 valid, assume latin 1 */
	if (!g_utf8_validate (str, -1, NULL))
		return g_convert (str, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
	else
		return g_strdup (str);    
}

static guint 
key_get_num_names (SeahorseKey *skey)
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
key_get_name (SeahorseKey *skey, guint index)
{
    SeahorsePGPKey *pkey;
    gpgme_user_id_t uid;
    
    g_assert (SEAHORSE_IS_PGP_KEY (skey));
    pkey = SEAHORSE_PGP_KEY (skey);

    uid = seahorse_pgp_key_get_nth_userid (pkey, index);
	return uid ? convert_string (uid->uid) : NULL;
}

static gchar* 
key_get_name_cn (SeahorseKey *skey, guint index)
{
    SeahorsePGPKey *pkey;
    gpgme_user_id_t uid;
    
    g_assert (SEAHORSE_IS_PGP_KEY (skey));
    pkey = SEAHORSE_PGP_KEY (skey);

    uid = seahorse_pgp_key_get_nth_userid (pkey, index);
	return uid && uid->email ? g_strdup (uid->email) : NULL;
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

	if (pkey->pubkey->revoked)
		return SEAHORSE_VALIDITY_REVOKED;
	if (pkey->pubkey->disabled)
		return SEAHORSE_VALIDITY_DISABLED;
	if (pkey->pubkey->uids->validity <= SEAHORSE_VALIDITY_UNKNOWN)
		return SEAHORSE_VALIDITY_UNKNOWN;
	return pkey->pubkey->uids->validity;    
}

static SeahorseValidity 
calc_trust (SeahorsePGPKey *pkey)
{
	g_return_val_if_fail (pkey->pubkey, SEAHORSE_VALIDITY_UNKNOWN);

   	if (pkey->pubkey->owner_trust <= SEAHORSE_VALIDITY_UNKNOWN)
		return SEAHORSE_VALIDITY_UNKNOWN;
	return pkey->pubkey->owner_trust;
}

static void
changed_key (SeahorsePGPKey *pkey)
{
    SeahorseKey *skey = SEAHORSE_KEY (pkey);
    guint l;
    
    skey->ktype = SKEY_PGP;
    
    if (!pkey->pubkey) {
        
        skey->keyid = "UNKNOWN ";
        skey->location = SKEY_LOC_UNKNOWN;
        skey->etype = SKEY_INVALID;
        skey->loaded = SKEY_INFO_NONE;
        skey->flags = SKEY_FLAG_DISABLED;
        
    } else {
    
        /* The key id */
        if (pkey->pubkey->subkeys)
            skey->keyid = pkey->pubkey->subkeys->keyid;
        else
            skey->keyid = "UNKNOWN ";
        
        /* The location */
        if (pkey->pubkey->keylist_mode & GPGME_KEYLIST_MODE_EXTERN && 
            skey->location < SKEY_LOC_REMOTE)
            skey->location = SKEY_LOC_REMOTE;
        else if (skey->location < SKEY_LOC_LOCAL)
            skey->location = SKEY_LOC_LOCAL;
        
        /* The type */
        skey->etype = pkey->seckey ? SKEY_PRIVATE : SKEY_PUBLIC;

        /* The info loaded */
        if (pkey->pubkey->keylist_mode & GPGME_KEYLIST_MODE_SIGS && 
            skey->loaded < SKEY_INFO_COMPLETE)
            skey->loaded = SKEY_INFO_COMPLETE;
        else if (skey->loaded < SKEY_INFO_BASIC)
            skey->loaded = SKEY_INFO_BASIC;
        
        /* The flags */
        skey->flags = 0;
    
        if (!pkey->pubkey->disabled && !pkey->pubkey->expired && 
            !pkey->pubkey->revoked && !pkey->pubkey->invalid)
        {
            skey->flags |= SKEY_FLAG_IS_VALID;
            if (pkey->pubkey->can_encrypt)
                skey->flags |= SKEY_FLAG_CAN_ENCRYPT;
            if (pkey->seckey && pkey->pubkey->can_sign)
                skey->flags |= SKEY_FLAG_CAN_SIGN;
        }
        
        if (pkey->pubkey->expired)
            skey->flags |= SKEY_FLAG_EXPIRED;
        
        if (pkey->pubkey->revoked)
            skey->flags |= SKEY_FLAG_REVOKED;
        
        if (pkey->pubkey->disabled)
            skey->flags |= SKEY_FLAG_DISABLED;
        
        if (calc_trust (pkey) >= SEAHORSE_VALIDITY_MARGINAL)
            skey->flags |= SKEY_FLAG_TRUSTED;
    }
    
    l = strlen (skey->keyid);
    if (l >= 16)
        skey->keyid += l - 16;
    
    seahorse_key_changed (skey, SKEY_CHANGE_ALL);
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
	return uid ? convert_string (uid->name) : NULL;
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
	return uid ? convert_string (uid->email) : NULL;
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
	return uid ? convert_string (uid->comment) : NULL;
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
        *name = signature->name ? convert_string (signature->name) : NULL;
    if (email)
        *email = signature->email ? convert_string (signature->email) : NULL;
    if (comment)
        *comment = signature->comment ? convert_string (signature->comment) : NULL;
}

guint         
seahorse_pgp_key_get_sigtype (SeahorsePGPKey *pkey, gpgme_key_sig_t signature)
{
    SeahorseKey *skey;
    
    skey = seahorse_context_find_key (SCTX_APP (), SKEY_PGP, 
                                      SKEY_LOC_LOCAL, signature->keyid);
    
    if (skey) {
        if (seahorse_key_get_etype (skey) == SKEY_PRIVATE) 
            return SKEY_PGPSIG_TRUSTED | SKEY_PGPSIG_PERSONAL;
        if (seahorse_key_get_flags (skey) & SKEY_FLAG_TRUSTED)
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
