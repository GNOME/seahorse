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

#include "seahorse-certificate.h"
#include "seahorse-pkcs11.h"
#include "seahorse-pkcs11-actions.h"
#include "seahorse-pkcs11-helpers.h"
#include "seahorse-token.h"
#include "seahorse-types.h"

#include "seahorse-util.h"
#include "seahorse-validity.h"

#include <gcr/gcr.h>
#include <gck/pkcs11.h>

#include <glib/gi18n-lib.h>

static const gulong REQUIRED_ATTRS[] = {
	CKA_VALUE,
	CKA_ID,
	CKA_LABEL,
	CKA_CLASS
};

enum {
	PROP_0,
	PROP_PLACE,
	PROP_ATTRIBUTES,
	PROP_FLAGS,
	PROP_ACTIONS
};

struct _SeahorseCertificatePrivate {
	SeahorseToken *token;
	GckAttributes *attributes;
	GckAttribute der_value;
	GtkActionGroup *actions;
};

static void seahorse_certificate_certificate_iface (GcrCertificateIface *iface);
static void seahorse_certificate_object_attributes_iface (GckObjectAttributesIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseCertificate, seahorse_certificate, GCK_TYPE_OBJECT,
                         GCR_CERTIFICATE_MIXIN_IMPLEMENT_COMPARABLE ();
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_CERTIFICATE, seahorse_certificate_certificate_iface);
                         G_IMPLEMENT_INTERFACE (GCK_TYPE_OBJECT_ATTRIBUTES, seahorse_certificate_object_attributes_iface)
);

static void
seahorse_certificate_init (SeahorseCertificate *self)
{
	self->pv = (G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_CERTIFICATE, SeahorseCertificatePrivate));
	self->pv->actions = seahorse_pkcs11_object_actions_instance ();
	gck_attribute_init_invalid (&self->pv->der_value, CKA_VALUE);
}

static void
seahorse_certificate_notify (GObject *obj,
                             GParamSpec *pspec)
{
	/* When the base class attributes changes, it can affects lots */
	if (g_str_equal (pspec->name, "attributes"))
		gcr_certificate_mixin_emit_notify (GCR_CERTIFICATE (obj));

	if (G_OBJECT_CLASS (seahorse_certificate_parent_class)->notify)
		G_OBJECT_CLASS (seahorse_certificate_parent_class)->notify (obj, pspec);
}


static void
seahorse_certificate_finalize (GObject *obj)
{
	SeahorseCertificate *self = SEAHORSE_CERTIFICATE (obj);

	g_clear_object (&self->pv->actions);

	if (self->pv->attributes)
		gck_attributes_unref (self->pv->attributes);
	gck_attribute_clear (&self->pv->der_value);

	G_OBJECT_CLASS (seahorse_certificate_parent_class)->finalize (obj);
}

static void
seahorse_certificate_get_property (GObject *obj,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	SeahorseCertificate *self = SEAHORSE_CERTIFICATE (obj);

	switch (prop_id) {
	case PROP_PLACE:
		g_value_set_object (value, self->pv->token);
		break;
	case PROP_ATTRIBUTES:
		g_value_set_boxed (value, self->pv->attributes);
		break;
	case PROP_FLAGS:
		g_value_set_flags (value, SEAHORSE_FLAG_PERSONAL |
		                          SEAHORSE_FLAG_EXPORTABLE);
		break;
	case PROP_ACTIONS:
		g_value_set_object (value, self->pv->actions);
		break;
	default:
		gcr_certificate_mixin_get_property (obj, prop_id, value, pspec);
		break;
	}
}

static void
seahorse_certificate_set_property (GObject *obj,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	SeahorseCertificate *self = SEAHORSE_CERTIFICATE (obj);
	GckAttribute *der_value = NULL;

	switch (prop_id) {
	case PROP_PLACE:
		g_return_if_fail (self->pv->token == NULL);
		self->pv->token = g_value_dup_object (value);
		break;
	case PROP_ATTRIBUTES:
		if (self->pv->attributes)
			gck_attributes_unref (self->pv->attributes);
		self->pv->attributes = g_value_dup_boxed (value);
		if (self->pv->attributes)
			der_value = gck_attributes_find (self->pv->attributes,
			                                 CKA_VALUE);
		if (der_value) {
			gck_attribute_clear (&self->pv->der_value);
			gck_attribute_init_copy (&self->pv->der_value, der_value);
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_certificate_class_init (SeahorseCertificateClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (SeahorseCertificatePrivate));

	gobject_class->finalize = seahorse_certificate_finalize;
	gobject_class->set_property = seahorse_certificate_set_property;
	gobject_class->get_property = seahorse_certificate_get_property;
	gobject_class->notify = seahorse_certificate_notify;

	g_object_class_install_property (gobject_class, PROP_PLACE,
	         g_param_spec_object ("place", "place", "place", SEAHORSE_TYPE_TOKEN,
	                              G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_FLAGS,
	         g_param_spec_flags ("flags", "flags", "flags", SEAHORSE_TYPE_FLAGS, SEAHORSE_FLAG_NONE,
	                              G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_ACTIONS,
	         g_param_spec_object ("actions", "Actions", "Actions", GTK_TYPE_ACTION_GROUP,
	                              G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));

	g_object_class_override_property (gobject_class, PROP_ATTRIBUTES, "attributes");

	gcr_certificate_mixin_class_init (gobject_class);
}

static const guchar *
seahorse_certificate_get_der_data (GcrCertificate *cert,
                                   gsize *n_length)
{
	SeahorseCertificate *self = SEAHORSE_CERTIFICATE (cert);

	g_return_val_if_fail (!gck_attribute_is_invalid (&self->pv->der_value), NULL);
	*n_length = self->pv->der_value.length;
	return self->pv->der_value.value;
}

static void
seahorse_certificate_certificate_iface (GcrCertificateIface *iface)
{
	iface->get_der_data = (gpointer)seahorse_certificate_get_der_data;
}

static void
seahorse_certificate_object_attributes_iface (GckObjectAttributesIface *iface)
{
	iface->attribute_types = REQUIRED_ATTRS;
	iface->n_attribute_types = G_N_ELEMENTS (REQUIRED_ATTRS);
}
