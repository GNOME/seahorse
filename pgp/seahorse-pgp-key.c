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
#include "config.h"

#include <string.h>

#include <glib/gi18n.h>

#include "seahorse-context.h"
#include "seahorse-source.h"
#include "seahorse-gtkstock.h"
#include "seahorse-util.h"

#include "common/seahorse-object-list.h"

#include "pgp/seahorse-pgp.h"
#include "pgp/seahorse-gpgmex.h"
#include "pgp/seahorse-pgp-key.h"
#include "pgp/seahorse-pgp-key-op.h"

enum {
	PROP_0,
	PROP_PUBKEY,
	PROP_SECKEY,
	PROP_PHOTOS,
	PROP_SUBKEYS,
	PROP_UIDS,
	PROP_FINGERPRINT,
	PROP_VALIDITY,
	PROP_VALIDITY_STR,
	PROP_TRUST,
	PROP_TRUST_STR,
	PROP_EXPIRES,
	PROP_EXPIRES_STR,
	PROP_LENGTH,
	PROP_ALGO
};

G_DEFINE_TYPE (SeahorsePgpKey, seahorse_pgp_key, SEAHORSE_TYPE_OBJECT);

struct _SeahorsePgpKeyPrivate {
	gpgme_key_t pubkey;   		/* The public key */
	gpgme_key_t seckey;      	/* The secret key */
	gboolean has_secret;		/* Whether we have a secret key or not */

	int list_mode;                  /* What to load our public key as */
	
	GList *uids;			/* All the UID objects */
	GList *subkeys;                 /* All the Subkey objects */
	
	GList *photos;                  /* List of photos */
	gboolean photos_loaded;		/* Photos were loaded */
};

/* -----------------------------------------------------------------------------
 * INTERNAL HELPERS
 */

static gboolean 
load_gpgme_key (GQuark id, int mode, int secret, gpgme_key_t *key)
{
	GError *err = NULL;
	gpgme_ctx_t ctx;
	gpgme_error_t gerr;
	
	ctx = seahorse_pgp_source_new_context ();
	gpgme_set_keylist_mode (ctx, mode);
	gerr = gpgme_op_keylist_start (ctx, seahorse_pgp_key_get_rawid (id), secret);
	if (GPG_IS_OK (gerr)) {
		gerr = gpgme_op_keylist_next (ctx, key);
		gpgme_op_keylist_end (ctx);
	}

	gpgme_release (ctx);
	
	if (!GPG_IS_OK (gerr)) {
		seahorse_gpgme_to_error (gerr, &err);
		g_message ("couldn't load PGP key: %s", err->message);
		return FALSE;
	}
	
	return TRUE;
}

static gboolean
require_key_public (SeahorsePgpKey *self, int list_mode)
{
	gpgme_key_t key = NULL;
	gboolean ret;
	GQuark id;
	
	if (self->pv->pubkey && (self->pv->list_mode & list_mode) == list_mode)
		return TRUE;
	
	list_mode |= self->pv->list_mode;
	
	id = seahorse_object_get_id (SEAHORSE_OBJECT (self));
	g_return_val_if_fail (id, FALSE);
	
	ret = load_gpgme_key (id, list_mode, FALSE, &key);
	if (!ret)
		return FALSE;
	
	self->pv->list_mode = list_mode;
	seahorse_pgp_key_set_public (self, key);
	gpgmex_key_unref (key);
	
	return TRUE;
}

static gboolean
require_key_private (SeahorsePgpKey *self)
{
	gpgme_key_t key = NULL;
	gboolean ret;
	GQuark id;
	
	if (self->pv->seckey)
		return TRUE;
	if (!self->pv->has_secret)
		return FALSE;
	
	id = seahorse_object_get_id (SEAHORSE_OBJECT (self));
	g_return_val_if_fail (id, FALSE);
	
	ret = load_gpgme_key (id, GPGME_KEYLIST_MODE_LOCAL, TRUE, &key);
	if (!ret)
		return FALSE;
	
	seahorse_pgp_key_set_private (self, key);
	gpgmex_key_unref (key);
	
	return TRUE;
}

static gboolean
require_key_uids (SeahorsePgpKey *self)
{
	return require_key_public (self, GPGME_KEYLIST_MODE_LOCAL);
}

