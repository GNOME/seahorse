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
#include "pgp/seahorse-pgp-key.h"
#include "pgp/seahorse-pgp-uid.h"
#include "pgp/seahorse-pgp-subkey.h"

enum {
	PROP_0,
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
	GList *uids;			/* All the UID objects */
	GList *subkeys;                 /* All the Subkey objects */
	GList *photos;                  /* List of photos */
};

/* -----------------------------------------------------------------------------
 * INTERNAL HELPERS
 */

static const gchar* 
calc_short_name (SeahorsePgpKey *self)
{
	GList *uids = seahorse_pgp_key_get_uids (self);
	return uids ? seahorse_pgp_uid_get_name (uids->data) : NULL;
}

static gchar* 
calc_name (SeahorsePgpKey *self)
{
	GList *uids = seahorse_pgp_key_get_uids (self);
	return uids ? seahorse_pgp_uid_calc_label (seahorse_pgp_uid_get_name (uids->data),
	                                           seahorse_pgp_uid_get_email (uids->data),
	                                           seahorse_pgp_uid_get_comment (uids->data)) : "";
}

static gchar* 
calc_markup (SeahorsePgpKey *self, guint flags)
{
	GList *uids = seahorse_pgp_key_get_uids (self);
	return uids ? seahorse_pgp_uid_calc_markup (seahorse_pgp_uid_get_name (uids->data),
	                                            seahorse_pgp_uid_get_email (uids->data),
	                                            seahorse_pgp_uid_get_comment (uids->data), flags) : "";
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static GList*
_seahorse_pgp_key_get_uids (SeahorsePgpKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), NULL);
	return self->pv->uids;
}

static void
_seahorse_pgp_key_set_uids (SeahorsePgpKey *self, GList *uids)
{
	GList *l;
	
	g_return_if_fail (SEAHORSE_IS_PGP_KEY (self));

	/* Remove the parent on each old one */
	for (l = self->pv->uids; l; l = g_list_next (l)) 
		seahorse_object_set_parent (l->data, NULL);

	seahorse_object_list_free (self->pv->uids);
	self->pv->uids = seahorse_object_list_copy (uids);

	/* Set parent and source on each new one, except the first */
	for (l = self->pv->uids ? g_list_next (self->pv->uids) : NULL;
	     l; l = g_list_next (l))
		seahorse_object_set_parent (l->data, SEAHORSE_OBJECT (self));
	
	g_object_notify (G_OBJECT (self), "uids");
}

static GList*
_seahorse_pgp_key_get_subkeys (SeahorsePgpKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), NULL);
	return self->pv->subkeys;
}

static void
_seahorse_pgp_key_set_subkeys (SeahorsePgpKey *self, GList *subkeys)
{
	GQuark id;
	
	g_return_if_fail (SEAHORSE_IS_PGP_KEY (self));
	g_return_if_fail (subkeys);
	
	seahorse_object_list_free (self->pv->subkeys);
	self->pv->subkeys = seahorse_object_list_copy (subkeys);
	
	id = seahorse_pgp_key_get_cannonical_id (seahorse_pgp_subkey_get_keyid (subkeys->data));
	g_object_set (self, "id", id, NULL); 
	
	g_object_notify (G_OBJECT (self), "subkeys");
}

static GList*
_seahorse_pgp_key_get_photos (SeahorsePgpKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), NULL);
	return self->pv->photos;
}

static void
_seahorse_pgp_key_set_photos (SeahorsePgpKey *self, GList *photos)
{
	g_return_if_fail (SEAHORSE_IS_PGP_KEY (self));
	
	seahorse_object_list_free (self->pv->photos);
	self->pv->photos = seahorse_object_list_copy (photos);
	
	g_object_notify (G_OBJECT (self), "photos");
}

static void
seahorse_pgp_key_realize (SeahorseObject *obj)
{
	SeahorsePgpKey *self = SEAHORSE_PGP_KEY (obj);
	const gchar *identifier, *nickname;
	gchar *markup, *name;
	GList *subkeys;
	
	
	SEAHORSE_OBJECT_CLASS (seahorse_pgp_key_parent_class)->realize (obj);
	
	identifier = "";
	subkeys = seahorse_pgp_key_get_subkeys (self);
	if (subkeys)
		identifier = seahorse_pgp_subkey_get_keyid (subkeys->data);

	name = calc_name (self);
	markup = calc_markup (self, seahorse_object_get_flags (obj));
	nickname = calc_short_name (self);
		
	g_object_set (obj,
		      "label", name,
		      "markup", markup,
		      "nickname", nickname,
		      "identifier", identifier,
		      NULL);
		
	g_free (markup);
	g_free (name);
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
	case PROP_PHOTOS:
		g_value_set_boxed (value, seahorse_pgp_key_get_photos (self));
		break;
	case PROP_SUBKEYS:
		g_value_set_boxed (value, seahorse_pgp_key_get_subkeys (self));
		break;
	case PROP_UIDS:
		g_value_set_boxed (value, seahorse_pgp_key_get_uids (self));
		break;
	case PROP_FINGERPRINT:
		g_value_set_string (value, seahorse_pgp_key_get_fingerprint (self));
		break;
	case PROP_VALIDITY_STR:
		g_value_set_string (value, seahorse_pgp_key_get_validity_str (self));
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
	case PROP_VALIDITY:
	case PROP_TRUST:
		g_warning ("This property %s getter must be overridden in class %s",
		           pspec->name, G_OBJECT_TYPE_NAME (object));
		break;
	}
}

