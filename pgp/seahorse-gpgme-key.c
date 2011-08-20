/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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
#include "seahorse-object-list.h"
#include "seahorse-source.h"
#include "seahorse-gtkstock.h"
#include "seahorse-util.h"

#include "pgp/seahorse-pgp-key.h"
#include "pgp/seahorse-gpgme.h"
#include "pgp/seahorse-gpgme-key-op.h"
#include "pgp/seahorse-gpgme-photo.h"
#include "pgp/seahorse-gpgme-source.h"
#include "pgp/seahorse-gpgme-uid.h"

enum {
	PROP_0,
	PROP_PUBKEY,
	PROP_SECKEY,
	PROP_VALIDITY,
	PROP_TRUST
};

G_DEFINE_TYPE (SeahorseGpgmeKey, seahorse_gpgme_key, SEAHORSE_TYPE_PGP_KEY);

struct _SeahorseGpgmeKeyPrivate {
	gpgme_key_t pubkey;   		/* The public key */
	gpgme_key_t seckey;      	/* The secret key */
	gboolean has_secret;		/* Whether we have a secret key or not */

	GList *uids;			/* We keep a copy of the uids. */

	int list_mode;                  /* What to load our public key as */
	gboolean photos_loaded;		/* Photos were loaded */
	
	gint block_loading;        	/* Loading is blocked while this flag is set */
};

/* -----------------------------------------------------------------------------
 * INTERNAL HELPERS
 */

static gboolean 
load_gpgme_key (GQuark id, int mode, int secret, gpgme_key_t *key)
{
	GError *error = NULL;
	gpgme_ctx_t ctx;
	gpgme_error_t gerr;
	
	ctx = seahorse_gpgme_source_new_context ();
	gpgme_set_keylist_mode (ctx, mode);
	gerr = gpgme_op_keylist_start (ctx, seahorse_pgp_key_calc_rawid (id), secret);
	if (GPG_IS_OK (gerr)) {
		gerr = gpgme_op_keylist_next (ctx, key);
		gpgme_op_keylist_end (ctx);
	}

	gpgme_release (ctx);

	if (seahorse_gpgme_propagate_error (gerr, &error)) {
		g_message ("couldn't load GPGME key: %s", error->message);
		g_clear_error (&error);
		return FALSE;
	}
	
	return TRUE;
}

static void
load_key_public (SeahorseGpgmeKey *self, int list_mode)
{
	gpgme_key_t key = NULL;
	gboolean ret;
	GQuark id;
	
	if (self->pv->block_loading)
		return;
	
	list_mode |= self->pv->list_mode;
	
	id = seahorse_object_get_id (SEAHORSE_OBJECT (self));
	g_return_if_fail (id);
	
	ret = load_gpgme_key (id, list_mode, FALSE, &key);
	if (ret) {
		self->pv->list_mode = list_mode;
		seahorse_gpgme_key_set_public (self, key);
		gpgme_key_unref (key);
	}
}

static gboolean
require_key_public (SeahorseGpgmeKey *self, int list_mode)
{
	if (!self->pv->pubkey || (self->pv->list_mode & list_mode) != list_mode)
		load_key_public (self, list_mode);
	return self->pv->pubkey && (self->pv->list_mode & list_mode) == list_mode;
}

static void
load_key_private (SeahorseGpgmeKey *self)
{
	gpgme_key_t key = NULL;
	gboolean ret;
	GQuark id;
	
	if (!self->pv->has_secret || self->pv->block_loading)
		return;
	
	id = seahorse_object_get_id (SEAHORSE_OBJECT (self));
	g_return_if_fail (id);
	
	ret = load_gpgme_key (id, GPGME_KEYLIST_MODE_LOCAL, TRUE, &key);
	if (ret) {
		seahorse_gpgme_key_set_private (self, key);
		gpgme_key_unref (key);
	}
}

static gboolean
require_key_private (SeahorseGpgmeKey *self)
{
	if (!self->pv->seckey)
		load_key_private (self);
	return self->pv->seckey != NULL;
}

static gboolean
require_key_uids (SeahorseGpgmeKey *self)
{
	return require_key_public (self, GPGME_KEYLIST_MODE_LOCAL | GPGME_KEYLIST_MODE_SIGS);
}

static gboolean
require_key_subkeys (SeahorseGpgmeKey *self)
{
	return require_key_public (self, GPGME_KEYLIST_MODE_LOCAL);
}