static gboolean
require_key_subkeys (SeahorsePgpKey *self)
{
	return require_key_public (self, GPGME_KEYLIST_MODE_LOCAL);
}

static gboolean
require_key_photos (SeahorsePgpKey *self)
{
	gpgme_error_t gerr;
	
	if (self->pv->photos_loaded)
		return TRUE;
	
	gerr = seahorse_pgp_key_op_photos_load (self);
	if (!GPG_IS_OK (gerr)) {
		g_warning ("couldn't load key photos: %s", gpgme_strerror (gerr));
		return FALSE;
	}
	
	self->pv->photos_loaded = TRUE;
	return TRUE;
}

static gchar* 
calc_short_name (SeahorsePgpKey *self)
{
	SeahorsePgpUid *uid;
	
	if (!require_key_uids (self))
		return NULL;

	uid = self->pv->uids->data;
	return uid ? seahorse_pgp_uid_get_name (uid) : NULL;
}

static gchar* 
calc_name (SeahorsePgpKey *self)
{
	SeahorsePgpUid *uid;

	if (!require_key_uids (self))
		return NULL;

	uid = self->pv->uids->data;
	return uid ? seahorse_pgp_uid_calc_label (seahorse_pgp_uid_get_userid (uid)) : "";
}

static gchar* 
calc_markup (SeahorsePgpKey *self, guint flags)
{
	SeahorsePgpUid *uid;

	if (!require_key_uids (self))
		return NULL;

	uid = self->pv->uids->data;
	return uid ? seahorse_pgp_uid_calc_markup (seahorse_pgp_uid_get_userid (uid), flags) : "";
}

static void
renumber_actual_uids (SeahorsePgpKey *self)
{
	GArray *index_map;
	GList *l;
	guint index, i;
	
	g_assert (SEAHORSE_IS_PGP_KEY (self));
	
	/* 
	 * This function is necessary because the uid stored in a gpgme_user_id_t
	 * struct is only usable with gpgme functions.  Problems will be caused if 
	 * that uid is used with functions found in seahorse-pgp-key-op.h.  This 
	 * function is only to be called with uids from gpgme_user_id_t structs.
	 */
	
	/* First we build a bitmap of where all the photo uid indexes are */
	index_map = g_array_new (FALSE, TRUE, sizeof (gboolean));
	for (l = self->pv->photos; l; l = g_list_next (l)) {
		index = seahorse_pgp_photo_get_index (l->data);
		if (index >= index_map->len)
			g_array_set_size (index_map, index + 1);
		g_array_index (index_map, gboolean, index) = TRUE;
	}
	
	/* Now for each UID we add however many photo indexes are below the gpgme index */
	for (l = self->pv->uids; l; l = g_list_next (l)) {
		index = seahorse_pgp_uid_get_gpgme_index (l->data);
		for (i = 0; i < index_map->len && i < index; ++i) {
			if(g_array_index (index_map, gboolean, index))
				++index;
		}
		seahorse_pgp_uid_set_actual_index (l->data, index);
	}
	
	g_array_free (index_map, TRUE);
}

static void 
realize_uids (SeahorsePgpKey *self)
{
	gpgme_user_id_t guid;
	SeahorsePgpUid *uid;
	GList *list = NULL;
	
	if (self->pv->pubkey) {

		for (guid = self->pv->pubkey->uids; guid; guid = guid->next) {
			uid = seahorse_pgp_uid_new (self->pv->pubkey, guid);
			list = seahorse_object_list_prepend (list, uid);
			g_object_unref (uid);
		}
		
		list = g_list_reverse (list);
	}

	seahorse_pgp_key_set_uids (self, list);
	seahorse_object_list_free (list);
}

static void 
realize_subkeys (SeahorsePgpKey *self)
{
	gpgme_subkey_t gsubkey;
	SeahorsePgpSubkey *subkey;
	GList *list = NULL;
	
	if (self->pv->pubkey) {

		for (gsubkey = self->pv->pubkey->subkeys; gsubkey; gsubkey = gsubkey->next) {
			subkey = seahorse_pgp_subkey_new (self->pv->pubkey, gsubkey);
			list = seahorse_object_list_prepend (list, subkey);
			g_object_unref (subkey);
		}
		
		list = g_list_reverse (list);
	}

	seahorse_pgp_key_set_subkeys (self, list);
	seahorse_object_list_free (list);
}