static void
seahorse_pgp_key_set_property (GObject *object, guint prop_id, const GValue *value, 
                               GParamSpec *pspec)
{
	SeahorsePgpKey *self = SEAHORSE_PGP_KEY (object);
    
	switch (prop_id) {
	case PROP_UIDS:
		seahorse_pgp_key_set_uids (self, g_value_get_boxed (value));
		break;
	case PROP_SUBKEYS:
		seahorse_pgp_key_set_subkeys (self, g_value_get_boxed (value));
		break;
	case PROP_PHOTOS:
		seahorse_pgp_key_set_photos (self, g_value_get_boxed (value));
		break;
	}
}

static void
seahorse_pgp_key_object_dispose (GObject *obj)
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
	
	/* Free all the attached Subkeys */
	seahorse_object_list_free (self->pv->subkeys);
	self->pv->subkeys = NULL;

	G_OBJECT_CLASS (seahorse_pgp_key_parent_class)->dispose (obj);
}

static void
seahorse_pgp_key_object_finalize (GObject *obj)
{
	SeahorsePgpKey *self = SEAHORSE_PGP_KEY (obj);

	g_assert (self->pv->uids == NULL);
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
	
	seahorse_class->realize = seahorse_pgp_key_realize;
	
	klass->get_uids = _seahorse_pgp_key_get_uids;
	klass->set_uids = _seahorse_pgp_key_set_uids;
	klass->get_subkeys = _seahorse_pgp_key_get_subkeys;
	klass->set_subkeys = _seahorse_pgp_key_set_subkeys;
	klass->get_photos = _seahorse_pgp_key_get_photos;
	klass->set_photos = _seahorse_pgp_key_set_photos;
	
	g_object_class_install_property (gobject_class, PROP_PHOTOS,
                g_param_spec_boxed ("photos", "Key Photos", "Photos for the key",
                                    SEAHORSE_BOXED_OBJECT_LIST, G_PARAM_READWRITE));
	
	g_object_class_install_property (gobject_class, PROP_SUBKEYS,
                g_param_spec_boxed ("subkeys", "PGP subkeys", "PGP subkeys",
                                    SEAHORSE_BOXED_OBJECT_LIST, G_PARAM_READWRITE));
	
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

SeahorsePgpKey*
seahorse_pgp_key_new (void)
{
	return g_object_new (SEAHORSE_TYPE_PGP_KEY, NULL);
}

GList*
seahorse_pgp_key_get_uids (SeahorsePgpKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), NULL);
	if (!SEAHORSE_PGP_KEY_GET_CLASS (self)->get_uids)
		return NULL;
	return SEAHORSE_PGP_KEY_GET_CLASS (self)->get_uids (self);
}

void
seahorse_pgp_key_set_uids (SeahorsePgpKey *self, GList *uids)
{
	g_return_if_fail (SEAHORSE_IS_PGP_KEY (self));
	g_return_if_fail (SEAHORSE_PGP_KEY_GET_CLASS (self)->set_uids);
	SEAHORSE_PGP_KEY_GET_CLASS (self)->set_uids (self, uids);
}

GList*
seahorse_pgp_key_get_subkeys (SeahorsePgpKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), NULL);
	if (!SEAHORSE_PGP_KEY_GET_CLASS (self)->get_subkeys)
		return NULL;
	return SEAHORSE_PGP_KEY_GET_CLASS (self)->get_subkeys (self);
}

void
seahorse_pgp_key_set_subkeys (SeahorsePgpKey *self, GList *subkeys)
{
	g_return_if_fail (SEAHORSE_IS_PGP_KEY (self));
	g_return_if_fail (SEAHORSE_PGP_KEY_GET_CLASS (self)->set_subkeys);
	SEAHORSE_PGP_KEY_GET_CLASS (self)->set_subkeys (self, subkeys);
}