static void
load_key_photos (SeahorseGpgmeKey *self)
{
	gpgme_error_t gerr;
	
	if (self->pv->block_loading)
		return;

	gerr = seahorse_gpgme_key_op_photos_load (self);
	if (!GPG_IS_OK (gerr)) 
		g_message ("couldn't load key photos: %s", gpgme_strerror (gerr));
	else
		self->pv->photos_loaded = TRUE;
}

static gboolean
require_key_photos (SeahorseGpgmeKey *self)
{
	if (!self->pv->photos_loaded)
		load_key_photos (self);
	return self->pv->photos_loaded;
}

static void
renumber_actual_uids (SeahorseGpgmeKey *self)
{
	GArray *index_map;
	GList *photos, *uids, *l;
	guint index, i;
	
	g_assert (SEAHORSE_IS_GPGME_KEY (self));
	
	/* 
	 * This function is necessary because the uid stored in a gpgme_user_id_t
	 * struct is only usable with gpgme functions.  Problems will be caused if 
	 * that uid is used with functions found in seahorse-gpgme-key-op.h.  This 
	 * function is only to be called with uids from gpgme_user_id_t structs.
	 */

	++self->pv->block_loading;
	photos = seahorse_pgp_key_get_photos (SEAHORSE_PGP_KEY (self));
	uids = self->pv->uids;
	--self->pv->block_loading;
	
	/* First we build a bitmap of where all the photo uid indexes are */
	index_map = g_array_new (FALSE, TRUE, sizeof (gboolean));
	for (l = photos; l; l = g_list_next (l)) {
		index = seahorse_gpgme_photo_get_index (l->data);
		if (index >= index_map->len)
			g_array_set_size (index_map, index + 1);
		g_array_index (index_map, gboolean, index) = TRUE;
	}
	
	/* Now for each UID we add however many photo indexes are below the gpgme index */
	for (l = uids; l; l = g_list_next (l)) {
		index = seahorse_gpgme_uid_get_gpgme_index (l->data);
		for (i = 0; i < index_map->len && i < index; ++i) {
			if(g_array_index (index_map, gboolean, index))
				++index;
		}
		seahorse_gpgme_uid_set_actual_index (l->data, index + 1);
	}
	
	g_array_free (index_map, TRUE);
}

static void
realize_uids (SeahorseGpgmeKey *self)
{
	gpgme_user_id_t guid;
	SeahorseGpgmeUid *uid;
	GList *results = NULL;
	gboolean changed = FALSE;
	GList *uids;

	uids = self->pv->uids;
	guid = self->pv->pubkey ? self->pv->pubkey->uids : NULL;

	/* Look for out of sync or missing UIDs */
	while (uids != NULL) {
		g_return_if_fail (SEAHORSE_IS_GPGME_UID (uids->data));
		uid = SEAHORSE_GPGME_UID (uids->data);
		uids = g_list_next (uids);

		/* Bring this UID up to date */
		if (guid && seahorse_gpgme_uid_is_same (uid, guid)) {
			if (seahorse_gpgme_uid_get_userid (uid) != guid) {
				g_object_set (uid, "pubkey", self->pv->pubkey, "userid", guid, NULL);
				changed = TRUE;
			}
			results = seahorse_object_list_append (results, uid);
			guid = guid->next;
		}
	}

	/* Add new UIDs */
	while (guid != NULL) {
		uid = seahorse_gpgme_uid_new (self->pv->pubkey, guid);
		changed = TRUE;
		results = seahorse_object_list_append (results, uid);
		g_object_unref (uid);
		guid = guid->next;
	}

	if (changed)
		seahorse_pgp_key_set_uids (SEAHORSE_PGP_KEY (self), results);
	seahorse_object_list_free (results);
}