static void
refresh_each_object (SeahorseObject *object, gpointer data)
{
	seahorse_object_refresh (object);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_pgp_key_realize (SeahorseObject *obj)
{
	SeahorsePgpKey *self = SEAHORSE_PGP_KEY (obj);
	SeahorseLocation loc;
	SeahorseUsage usage;
	const gchar *description, *icon, *identifier;
	gchar *name, *markup, *nickname;
	GQuark id;
	guint flags;
	
	if (!require_key_public (self, GPGME_KEYLIST_MODE_LOCAL))
		g_return_if_reached ();
	
	g_return_if_fail (self->pv->pubkey);
	g_return_if_fail (self->pv->pubkey->subkeys);
	id = seahorse_pgp_key_get_cannonical_id (self->pv->pubkey->subkeys->keyid);
	g_object_set (self, "id", id, NULL); 
	
	SEAHORSE_OBJECT_CLASS (seahorse_pgp_key_parent_class)->realize (obj);
	
	/* Update the sub UIDs */
	realize_uids (self);
	realize_subkeys (self);
	
	/* The key id */
	g_return_if_fail (self->pv->pubkey->subkeys);
	identifier = self->pv->pubkey->subkeys->keyid;

	/* The location */
	loc = seahorse_object_get_location (obj);
	
	if (self->pv->pubkey->keylist_mode & GPGME_KEYLIST_MODE_EXTERN && 
	    loc <= SEAHORSE_LOCATION_REMOTE)
		loc = SEAHORSE_LOCATION_REMOTE;
	
	else if (loc <= SEAHORSE_LOCATION_LOCAL)
		loc = SEAHORSE_LOCATION_LOCAL;

	/* The type */
	if (self->pv->seckey) {
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

	if (!self->pv->pubkey->disabled && !self->pv->pubkey->expired && 
	    !self->pv->pubkey->revoked && !self->pv->pubkey->invalid) {
		if (seahorse_pgp_key_get_validity (self) >= SEAHORSE_VALIDITY_MARGINAL)
			flags |= SEAHORSE_FLAG_IS_VALID;
		if (self->pv->pubkey->can_encrypt)
			flags |= SEAHORSE_FLAG_CAN_ENCRYPT;
		if (self->pv->seckey && self->pv->pubkey->can_sign)
			flags |= SEAHORSE_FLAG_CAN_SIGN;
	}

	if (self->pv->pubkey->expired)
		flags |= SEAHORSE_FLAG_EXPIRED;

	if (self->pv->pubkey->revoked)
		flags |= SEAHORSE_FLAG_REVOKED;

	if (self->pv->pubkey->disabled)
		flags |= SEAHORSE_FLAG_DISABLED;

	if (seahorse_pgp_key_get_trust (self) >= SEAHORSE_VALIDITY_MARGINAL && 
	    !self->pv->pubkey->revoked && !self->pv->pubkey->disabled && 
	    !self->pv->pubkey->expired)
		flags |= SEAHORSE_FLAG_TRUSTED;
		
	name = calc_name (self);
	markup = calc_markup (self, flags);
	nickname = calc_short_name (self);
		
	g_object_set (obj,
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

static void
seahorse_pgp_key_flush (SeahorseObject *obj)
{
	SeahorsePgpKey *self = SEAHORSE_PGP_KEY (obj);
	GList *l;
	
	/* Free all the attached UIDs */
	for (l = self->pv->uids; l; l = g_list_next (l))
		seahorse_object_set_parent (l->data, NULL);
	seahorse_object_list_free (self->pv->uids);
	self->pv->uids = NULL;

	/* Free all the attached Photos */
	seahorse_object_list_free (self->pv->photos);
	self->pv->photos = NULL;
	self->pv->photos_loaded = FALSE;
	
	/* Free all the attached Subkeys */
	seahorse_object_list_free (self->pv->subkeys);
	self->pv->subkeys = NULL;

	if (self->pv->pubkey)
		gpgmex_key_unref (self->pv->pubkey);
	if (self->pv->seckey)
		gpgmex_key_unref (self->pv->seckey);
	self->pv->pubkey = self->pv->seckey = NULL;

	SEAHORSE_OBJECT_CLASS (seahorse_pgp_key_parent_class)->flush (obj);
}

static void
seahorse_pgp_key_init (SeahorsePgpKey *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_PGP_KEY, SeahorsePgpKeyPrivate);
}

static void
seahorse_pgp_key_get_property (GObject *object, guint prop_id,
                               GValue *value, GParamSpec *pspec)
{
	SeahorsePgpKey *self = SEAHORSE_PGP_KEY (object);
    
	switch (prop_id) {
	case PROP_PUBKEY:
		g_value_set_boxed (value, seahorse_pgp_key_get_public (self));
		break;
	case PROP_SECKEY:
		g_value_set_boxed (value, seahorse_pgp_key_get_private (self));
		break;
	case PROP_PHOTOS:
		g_value_set_boxed (value, seahorse_pgp_key_get_photos (self));
		break;
	case PROP_SUBKEYS:
		g_value_set_pointer (value, seahorse_pgp_key_get_subkeys (self));
		break;
	case PROP_UIDS:
		g_value_set_boxed (value, seahorse_pgp_key_get_uids (self));
		break;
	case PROP_FINGERPRINT:
		g_value_take_string (value, seahorse_pgp_key_get_fingerprint (self));
		break;
	case PROP_VALIDITY:
		g_value_set_uint (value, seahorse_pgp_key_get_validity (self));
		break;
	case PROP_VALIDITY_STR:
		g_value_set_string (value, seahorse_pgp_key_get_validity_str (self));
		break;
	case PROP_TRUST:
		g_value_set_uint (value, seahorse_pgp_key_get_trust (self));
		break;
	case PROP_TRUST_STR:
		g_value_set_string (value, seahorse_pgp_key_get_trust_str (self));
		break;
	case PROP_EXPIRES:
		g_value_set_ulong (value, seahorse_pgp_key_get_expires (self));
		break;
	case PROP_EXPIRES_STR:
		g_value_take_string (value, seahorse_pgp_key_get_expires_str (self));
		break;
	case PROP_LENGTH:
		g_value_set_uint (value, seahorse_pgp_key_get_length (self));
		break;
	case PROP_ALGO:
		g_value_set_string (value, seahorse_pgp_key_get_algo (self));
		break;
	}
}

static void
seahorse_pgp_key_set_property (GObject *object, guint prop_id, const GValue *value, 
                               GParamSpec *pspec)
{
	SeahorsePgpKey *self = SEAHORSE_PGP_KEY (object);

	switch (prop_id) {
	case PROP_PUBKEY:
		seahorse_pgp_key_set_public (self, g_value_get_boxed (value));
		break;
	case PROP_SECKEY:
		seahorse_pgp_key_set_private (self, g_value_get_boxed (value));
		break;
	}
}

static void
seahorse_pgp_key_object_dispose (GObject *obj)
{
	seahorse_pgp_key_flush (SEAHORSE_OBJECT (obj));
	G_OBJECT_CLASS (seahorse_pgp_key_parent_class)->dispose (obj);
}

static void
seahorse_pgp_key_object_finalize (GObject *obj)
{
	SeahorsePgpKey *self = SEAHORSE_PGP_KEY (obj);

	g_assert (self->pv->uids == NULL);
	g_assert (self->pv->pubkey == NULL);
	g_assert (self->pv->seckey == NULL);
	g_assert (self->pv->photos == NULL);
	g_assert (self->pv->subkeys == NULL);
    
	G_OBJECT_CLASS (seahorse_pgp_key_parent_class)->finalize (obj);
}

static void
seahorse_pgp_key_class_init (SeahorsePgpKeyClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseObjectClass *seahorse_class = SEAHORSE_OBJECT_CLASS (klass);
	
	seahorse_pgp_key_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorsePgpKeyPrivate));

	gobject_class->dispose = seahorse_pgp_key_object_dispose;
	gobject_class->finalize = seahorse_pgp_key_object_finalize;
	gobject_class->set_property = seahorse_pgp_key_set_property;
	gobject_class->get_property = seahorse_pgp_key_get_property;
	
	seahorse_class->flush = seahorse_pgp_key_flush;
	seahorse_class->realize = seahorse_pgp_key_realize;
    
	g_object_class_install_property (gobject_class, PROP_PUBKEY,
	        g_param_spec_boxed ("pubkey", "GPGME Public Key", "GPGME Public Key that this object represents",
	                            SEAHORSE_PGP_BOXED_KEY, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_SECKEY,
                g_param_spec_boxed ("seckey", "GPGME Secret Key", "GPGME Secret Key that this object represents",
                                    SEAHORSE_PGP_BOXED_KEY, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_PHOTOS,
                g_param_spec_boxed ("photos", "Key Photos", "Photos for the key",
                                    SEAHORSE_BOXED_OBJECT_LIST, G_PARAM_READWRITE));
	
	g_object_class_install_property (gobject_class, PROP_SUBKEYS,
                g_param_spec_boxed ("subkeys", "PGP subkeys", "PGP subkeys",
                                    SEAHORSE_BOXED_OBJECT_LIST, G_PARAM_READABLE));
	
	g_object_class_install_property (gobject_class, PROP_UIDS,
                g_param_spec_boxed ("uids", "PGP User Ids", "PGP User Ids",
                                    SEAHORSE_BOXED_OBJECT_LIST, G_PARAM_READWRITE));

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
 	
 	g_object_class_install_property (gobject_class, PROP_ALGO,
 	        g_param_spec_string ("algo", "Algorithm", "The algorithm of this key.",
 	                             "", G_PARAM_READABLE));
}


/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorsePgpKey* 
seahorse_pgp_key_new (SeahorseSource *sksrc, gpgme_key_t pubkey, 
                      gpgme_key_t seckey)
{
	return g_object_new (SEAHORSE_TYPE_PGP_KEY, "source", sksrc,
	                     "pubkey", pubkey, "seckey", seckey, NULL);
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
seahorse_pgp_key_reload (SeahorsePgpKey *pkey)
{
	SeahorseSource *src;
	
	g_return_if_fail (SEAHORSE_IS_PGP_KEY (pkey));
	src = seahorse_object_get_source (SEAHORSE_OBJECT (pkey));
	g_return_if_fail (SEAHORSE_IS_PGP_SOURCE (src));
	seahorse_source_load_async (src, seahorse_object_get_id (SEAHORSE_OBJECT (pkey)));
}

gpgme_key_t
seahorse_pgp_key_get_public (SeahorsePgpKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), NULL);
	if (require_key_public (self, GPGME_KEYLIST_MODE_LOCAL))
		return self->pv->pubkey;
	return NULL;
}

void
seahorse_pgp_key_set_public (SeahorsePgpKey *self, gpgme_key_t key)
{
	GObject *obj;
	
	g_return_if_fail (SEAHORSE_IS_PGP_KEY (self));
	
	if (self->pv->pubkey)
		gpgmex_key_unref (self->pv->pubkey);
	self->pv->pubkey = key;
	if (self->pv->pubkey) {
		gpgmex_key_ref (self->pv->pubkey);
		self->pv->list_mode |= self->pv->pubkey->keylist_mode;
	}
	
	obj = G_OBJECT (self);
	g_object_freeze_notify (obj);
	seahorse_pgp_key_realize (SEAHORSE_OBJECT (self));
	g_object_notify (obj, "fingerprint");
	g_object_notify (obj, "validity");
	g_object_notify (obj, "validity-str");
	g_object_notify (obj, "trust");
	g_object_notify (obj, "trust-str");
	g_object_notify (obj, "expires");
	g_object_notify (obj, "expires-str");
	g_object_notify (obj, "length");
	g_object_notify (obj, "algo");
	g_object_thaw_notify (obj);
}

gpgme_key_t
seahorse_pgp_key_get_private (SeahorsePgpKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), NULL);
	if (require_key_private (self))
		return self->pv->seckey;
	return NULL;	
}

