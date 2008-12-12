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
#include "seahorse-gpgmex.h"
#include "seahorse-pgp-subkey.h"
#include "seahorse-pgp-uid.h"

#include <string.h>

#include <glib/gi18n.h>

enum {
	PROP_0,
	PROP_PUBKEY,
	PROP_SUBKEY,
	PROP_INDEX,
	PROP_KEYID,
	PROP_ALGORITHM,
	PROP_EXPIRES
};

G_DEFINE_TYPE (SeahorsePgpSubkey, seahorse_pgp_subkey, G_TYPE_OBJECT);

struct _SeahorsePgpSubkeyPrivate {
	gpgme_key_t pubkey;         /* The public key that this subkey is part of */
	gpgme_subkey_t subkey;      /* The subkey referred to */
	guint index;                /* The GPGME index of the subkey */
};

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_pgp_subkey_init (SeahorsePgpSubkey *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_PGP_SUBKEY, SeahorsePgpSubkeyPrivate);
	self->pv->index = 0;
}

static GObject*
seahorse_pgp_subkey_constructor (GType type, guint n_props, GObjectConstructParam *props)
{
	GObject *obj = G_OBJECT_CLASS (seahorse_pgp_subkey_parent_class)->constructor (type, n_props, props);
	SeahorsePgpSubkey *self = NULL;
	
	if (obj) {
		self = SEAHORSE_PGP_SUBKEY (obj);
		g_return_val_if_fail (self->pv->pubkey, NULL);
	}
	
	return obj;
}

static void
seahorse_pgp_subkey_get_property (GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec)
{
	SeahorsePgpSubkey *self = SEAHORSE_PGP_SUBKEY (object);
	
	switch (prop_id) {
	case PROP_PUBKEY:
		g_value_set_boxed (value, seahorse_pgp_subkey_get_pubkey (self));
		break;
	case PROP_SUBKEY:
		g_value_set_pointer (value, seahorse_pgp_subkey_get_subkey (self));
		break;
	case PROP_INDEX:
		g_value_set_uint (value, seahorse_pgp_subkey_get_index (self));
		break;
	case PROP_KEYID:
		g_value_set_string (value, seahorse_pgp_subkey_get_keyid (self));
		break;
	case PROP_ALGORITHM:
		g_value_set_string (value, seahorse_pgp_subkey_get_algorithm (self));
		break;
	case PROP_EXPIRES:
		g_value_set_ulong (value, seahorse_pgp_subkey_get_expires (self));
		break;
	}
}

static void
seahorse_pgp_subkey_set_property (GObject *object, guint prop_id, const GValue *value, 
                                  GParamSpec *pspec)
{
	SeahorsePgpSubkey *self = SEAHORSE_PGP_SUBKEY (object);

	switch (prop_id) {
	case PROP_PUBKEY:
		g_return_if_fail (!self->pv->pubkey);
		self->pv->pubkey = g_value_get_boxed (value);
		if (self->pv->pubkey)
			gpgmex_key_ref (self->pv->pubkey);
		break;
	case PROP_SUBKEY:
		seahorse_pgp_subkey_set_subkey (self, g_value_get_pointer (value));
		break;
	}
}

static void
seahorse_pgp_subkey_finalize (GObject *gobject)
{
	SeahorsePgpSubkey *self = SEAHORSE_PGP_SUBKEY (gobject);

	/* Unref the key */
	if (self->pv->pubkey)
		gpgmex_key_unref (self->pv->pubkey);
	self->pv->pubkey = NULL;
	self->pv->subkey = NULL;
    
	G_OBJECT_CLASS (seahorse_pgp_subkey_parent_class)->finalize (gobject);
}