static void 
realize_subkeys (SeahorseGpgmeKey *self)
{
	gpgme_subkey_t gsubkey;
	SeahorseGpgmeSubkey *subkey;
	GList *list = NULL;
	
	if (self->pv->pubkey) {

		for (gsubkey = self->pv->pubkey->subkeys; gsubkey; gsubkey = gsubkey->next) {
			subkey = seahorse_gpgme_subkey_new (self->pv->pubkey, gsubkey);
			list = seahorse_object_list_prepend (list, subkey);
			g_object_unref (subkey);
		}
		
		list = g_list_reverse (list);
	}

	seahorse_pgp_key_set_subkeys (SEAHORSE_PGP_KEY (self), list);
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
seahorse_gpgme_key_realize (SeahorseObject *obj)
{
	SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (obj);
	SeahorseUsage usage;
	guint flags;
	
	if (!self->pv->pubkey)
		return;
	
	if (!require_key_public (self, GPGME_KEYLIST_MODE_LOCAL))
		g_return_if_reached ();
	
	g_return_if_fail (self->pv->pubkey);
	g_return_if_fail (self->pv->pubkey->subkeys);

	/* Update the sub UIDs */
	realize_uids (self);
	realize_subkeys (self);

	/* The flags */
	flags = SEAHORSE_FLAG_EXPORTABLE | SEAHORSE_FLAG_DELETABLE;

	if (!self->pv->pubkey->disabled && !self->pv->pubkey->expired && 
	    !self->pv->pubkey->revoked && !self->pv->pubkey->invalid) {
		if (seahorse_gpgme_key_get_validity (self) >= SEAHORSE_VALIDITY_MARGINAL)
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

	if (seahorse_gpgme_key_get_trust (self) >= SEAHORSE_VALIDITY_MARGINAL && 
	    !self->pv->pubkey->revoked && !self->pv->pubkey->disabled && 
	    !self->pv->pubkey->expired)
		flags |= SEAHORSE_FLAG_TRUSTED;

	g_object_set (obj, "flags", flags, NULL);
	
	/* The type */
	if (self->pv->seckey)
		usage = SEAHORSE_USAGE_PRIVATE_KEY;
	else
		usage = SEAHORSE_USAGE_PUBLIC_KEY;

	g_object_set (obj, "usage", usage, NULL);
	
	++self->pv->block_loading;
	SEAHORSE_OBJECT_CLASS (seahorse_gpgme_key_parent_class)->realize (obj);
	--self->pv->block_loading;
}

static void
seahorse_gpgme_key_refresh (SeahorseObject *obj)
{
	SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (obj);
	
	if (self->pv->pubkey)
		load_key_public (self, self->pv->list_mode);
	if (self->pv->seckey)
		load_key_private (self);
	if (self->pv->photos_loaded)
		load_key_photos (self);

	SEAHORSE_OBJECT_CLASS (seahorse_gpgme_key_parent_class)->refresh (obj);
}

static GList*
seahorse_gpgme_key_get_uids (SeahorsePgpKey *base)
{
	SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (base);
	require_key_uids (self);
	return SEAHORSE_PGP_KEY_CLASS (seahorse_gpgme_key_parent_class)->get_uids (base);
}

static void
seahorse_gpgme_key_set_uids (SeahorsePgpKey *base, GList *uids)
{
	SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (base);
	GList *l;
	
	SEAHORSE_PGP_KEY_CLASS (seahorse_gpgme_key_parent_class)->set_uids (base, uids);
	
	/* Remove the parent on each old one */
	for (l = self->pv->uids; l; l = g_list_next (l))
		seahorse_context_remove_object (seahorse_context_instance (), l->data);

	/* Keep our own copy of the UID list */
	seahorse_object_list_free (self->pv->uids);
	self->pv->uids = seahorse_object_list_copy (uids);
	
	/* Add UIDS to context so that they show up */
	for (l = self->pv->uids; l; l = g_list_next (l))
		seahorse_context_add_object (seahorse_context_instance (), l->data);

	renumber_actual_uids (self);
}

static GList*
seahorse_gpgme_key_get_subkeys (SeahorsePgpKey *base)
{
	SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (base);
	require_key_subkeys (self);
	return SEAHORSE_PGP_KEY_CLASS (seahorse_gpgme_key_parent_class)->get_subkeys (base);
}

static GList*
seahorse_gpgme_key_get_photos (SeahorsePgpKey *base)
{
	SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (base);
	require_key_photos (self);
	return SEAHORSE_PGP_KEY_CLASS (seahorse_gpgme_key_parent_class)->get_photos (base);
}

static void
seahorse_gpgme_key_set_photos (SeahorsePgpKey *base, GList *photos)
{
	SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (base);
	self->pv->photos_loaded = TRUE;
	SEAHORSE_PGP_KEY_CLASS (seahorse_gpgme_key_parent_class)->set_photos (base, photos);
	renumber_actual_uids (self);
}

static void
seahorse_gpgme_key_init (SeahorseGpgmeKey *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_GPGME_KEY, SeahorseGpgmeKeyPrivate);
	g_object_set (self, "location", SEAHORSE_LOCATION_LOCAL, NULL);
}

static void
seahorse_gpgme_key_get_property (GObject *object, guint prop_id,
                               GValue *value, GParamSpec *pspec)
{
	SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (object);
    
	switch (prop_id) {
	case PROP_PUBKEY:
		g_value_set_boxed (value, seahorse_gpgme_key_get_public (self));
		break;
	case PROP_SECKEY:
		g_value_set_boxed (value, seahorse_gpgme_key_get_private (self));
		break;
	case PROP_VALIDITY:
		g_value_set_uint (value, seahorse_gpgme_key_get_validity (self));
		break;
	case PROP_TRUST:
		g_value_set_uint (value, seahorse_gpgme_key_get_trust (self));
		break;
	}
}

static void
seahorse_gpgme_key_set_property (GObject *object, guint prop_id, const GValue *value, 
                               GParamSpec *pspec)
{
	SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (object);

	switch (prop_id) {
	case PROP_PUBKEY:
		seahorse_gpgme_key_set_public (self, g_value_get_boxed (value));
		break;
	case PROP_SECKEY:
		seahorse_gpgme_key_set_private (self, g_value_get_boxed (value));
		break;
	}
}

static void
seahorse_gpgme_key_object_dispose (GObject *obj)
{
	SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (obj);
	GList *l;
	
	/* Remove the attached UIDs */
	for (l = self->pv->uids; l; l = g_list_next (l))
		seahorse_context_remove_object (seahorse_context_instance (), l->data);

	if (self->pv->pubkey)
		gpgme_key_unref (self->pv->pubkey);
	if (self->pv->seckey)
		gpgme_key_unref (self->pv->seckey);
	self->pv->pubkey = self->pv->seckey = NULL;
	
	seahorse_object_list_free (self->pv->uids);
	self->pv->uids = NULL;

	G_OBJECT_CLASS (seahorse_gpgme_key_parent_class)->dispose (obj);
}

static void
seahorse_gpgme_key_object_finalize (GObject *obj)
{
	SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (obj);

	g_assert (self->pv->pubkey == NULL);
	g_assert (self->pv->seckey == NULL);
	g_assert (self->pv->uids == NULL);
    
	G_OBJECT_CLASS (seahorse_gpgme_key_parent_class)->finalize (obj);
}

static void
seahorse_gpgme_key_class_init (SeahorseGpgmeKeyClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseObjectClass *seahorse_class = SEAHORSE_OBJECT_CLASS (klass);
	SeahorsePgpKeyClass *pgp_class = SEAHORSE_PGP_KEY_CLASS (klass);
	
	seahorse_gpgme_key_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseGpgmeKeyPrivate));

	gobject_class->dispose = seahorse_gpgme_key_object_dispose;
	gobject_class->finalize = seahorse_gpgme_key_object_finalize;
	gobject_class->set_property = seahorse_gpgme_key_set_property;
	gobject_class->get_property = seahorse_gpgme_key_get_property;
	
	seahorse_class->refresh = seahorse_gpgme_key_refresh;
	seahorse_class->realize = seahorse_gpgme_key_realize;

	pgp_class->get_uids = seahorse_gpgme_key_get_uids;
	pgp_class->set_uids = seahorse_gpgme_key_set_uids;
	pgp_class->get_subkeys = seahorse_gpgme_key_get_subkeys;
	pgp_class->get_photos = seahorse_gpgme_key_get_photos;
	pgp_class->set_photos = seahorse_gpgme_key_set_photos;
	
	g_object_class_install_property (gobject_class, PROP_PUBKEY,
	        g_param_spec_boxed ("pubkey", "GPGME Public Key", "GPGME Public Key that this object represents",
	                            SEAHORSE_GPGME_BOXED_KEY, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_SECKEY,
                g_param_spec_boxed ("seckey", "GPGME Secret Key", "GPGME Secret Key that this object represents",
                                    SEAHORSE_GPGME_BOXED_KEY, G_PARAM_READWRITE));

	g_object_class_override_property (gobject_class, PROP_VALIDITY, "validity");

        g_object_class_override_property (gobject_class, PROP_TRUST, "trust");
}


