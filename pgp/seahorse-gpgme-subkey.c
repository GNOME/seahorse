/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

#include "seahorse-pgp.h"
#include "seahorse-gpgme.h"
#include "seahorse-gpgme-subkey.h"
#include "seahorse-gpgme-uid.h"

#include <string.h>

#include <glib/gi18n.h>

enum {
	PROP_0,
	PROP_PUBKEY,
	PROP_SUBKEY
};

G_DEFINE_TYPE (SeahorseGpgmeSubkey, seahorse_gpgme_subkey, SEAHORSE_TYPE_PGP_SUBKEY);

struct _SeahorseGpgmeSubkeyPrivate {
	gpgme_key_t pubkey;         /* The public key that this subkey is part of */
	gpgme_subkey_t subkey;      /* The subkey referred to */
};

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_gpgme_subkey_init (SeahorseGpgmeSubkey *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_GPGME_SUBKEY, SeahorseGpgmeSubkeyPrivate);
}

static GObject*
seahorse_gpgme_subkey_constructor (GType type, guint n_props, GObjectConstructParam *props)
{
	GObject *obj = G_OBJECT_CLASS (seahorse_gpgme_subkey_parent_class)->constructor (type, n_props, props);
	SeahorseGpgmeSubkey *self = NULL;
	
	if (obj) {
		self = SEAHORSE_GPGME_SUBKEY (obj);
		g_return_val_if_fail (self->pv->pubkey, NULL);
	}
	
	return obj;
}

static void
seahorse_gpgme_subkey_get_property (GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec)
{
	SeahorseGpgmeSubkey *self = SEAHORSE_GPGME_SUBKEY (object);
	
	switch (prop_id) {
	case PROP_PUBKEY:
		g_value_set_boxed (value, seahorse_gpgme_subkey_get_pubkey (self));
		break;
	case PROP_SUBKEY:
		g_value_set_pointer (value, seahorse_gpgme_subkey_get_subkey (self));
		break;
	}
}

static void
seahorse_gpgme_subkey_set_property (GObject *object, guint prop_id, const GValue *value, 
                                  GParamSpec *pspec)
{
	SeahorseGpgmeSubkey *self = SEAHORSE_GPGME_SUBKEY (object);

	switch (prop_id) {
	case PROP_PUBKEY:
		g_return_if_fail (!self->pv->pubkey);
		self->pv->pubkey = g_value_get_boxed (value);
		if (self->pv->pubkey)
			gpgme_key_ref (self->pv->pubkey);
		break;
	case PROP_SUBKEY:
		seahorse_gpgme_subkey_set_subkey (self, g_value_get_pointer (value));
		break;
	}
}

static void
seahorse_gpgme_subkey_finalize (GObject *gobject)
{
	SeahorseGpgmeSubkey *self = SEAHORSE_GPGME_SUBKEY (gobject);

	/* Unref the key */
	if (self->pv->pubkey)
		gpgme_key_unref (self->pv->pubkey);
	self->pv->pubkey = NULL;
	self->pv->subkey = NULL;
    
	G_OBJECT_CLASS (seahorse_gpgme_subkey_parent_class)->finalize (gobject);
}

static void
seahorse_gpgme_subkey_class_init (SeahorseGpgmeSubkeyClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);    

	seahorse_gpgme_subkey_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseGpgmeSubkeyPrivate));

	gobject_class->constructor = seahorse_gpgme_subkey_constructor;
	gobject_class->finalize = seahorse_gpgme_subkey_finalize;
	gobject_class->set_property = seahorse_gpgme_subkey_set_property;
	gobject_class->get_property = seahorse_gpgme_subkey_get_property;
    
	g_object_class_install_property (gobject_class, PROP_PUBKEY,
	        g_param_spec_boxed ("pubkey", "Public Key", "GPGME Public Key that this subkey is on",
	                            SEAHORSE_GPGME_BOXED_KEY, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_SUBKEY,
	        g_param_spec_pointer ("subkey", "Subkey", "GPGME Subkey",
	                              G_PARAM_READWRITE));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseGpgmeSubkey* 
seahorse_gpgme_subkey_new (gpgme_key_t pubkey, gpgme_subkey_t subkey) 
{
	return g_object_new (SEAHORSE_TYPE_GPGME_SUBKEY, 
	                     "pubkey", pubkey, 
	                     "subkey", subkey, NULL);
}


gpgme_key_t
seahorse_gpgme_subkey_get_pubkey (SeahorseGpgmeSubkey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GPGME_SUBKEY (self), NULL);
	g_return_val_if_fail (self->pv->pubkey, NULL);
	return self->pv->pubkey;
}

gpgme_subkey_t
seahorse_gpgme_subkey_get_subkey (SeahorseGpgmeSubkey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GPGME_SUBKEY (self), NULL);
	g_return_val_if_fail (self->pv->subkey, NULL);
	return self->pv->subkey;
}

void
seahorse_gpgme_subkey_set_subkey (SeahorseGpgmeSubkey *self, gpgme_subkey_t subkey)
{
	gchar *description, *fingerprint, *name;
	SeahorsePgpSubkey *base;
	const gchar *algo_type;
	GObject *obj;
	gpgme_subkey_t sub;
	gint i, index;
	
	g_return_if_fail (SEAHORSE_IS_GPGME_SUBKEY (self));
	g_return_if_fail (subkey);
	
	/* Make sure that this subkey is in the pubkey */
	index = -1;
	for (i = 0, sub = self->pv->pubkey->subkeys; sub; ++i, sub = sub->next) {
		if(sub == subkey) {
			index = i;
			break;
		}
	}
	g_return_if_fail (index >= 0);
	
	/* Calculate the algorithm */
	algo_type = gpgme_pubkey_algo_name (subkey->pubkey_algo);
	if (algo_type == NULL)
		algo_type = _("Unknown");
	else if (g_str_equal ("Elg", algo_type) || g_str_equal("ELG-E", algo_type))
		algo_type = _("ElGamal");

	/* Additional properties */
	fingerprint = seahorse_pgp_subkey_calc_fingerprint (subkey->fpr);
	name = seahorse_gpgme_uid_calc_name (self->pv->pubkey->uids);
	description = seahorse_pgp_subkey_calc_description (name, index);
	
	self->pv->subkey = subkey;
	
	obj = G_OBJECT (self);
	g_object_freeze_notify (obj);
	
	base = SEAHORSE_PGP_SUBKEY (self);
	seahorse_pgp_subkey_set_index (base, index);
	seahorse_pgp_subkey_set_keyid (base, subkey->keyid);
	seahorse_pgp_subkey_set_algorithm (base, algo_type);
	seahorse_pgp_subkey_set_length (base, subkey->length);
	seahorse_pgp_subkey_set_description (base, description);
	seahorse_pgp_subkey_set_fingerprint (base, fingerprint);
	
	g_object_notify (obj, "subkey");
	g_object_thaw_notify (obj);
	
	g_free (description);
	g_free (name);
	g_free (fingerprint);
}
