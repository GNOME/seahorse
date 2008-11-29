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
    PROP_SIMPLE_NAME,
    PROP_FINGERPRINT,
    PROP_VALIDITY,
    PROP_VALIDITY_STR,
    PROP_TRUST,
    PROP_TRUST_STR,
    PROP_EXPIRES,
    PROP_EXPIRES_STR,
    PROP_LENGTH
};

G_DEFINE_TYPE (SeahorsePGPKey, seahorse_pgp_key, SEAHORSE_TYPE_OBJECT);

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

static gchar* 
calc_name (SeahorsePGPKey *self)
{
	SeahorsePGPUid *uid;
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), NULL);
	uid = g_list_nth_data (self->uids, 0);
	return uid ? seahorse_pgp_uid_get_display_name (uid) : NULL;
}

static gchar* 
calc_markup (SeahorsePGPKey *self)
{
	SeahorsePGPUid *uid;
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), NULL);
	uid = g_list_nth_data (self->uids, 0);
	return uid ? seahorse_pgp_uid_get_markup (uid, seahorse_object_get_flags (SEAHORSE_OBJECT (self))) : NULL;
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
	g_return_if_fail (SEAHORSE_IS_SOURCE (source));
	
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
changed_key (SeahorsePGPKey *self)
{
	SeahorseObject *obj = SEAHORSE_OBJECT (self);
	SeahorseLocation loc;
	SeahorseUsage usage;
	const gchar *description, *icon, *identifier;
	gchar *name, *markup, *nickname;
	GQuark id;
	guint flags;
	
	if (!self->pubkey) {
		
		self->loaded = SKEY_INFO_NONE;
		g_object_set (obj,
		              "id", 0,
		              "label", "",
		              "icon", NULL,
		              "usage", SEAHORSE_USAGE_NONE,
		              "markup", "",
		              "idenitfier", "",
		              "nickname", "",
		              "description", _("Invalid"),
		              "location", SEAHORSE_LOCATION_INVALID,
		              "flags", SEAHORSE_FLAG_DISABLED,
		              NULL);
		return;
	}

	/* Update the sub UIDs */
	update_uids (self);

	/* The key id */
	id = 0;
	identifier = NULL;
	if (self->pubkey->subkeys) {
		id = seahorse_pgp_key_get_cannonical_id (self->pubkey->subkeys->keyid);
		identifier = self->pubkey->subkeys->keyid;
	}

	/* The location */
	loc = seahorse_object_get_location (obj);
	if (self->pubkey->keylist_mode & GPGME_KEYLIST_MODE_EXTERN && 
	    loc <= SEAHORSE_LOCATION_REMOTE)
		loc = SEAHORSE_LOCATION_REMOTE;
	else if (loc <= SEAHORSE_LOCATION_LOCAL)
		loc = SEAHORSE_LOCATION_LOCAL;

	/* The type */
	if (self->seckey) {
		usage = SEAHORSE_USAGE_PRIVATE_KEY;
		description = _("Private PGP Key");
		icon = SEAHORSE_STOCK_SECRET;
	} else {
		usage = SEAHORSE_USAGE_PUBLIC_KEY;
		description = _("Public PGP Key");
		icon = SEAHORSE_STOCK_KEY;
	}

	/* The flags */
	flags = SEAHORSE_FLAG_EXPORTABLE;

	if (!self->pubkey->disabled && !self->pubkey->expired && 
	    !self->pubkey->revoked && !self->pubkey->invalid) {
		if (calc_validity (self) >= SEAHORSE_VALIDITY_MARGINAL)
			flags |= SEAHORSE_FLAG_IS_VALID;
		if (self->pubkey->can_encrypt)
			flags |= SEAHORSE_FLAG_CAN_ENCRYPT;
		if (self->seckey && self->pubkey->can_sign)
			flags |= SEAHORSE_FLAG_CAN_SIGN;
	}

	if (self->pubkey->expired)
		flags |= SEAHORSE_FLAG_EXPIRED;

	if (self->pubkey->revoked)
		flags |= SEAHORSE_FLAG_REVOKED;

	if (self->pubkey->disabled)
		flags |= SEAHORSE_FLAG_DISABLED;

	if (calc_trust (self) >= SEAHORSE_VALIDITY_MARGINAL && 
	    !self->pubkey->revoked && !self->pubkey->disabled && 
	    !self->pubkey->expired)
		flags |= SEAHORSE_FLAG_TRUSTED;
	
	name = calc_name (self);
	markup = calc_markup (self);
	nickname = calc_short_name (self);
	
	g_object_set (obj,
		      "id", id,
		      "label", name,
		      "icon", icon,
		      "usage", usage,
		      "markup", markup,
		      "nickname", nickname,
		      "identifier", identifier,
		      "description", description,
		      "location", loc,
		      "flags", flags,
		      NULL);
	
	g_free (name);
	g_free (markup);
	g_free (nickname);
}