/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseGpgmeKey* 
seahorse_gpgme_key_new (SeahorseSource *sksrc, gpgme_key_t pubkey, 
                        gpgme_key_t seckey)
{
	const gchar *keyid;

	g_return_val_if_fail (pubkey || seckey, NULL);
	
	if (pubkey != NULL)
		keyid = pubkey->subkeys->keyid;
	else
		keyid = seckey->subkeys->keyid;
	
	return g_object_new (SEAHORSE_TYPE_GPGME_KEY, "source", sksrc,
	                     "id", seahorse_pgp_key_canonize_id (keyid),
	                     "pubkey", pubkey, "seckey", seckey, 
	                     NULL);
}

gpgme_key_t
seahorse_gpgme_key_get_public (SeahorseGpgmeKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GPGME_KEY (self), NULL);
	if (require_key_public (self, GPGME_KEYLIST_MODE_LOCAL))
		return self->pv->pubkey;
	return NULL;
}

void
seahorse_gpgme_key_set_public (SeahorseGpgmeKey *self, gpgme_key_t key)
{
	GObject *obj;
	
	g_return_if_fail (SEAHORSE_IS_GPGME_KEY (self));
	
	if (self->pv->pubkey)
		gpgme_key_unref (self->pv->pubkey);
	self->pv->pubkey = key;
	if (self->pv->pubkey) {
		gpgme_key_ref (self->pv->pubkey);
		self->pv->list_mode |= self->pv->pubkey->keylist_mode;
	}
	
	obj = G_OBJECT (self);
	g_object_freeze_notify (obj);
	seahorse_gpgme_key_realize (SEAHORSE_OBJECT (self));
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
seahorse_gpgme_key_get_private (SeahorseGpgmeKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GPGME_KEY (self), NULL);
	if (require_key_private (self))
		return self->pv->seckey;
	return NULL;	
}