static void
seahorse_pgp_subkey_class_init (SeahorsePgpSubkeyClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);    

	seahorse_pgp_subkey_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorsePgpSubkeyPrivate));

	gobject_class->constructor = seahorse_pgp_subkey_constructor;
	gobject_class->finalize = seahorse_pgp_subkey_finalize;
	gobject_class->set_property = seahorse_pgp_subkey_set_property;
	gobject_class->get_property = seahorse_pgp_subkey_get_property;
    
	g_object_class_install_property (gobject_class, PROP_PUBKEY,
	        g_param_spec_boxed ("pubkey", "Public Key", "GPGME Public Key that this subkey is on",
	                            SEAHORSE_PGP_BOXED_KEY, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_SUBKEY,
	        g_param_spec_pointer ("subkey", "Subkey", "GPGME Subkey",
	                              G_PARAM_READWRITE));
                      
	g_object_class_install_property (gobject_class, PROP_INDEX,
	        g_param_spec_uint ("index", "GPGME Index", "GPGME Subkey Index",
	                           0, G_MAXUINT, 0, G_PARAM_READWRITE));
	
        g_object_class_install_property (gobject_class, PROP_KEYID,
                g_param_spec_string ("keyid", "Key ID", "GPG Key ID",
                                     "", G_PARAM_READABLE));
        
        g_object_class_install_property (gobject_class, PROP_ALGORITHM,
                g_param_spec_string ("algorithm", "Algorithm", "GPG Algorithm",
                                     "", G_PARAM_READABLE));
        
        g_object_class_install_property (gobject_class, PROP_EXPIRES,
                g_param_spec_ulong ("expires", "Expires On", "Date this key expires on",
                                    0, G_MAXULONG, 0, G_PARAM_READABLE));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorsePgpSubkey* 
seahorse_pgp_subkey_new (gpgme_key_t pubkey, gpgme_subkey_t subkey) 
{
	return g_object_new (SEAHORSE_TYPE_PGP_SUBKEY, 
	                     "pubkey", pubkey, 
	                     "subkey", subkey, NULL);
}


gpgme_key_t
seahorse_pgp_subkey_get_pubkey (SeahorsePgpSubkey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_SUBKEY (self), NULL);
	g_return_val_if_fail (self->pv->pubkey, NULL);
	return self->pv->pubkey;
}

gpgme_subkey_t
seahorse_pgp_subkey_get_subkey (SeahorsePgpSubkey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_SUBKEY (self), NULL);
	g_return_val_if_fail (self->pv->subkey, NULL);
	return self->pv->subkey;
}

void
seahorse_pgp_subkey_set_subkey (SeahorsePgpSubkey *self, gpgme_subkey_t subkey)
{
	GObject *obj;
	gpgme_subkey_t sub;
	gint i, index;
	
	g_return_if_fail (SEAHORSE_IS_PGP_SUBKEY (self));
	g_return_if_fail (subkey);
	
	/* Make sure that this userid is in the pubkey */
	index = -1;
	for (i = 0, sub = self->pv->pubkey->subkeys; sub; ++i, sub = sub->next) {
		if(sub == subkey) {
			index = i;
			break;
		}
	}
	
	g_return_if_fail (index >= 0);
	
	self->pv->subkey = subkey;
	self->pv->index = index;
	
	obj = G_OBJECT (self);
	g_object_freeze_notify (obj);
	g_object_notify (obj, "subkey");
	g_object_notify (obj, "index");
	g_object_notify (obj, "keyid");
	g_object_notify (obj, "algorithm");
	g_object_notify (obj, "expires");
	g_object_thaw_notify (obj);
}

guint
seahorse_pgp_subkey_get_index (SeahorsePgpSubkey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_SUBKEY (self), 0);
	return self->pv->index;
}

const gchar*
seahorse_pgp_subkey_get_keyid (SeahorsePgpSubkey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_SUBKEY (self), NULL);
	g_return_val_if_fail (self->pv->subkey, NULL);
	return self->pv->subkey->keyid;
}

const gchar*
seahorse_pgp_subkey_get_algorithm (SeahorsePgpSubkey *self)
{
	const gchar* algo_type;

	g_return_val_if_fail (SEAHORSE_IS_PGP_SUBKEY (self), NULL);
	g_return_val_if_fail (self->pv->subkey, NULL);
	
	algo_type = gpgme_pubkey_algo_name (self->pv->subkey->pubkey_algo);

	if (algo_type == NULL)
		algo_type = _("Unknown");
	else if (g_str_equal ("Elg", algo_type) || g_str_equal("ELG-E", algo_type))
		algo_type = _("ElGamal");
	
	return algo_type;
}

gulong
seahorse_pgp_subkey_get_expires (SeahorsePgpSubkey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_SUBKEY (self), 0);
	g_return_val_if_fail (self->pv->subkey, 0);
	return self->pv->subkey->expires;
}

gchar*
seahorse_pgp_subkey_get_description (SeahorsePgpSubkey *self)
{
	gchar *label;
	gchar *description;
	
	g_return_val_if_fail (SEAHORSE_IS_PGP_SUBKEY (self), NULL);
	g_return_val_if_fail (self->pv->pubkey, NULL);
	
	if (self->pv->pubkey->uids)
		label = seahorse_pgp_uid_calc_name (self->pv->pubkey->uids);
	else
		label = g_strdup (_("Key"));
	
	if (self->pv->index == 0)
		return label;
	
	description = g_strdup_printf (_("Subkey %d of %s"), self->pv->index, label);
	g_free (label);
	
	return description;
}
