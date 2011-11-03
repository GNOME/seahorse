/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "config.h"

#include "seahorse-private-key.h"
#include "seahorse-pkcs11.h"
#include "seahorse-pkcs11-actions.h"
#include "seahorse-pkcs11-helpers.h"
#include "seahorse-token.h"
#include "seahorse-types.h"

#include "seahorse-util.h"

#include <gcr/gcr.h>
#include <gck/pkcs11.h>

#include <glib/gi18n-lib.h>

static const gulong REQUIRED_ATTRS[] = {
	CKA_MODULUS_BITS,
	CKA_ID,
	CKA_ID,
	CKA_LABEL,
	CKA_CLASS,
	CKA_KEY_TYPE,
};

enum {
	PROP_0,
	PROP_PLACE,
	PROP_ATTRIBUTES,
	PROP_FLAGS,
	PROP_ACTIONS,
	PROP_CERTIFICATE,
};

struct _SeahorsePrivateKeyPrivate {
	SeahorseToken *token;
	GckAttributes *attributes;
	GtkActionGroup *actions;
	SeahorseCertificate *certificate;
};

static void seahorse_private_key_object_attributes_iface (GckObjectAttributesIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorsePrivateKey, seahorse_private_key, GCK_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCK_TYPE_OBJECT_ATTRIBUTES, seahorse_private_key_object_attributes_iface)
);

static void
seahorse_private_key_init (SeahorsePrivateKey *self)
{
	self->pv = (G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_PRIVATE_KEY, SeahorsePrivateKeyPrivate));
	self->pv->actions = seahorse_pkcs11_object_actions_instance ();
}

static void
seahorse_private_key_finalize (GObject *obj)
{
	SeahorsePrivateKey *self = SEAHORSE_PRIVATE_KEY (obj);

	g_clear_object (&self->pv->actions);

	if (self->pv->attributes)
		gck_attributes_unref (self->pv->attributes);

	G_OBJECT_CLASS (seahorse_private_key_parent_class)->finalize (obj);
}

static void
seahorse_private_key_get_property (GObject *obj,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	SeahorsePrivateKey *self = SEAHORSE_PRIVATE_KEY (obj);

	switch (prop_id) {
	case PROP_PLACE:
		g_value_set_object (value, self->pv->token);
		break;
	case PROP_ATTRIBUTES:
		g_value_set_boxed (value, self->pv->attributes);
		break;
	case PROP_FLAGS:
		g_value_set_flags (value, SEAHORSE_FLAG_PERSONAL);
		break;
	case PROP_ACTIONS:
		g_value_set_object (value, self->pv->actions);
		break;
	case PROP_CERTIFICATE:
		g_value_set_object (value, seahorse_private_key_get_certificate (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_private_key_set_property (GObject *obj,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	SeahorsePrivateKey *self = SEAHORSE_PRIVATE_KEY (obj);

	switch (prop_id) {
	case PROP_PLACE:
		if (self->pv->token)
			g_object_remove_weak_pointer (G_OBJECT (self->pv->token),
			                              (gpointer *)&self->pv->token);
		self->pv->token = g_value_get_object (value);
		if (self->pv->token)
			g_object_add_weak_pointer (G_OBJECT (self->pv->token),
			                           (gpointer *)&self->pv->token);
		break;
	case PROP_ATTRIBUTES:
		if (self->pv->attributes)
			gck_attributes_unref (self->pv->attributes);
		self->pv->attributes = g_value_dup_boxed (value);
		break;
	case PROP_CERTIFICATE:
		seahorse_private_key_set_certificate (self, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_private_key_class_init (SeahorsePrivateKeyClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (SeahorsePrivateKeyPrivate));

	gobject_class->finalize = seahorse_private_key_finalize;
	gobject_class->set_property = seahorse_private_key_set_property;
	gobject_class->get_property = seahorse_private_key_get_property;

	g_object_class_install_property (gobject_class, PROP_PLACE,
	         g_param_spec_object ("place", "place", "place", SEAHORSE_TYPE_TOKEN,
	                              G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_FLAGS,
	         g_param_spec_flags ("flags", "flags", "flags", SEAHORSE_TYPE_FLAGS, SEAHORSE_FLAG_NONE,
	                              G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_ACTIONS,
	         g_param_spec_object ("actions", "Actions", "Actions", GTK_TYPE_ACTION_GROUP,
	                              G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_CERTIFICATE,
	            g_param_spec_object ("certificate", "Certificate", "Certificate associated with this private key",
	                                 SEAHORSE_TYPE_CERTIFICATE, G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

	g_object_class_override_property (gobject_class, PROP_ATTRIBUTES, "attributes");
}

static void
seahorse_private_key_object_attributes_iface (GckObjectAttributesIface *iface)
{
	iface->attribute_types = REQUIRED_ATTRS;
	iface->n_attribute_types = G_N_ELEMENTS (REQUIRED_ATTRS);
}

SeahorseCertificate *
seahorse_private_key_get_certificate (SeahorsePrivateKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PRIVATE_KEY (self), NULL);
	return self->pv->certificate;
}

void
seahorse_private_key_set_certificate (SeahorsePrivateKey *self,
                                      SeahorseCertificate *certificate)
{
	GObject *obj;

	g_return_if_fail (SEAHORSE_IS_PRIVATE_KEY (self));
	g_return_if_fail (certificate == NULL || SEAHORSE_IS_CERTIFICATE (certificate));

	if (self->pv->certificate)
		g_object_remove_weak_pointer (G_OBJECT (certificate),
		                              (gpointer *)self->pv->certificate);
	self->pv->certificate = certificate;
	if (self->pv->certificate)
		g_object_add_weak_pointer (G_OBJECT (certificate),
		                           (gpointer *)self->pv->certificate);

	obj = G_OBJECT (self);
	g_object_notify (obj, "certificate");
}