void
seahorse_gpgme_key_set_private (SeahorseGpgmeKey *self, gpgme_key_t key)
{
	GObject *obj;
	
	g_return_if_fail (SEAHORSE_IS_GPGME_KEY (self));
	
	if (self->pv->seckey)
		gpgme_key_unref (self->pv->seckey);
	self->pv->seckey = key;
	if (self->pv->seckey)
		gpgme_key_ref (self->pv->seckey);
	
	obj = G_OBJECT (self);
	g_object_freeze_notify (obj);
	seahorse_gpgme_key_realize (SEAHORSE_OBJECT (self));
	g_object_thaw_notify (obj);
}

SeahorseValidity
seahorse_gpgme_key_get_validity (SeahorseGpgmeKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GPGME_KEY (self), SEAHORSE_VALIDITY_UNKNOWN);
	
	if (!require_key_public (self, GPGME_KEYLIST_MODE_LOCAL))
		return SEAHORSE_VALIDITY_UNKNOWN;
	
	g_return_val_if_fail (self->pv->pubkey, SEAHORSE_VALIDITY_UNKNOWN);
	g_return_val_if_fail (self->pv->pubkey->uids, SEAHORSE_VALIDITY_UNKNOWN);
	
	if (self->pv->pubkey->revoked)
		return SEAHORSE_VALIDITY_REVOKED;
	if (self->pv->pubkey->disabled)
		return SEAHORSE_VALIDITY_DISABLED;
	return seahorse_gpgme_convert_validity (self->pv->pubkey->uids->validity);
}

SeahorseValidity
seahorse_gpgme_key_get_trust (SeahorseGpgmeKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GPGME_KEY (self), SEAHORSE_VALIDITY_UNKNOWN);
	if (!require_key_public (self, GPGME_KEYLIST_MODE_LOCAL))
		return SEAHORSE_VALIDITY_UNKNOWN;
	
	return seahorse_gpgme_convert_validity (self->pv->pubkey->owner_trust);
}

void
seahorse_gpgme_key_refresh_matching (gpgme_key_t key)
{
	SeahorseObjectPredicate pred;
	
	g_return_if_fail (key->subkeys->keyid);
	
	memset (&pred, 0, sizeof (pred));
	pred.type = SEAHORSE_TYPE_GPGME_KEY;
	pred.id = seahorse_pgp_key_canonize_id (key->subkeys->keyid);
	
	seahorse_context_for_objects_full (NULL, &pred, refresh_each_object, NULL);
}