void
seahorse_pgp_key_set_private (SeahorsePgpKey *self, gpgme_key_t key)
{
	GObject *obj;
	
	g_return_if_fail (SEAHORSE_IS_PGP_KEY (self));
	
	if (self->pv->seckey)
		gpgmex_key_unref (self->pv->seckey);
	self->pv->seckey = key;
	if (self->pv->seckey)
		gpgmex_key_ref (self->pv->seckey);
	
	obj = G_OBJECT (self);
	g_object_freeze_notify (obj);
	seahorse_pgp_key_realize (SEAHORSE_OBJECT (self));
	g_object_thaw_notify (obj);
}

GList*
seahorse_pgp_key_get_uids (SeahorsePgpKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), NULL);
	if (!require_key_uids (self))
		return NULL;
	return self->pv->uids;
}

void
seahorse_pgp_key_set_uids (SeahorsePgpKey *self, GList *uids)
{
	SeahorseSource *source;
	GList *l;
	
	g_return_if_fail (SEAHORSE_IS_PGP_KEY (self));

	/* Remove the parent on each old one */
	for (l = self->pv->uids; l; l = g_list_next (l)) 
		seahorse_object_set_parent (l->data, NULL);

	seahorse_object_list_free (self->pv->uids);
	self->pv->uids = seahorse_object_list_copy (uids);

	source = seahorse_object_get_source (SEAHORSE_OBJECT (self));
	g_return_if_fail (SEAHORSE_IS_SOURCE (source));

	/* Set parent and source on each new one, except the first */
	for (l = self->pv->uids ? g_list_next (self->pv->uids) : NULL;
	     l; l = g_list_next (l)) {
		g_object_set (l->data, "source", source, NULL);
		seahorse_object_set_parent (l->data, SEAHORSE_OBJECT (self));
	}
	
	renumber_actual_uids (self);
	g_object_notify (G_OBJECT (self), "uids");
}