GList*
seahorse_pgp_key_get_photos (SeahorsePgpKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), NULL);
	if (!SEAHORSE_PGP_KEY_GET_CLASS (self)->get_photos)
		return NULL;
	return SEAHORSE_PGP_KEY_GET_CLASS (self)->get_photos (self);
}

void
seahorse_pgp_key_set_photos (SeahorsePgpKey *self, GList *photos)
{
	g_return_if_fail (SEAHORSE_IS_PGP_KEY (self));
	g_return_if_fail (SEAHORSE_PGP_KEY_GET_CLASS (self)->set_photos);
	SEAHORSE_PGP_KEY_GET_CLASS (self)->set_photos (self, photos);
}

const gchar*
seahorse_pgp_key_get_fingerprint (SeahorsePgpKey *self)
{
	GList *subkeys;
	    
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), NULL);

	subkeys = seahorse_pgp_key_get_subkeys (self);
	if (!subkeys)
		return "";
	
	return seahorse_pgp_subkey_get_fingerprint (subkeys->data);
}

SeahorseValidity
seahorse_pgp_key_get_validity (SeahorsePgpKey *self)
{
	guint validity = SEAHORSE_VALIDITY_UNKNOWN;
	g_object_get (self, "validity", &validity, NULL);
	return validity;
}

const gchar*
seahorse_pgp_key_get_validity_str (SeahorsePgpKey *self)
{
	return seahorse_validity_get_string (seahorse_pgp_key_get_validity (self));
}

gulong
seahorse_pgp_key_get_expires (SeahorsePgpKey *self)
{
	GList *subkeys;
	    
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), 0);

	subkeys = seahorse_pgp_key_get_subkeys (self);
	if (!subkeys)
		return 0;
	
	return seahorse_pgp_subkey_get_expires (subkeys->data);
}

gchar*
seahorse_pgp_key_get_expires_str (SeahorsePgpKey *self)
{
	GTimeVal timeval;
	gulong expires;
	
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), NULL);
	
	expires = seahorse_pgp_key_get_expires (self);
	if (expires == 0)
		return g_strdup ("");
	
	g_get_current_time (&timeval);
	if (timeval.tv_sec > expires)
		return g_strdup (_("Expired"));
	
	return seahorse_util_get_date_string (expires);
}

SeahorseValidity
seahorse_pgp_key_get_trust (SeahorsePgpKey *self)
{
	guint trust = SEAHORSE_VALIDITY_UNKNOWN;
	g_object_get (self, "trust", &trust, NULL);
	return trust;
}

const gchar*
seahorse_pgp_key_get_trust_str (SeahorsePgpKey *self)
{
	return seahorse_validity_get_string (seahorse_pgp_key_get_trust (self));
}

guint
seahorse_pgp_key_get_length (SeahorsePgpKey *self)
{
	GList *subkeys;
	    
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), 0);

	subkeys = seahorse_pgp_key_get_subkeys (self);
	if (!subkeys)
		return 0;
	
	return seahorse_pgp_subkey_get_length (subkeys->data);
}

const gchar*
seahorse_pgp_key_get_algo (SeahorsePgpKey *self)
{
	GList *subkeys;
	    
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), 0);

	subkeys = seahorse_pgp_key_get_subkeys (self);
	if (!subkeys)
		return 0;
	
	return seahorse_pgp_subkey_get_algorithm (subkeys->data);
}

const gchar*
seahorse_pgp_key_get_keyid (SeahorsePgpKey *self)
{
	GList *subkeys;
	    
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), 0);

	subkeys = seahorse_pgp_key_get_subkeys (self);
	if (!subkeys)
		return 0;
	
	return seahorse_pgp_subkey_get_keyid (subkeys->data);
}

gboolean
seahorse_pgp_key_has_keyid (SeahorsePgpKey *self, const gchar *match)
{
	GList *subkeys, *l;
	SeahorsePgpSubkey *subkey;
	const gchar *keyid;
	guint n_match, n_keyid;
	
	g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (self), FALSE);
	g_return_val_if_fail (match, FALSE);

	subkeys = seahorse_pgp_key_get_subkeys (self);
	if (!subkeys)
		return FALSE;
	
	n_match = strlen (match);
	
	for (l = subkeys; l && (subkey = SEAHORSE_PGP_SUBKEY (l->data)); l = g_list_next (l)) {
		keyid = seahorse_pgp_subkey_get_keyid (subkey);
		g_return_val_if_fail (keyid, FALSE);
		n_keyid = strlen (keyid);
		if (n_match <= n_keyid) {
			keyid += (n_keyid - n_match);
			if (strncmp (keyid, match, n_match) == 0)
				return TRUE;
		}
	}
	
	return FALSE;
}