/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_pgp_key_init (SeahorsePGPKey *pkey)
{

}


static void
seahorse_pgp_key_get_property (GObject *object, guint prop_id,
                               GValue *value, GParamSpec *pspec)
{
	SeahorsePGPKey *self = SEAHORSE_PGP_KEY (object);
	gchar *expires;
    
	switch (prop_id) {
	case PROP_PUBKEY:
		g_value_set_pointer (value, self->pubkey);
		break;
	case PROP_SECKEY:
		g_value_set_pointer (value, self->seckey);
		break;
	case PROP_FINGERPRINT:
		g_value_take_string (value, calc_fingerprint (self));
		break;
	case PROP_VALIDITY:
		g_value_set_uint (value, calc_validity (self));
		break;
	case PROP_VALIDITY_STR:
		g_value_set_string (value, seahorse_validity_get_string (calc_validity (self)));
		break;
	case PROP_TRUST:
		g_value_set_uint (value, calc_trust (self));
		break;
	case PROP_TRUST_STR:
		g_value_set_string (value, seahorse_validity_get_string (calc_trust (self)));
		break;
	case PROP_EXPIRES:
		if (self->pubkey)
			g_value_set_ulong (value, self->pubkey->subkeys->expires);
		break;
	case PROP_EXPIRES_STR:
		if (seahorse_object_get_flags (SEAHORSE_OBJECT (self)) & SEAHORSE_FLAG_EXPIRED) {
			expires = g_strdup (_("Expired"));
		} else {
			if (self->pubkey->subkeys->expires == 0)
				expires = g_strdup ("");
			else 
				expires = seahorse_util_get_date_string (self->pubkey->subkeys->expires);
		}
		g_value_take_string (value, expires);
		break;
	case PROP_LENGTH:
		if (self->pubkey)
			g_value_set_uint (value, self->pubkey->subkeys->length);
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
    
    seahorse_pgp_key_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->dispose = seahorse_pgp_key_object_dispose;
    gobject_class->finalize = seahorse_pgp_key_object_finalize;
    gobject_class->set_property = seahorse_pgp_key_set_property;
    gobject_class->get_property = seahorse_pgp_key_get_property;
    
    g_object_class_install_property (gobject_class, PROP_PUBKEY,
        g_param_spec_pointer ("pubkey", "Gpgme Public Key", "Gpgme Public Key that this object represents",
                              G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_SECKEY,
        g_param_spec_pointer ("seckey", "Gpgme Secret Key", "Gpgme Secret Key that this object represents",
                              G_PARAM_READWRITE));
                      
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
    pkey = g_object_new (SEAHORSE_TYPE_PGP_KEY, "source", sksrc,
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
        if (seahorse_object_get_flags (sobj) & SEAHORSE_FLAG_TRUSTED)
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

const gchar* 
seahorse_pgp_key_get_rawid (GQuark keyid)
{
	const gchar* id, *rawid;
	
	id = g_quark_to_string (keyid);
	g_return_val_if_fail (id != NULL, NULL);
	
	rawid = strchr (id, ':');
	return rawid ? rawid + 1 : id;
}

void
seahorse_pgp_key_reload (SeahorsePGPKey *pkey)
{
	SeahorseSource *src;
	
	g_return_if_fail (SEAHORSE_IS_PGP_KEY (pkey));
	src = seahorse_object_get_source (SEAHORSE_OBJECT (pkey));
	g_return_if_fail (SEAHORSE_IS_PGP_SOURCE (src));
	seahorse_source_load_async (src, seahorse_object_get_id (SEAHORSE_OBJECT (pkey)));
}

gulong
seahorse_pgp_key_get_expires (SeahorsePGPKey *self)
{
	gulong expires;
	g_object_get (self, "expires", &expires, NULL);
	return expires;
}

gchar*
seahorse_pgp_key_get_expires_str (SeahorsePGPKey *self)
{
	gchar *expires;
	g_object_get (self, "expires-str", &expires, NULL);
	return expires;
}

gchar*
seahorse_pgp_key_get_fingerprint (SeahorsePGPKey *self)
{
	gchar *fpr;
	g_object_get (self, "fingerprint", &fpr, NULL);
	return fpr;	
}

SeahorseValidity
seahorse_pgp_key_get_trust (SeahorsePGPKey *self)
{
    guint validity;
    g_object_get (self, "trust", &validity, NULL);
    return validity;
}