GList*
seahorse_pgp_key_get_subkeys (SeahorsePgpKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), NULL);
	if (!require_key_subkeys (self))
		return NULL;
	return self->pv->subkeys;
}

void
seahorse_pgp_key_set_subkeys (SeahorsePgpKey *self, GList *subkeys)
{
	g_return_if_fail (SEAHORSE_IS_PGP_KEY (self));
	
	seahorse_object_list_free (self->pv->subkeys);
	self->pv->subkeys = seahorse_object_list_copy (subkeys);
	
	g_object_notify (G_OBJECT (self), "subkeys");
}

GList*
seahorse_pgp_key_get_photos (SeahorsePgpKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), NULL);
	if (!require_key_photos (self))
		return NULL;
	return self->pv->photos;
}

void
seahorse_pgp_key_set_photos (SeahorsePgpKey *self, GList *photos)
{
	g_return_if_fail (SEAHORSE_IS_PGP_KEY (self));
	
	seahorse_object_list_free (self->pv->photos);
	self->pv->photos = seahorse_object_list_copy (photos);
	self->pv->photos_loaded = TRUE;
	
	renumber_actual_uids (self);
	g_object_notify (G_OBJECT (self), "photos");
}

gchar*
seahorse_pgp_key_get_fingerprint (SeahorsePgpKey *self)
{
	const gchar *raw;
	GString *string;
	guint index, len;
	gchar *fpr;
	    
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), NULL);
	if (!require_key_public (self, GPGME_KEYLIST_MODE_LOCAL))
		return NULL;
	
	g_return_val_if_fail (self->pv->pubkey && self->pv->pubkey->subkeys, NULL);

	raw = self->pv->pubkey->subkeys->fpr;
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

SeahorseValidity
seahorse_pgp_key_get_validity (SeahorsePgpKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), SEAHORSE_VALIDITY_UNKNOWN);
	if (!require_key_public (self, GPGME_KEYLIST_MODE_LOCAL))
		return SEAHORSE_VALIDITY_UNKNOWN;
	
	g_return_val_if_fail (self->pv->pubkey, SEAHORSE_VALIDITY_UNKNOWN);
	g_return_val_if_fail (self->pv->uids, SEAHORSE_VALIDITY_UNKNOWN);

	if (self->pv->pubkey->revoked)
		return SEAHORSE_VALIDITY_REVOKED;
	if (self->pv->pubkey->disabled)
		return SEAHORSE_VALIDITY_DISABLED;
	return seahorse_pgp_uid_get_validity (self->pv->uids->data);
}

const gchar*
seahorse_pgp_key_get_validity_str (SeahorsePgpKey *self)
{
	SeahorseValidity validity = seahorse_pgp_key_get_validity (self);
	return seahorse_validity_get_string (validity);
}

gulong
seahorse_pgp_key_get_expires (SeahorsePgpKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), 0);
	if (!require_key_public (self, GPGME_KEYLIST_MODE_LOCAL))
		return 0;
	g_return_val_if_fail (self->pv->pubkey->subkeys, 0);
	return self->pv->pubkey->subkeys->expires;
}

gchar*
seahorse_pgp_key_get_expires_str (SeahorsePgpKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), SEAHORSE_VALIDITY_UNKNOWN);
	if (!require_key_public (self, GPGME_KEYLIST_MODE_LOCAL))
		return g_strdup ("");
	
	if (self->pv->pubkey->expired) {
		return g_strdup (_("Expired"));
	} else {
		g_return_val_if_fail (self->pv->pubkey->subkeys, NULL);
		if (self->pv->pubkey->subkeys->expires == 0)
			return g_strdup ("");
		else 
			return seahorse_util_get_date_string (self->pv->pubkey->subkeys->expires);
	}
}

SeahorseValidity
seahorse_pgp_key_get_trust (SeahorsePgpKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), SEAHORSE_VALIDITY_UNKNOWN);
	if (!require_key_public (self, GPGME_KEYLIST_MODE_LOCAL))
		return SEAHORSE_VALIDITY_UNKNOWN;
	
	return gpgmex_validity_to_seahorse (self->pv->pubkey->owner_trust);
}

const gchar*
seahorse_pgp_key_get_trust_str (SeahorsePgpKey *self)
{
	SeahorseValidity trust = seahorse_pgp_key_get_trust (self);
	return seahorse_validity_get_string (trust);
}

guint
seahorse_pgp_key_get_length (SeahorsePgpKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), 0);
	if (!require_key_public (self, GPGME_KEYLIST_MODE_LOCAL))
		return 0;
	
	g_return_val_if_fail (self->pv->pubkey->subkeys, 0);
	return self->pv->pubkey->subkeys->length;
}

const gchar*
seahorse_pgp_key_get_algo (SeahorsePgpKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), NULL);
	if (!require_key_subkeys (self))
		return "";
	g_return_val_if_fail (self->pv->subkeys, NULL);
	return seahorse_pgp_subkey_get_algorithm (self->pv->subkeys->data);
}

const gchar*
seahorse_pgp_key_get_keyid (SeahorsePgpKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), NULL);
	g_return_val_if_fail (self->pv->pubkey, NULL);
	g_return_val_if_fail (self->pv->pubkey->subkeys, NULL);
	g_return_val_if_fail (self->pv->pubkey->subkeys->keyid, NULL);
	return self->pv->pubkey->subkeys->keyid;
}

gboolean
seahorse_pgp_key_has_keyid (SeahorsePgpKey *self, const gchar *match)
{
	gpgme_subkey_t subkey;
	const gchar *keyid;
	guint n_match, n_keyid;
	
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), FALSE);
	g_return_val_if_fail (match, FALSE);
	g_return_val_if_fail (self->pv->pubkey, FALSE);
	g_return_val_if_fail (self->pv->pubkey->subkeys, FALSE);
	
	n_match = strlen (match);
	
	for (subkey = self->pv->pubkey->subkeys; subkey; subkey = subkey->next) {
		g_return_val_if_fail (subkey->keyid, FALSE);
		keyid = subkey->keyid;
		n_keyid = strlen (keyid);
		if (n_match <= n_keyid) {
			keyid += (n_keyid - n_match);
			if (strncmp (keyid, match, n_match) == 0)
				return TRUE;
		}
	}
	
	return FALSE;
}

void
seahorse_pgp_key_refresh_matching (gpgme_key_t key)
{
	SeahorseObjectPredicate pred;
	
	g_return_if_fail (key->subkeys->keyid);
	
	memset (&pred, 0, sizeof (pred));
	pred.type = SEAHORSE_TYPE_PGP_KEY;
	pred.id = seahorse_pgp_key_get_cannonical_id (key->subkeys->keyid);
	
	seahorse_context_for_objects_full (NULL, &pred, refresh_each_object, NULL);
}
