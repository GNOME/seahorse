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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "seahorse-common.h"

#include "seahorse-certificate.h"
#include "seahorse-pkcs11.h"
#include "seahorse-pkcs11-deleter.h"
#include "seahorse-pkcs11-properties.h"
#include "seahorse-private-key.h"
#include "seahorse-token.h"

#include "seahorse-util.h"
#include "seahorse-validity.h"

#include <gcr/gcr.h>

#include <glib/gi18n-lib.h>

static const gulong REQUIRED_ATTRS[] = {
	CKA_VALUE,
	CKA_ID,
	CKA_LABEL,
	CKA_CLASS,
	CKA_CERTIFICATE_CATEGORY,
	CKA_MODIFIABLE
};

enum {
	PROP_0,
	PROP_PLACE,
	PROP_ATTRIBUTES,
	PROP_FLAGS,
	PROP_ACTIONS,
	PROP_PARTNER,

	PROP_EXPORTABLE,
	PROP_DELETABLE,
	PROP_ICON,
	PROP_DESCRIPTION
};

struct _SeahorseCertificatePrivate {
	SeahorseToken *token;
	GckAttributes *attributes;
	const GckAttribute *value;
	SeahorsePrivateKey *private_key;
	GIcon *icon;
	guint flags;
};

static void   seahorse_certificate_certificate_iface           (GcrCertificateIface *iface);

static void   seahorse_certificate_object_cache_iface          (GckObjectCacheIface *iface);

static void   seahorse_certificate_deletable_iface             (SeahorseDeletableIface *iface);

static void   seahorse_certificate_exportable_iface            (SeahorseExportableIface *iface);

static void   seahorse_certificate_viewable_iface              (SeahorseViewableIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseCertificate, seahorse_certificate, GCK_TYPE_OBJECT,
                         GCR_CERTIFICATE_MIXIN_IMPLEMENT_COMPARABLE ();
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_CERTIFICATE, seahorse_certificate_certificate_iface);
                         G_IMPLEMENT_INTERFACE (GCK_TYPE_OBJECT_CACHE, seahorse_certificate_object_cache_iface);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_DELETABLE, seahorse_certificate_deletable_iface);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_EXPORTABLE, seahorse_certificate_exportable_iface);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_VIEWABLE, seahorse_certificate_viewable_iface);
);

static void
update_icon (SeahorseCertificate *self)
{
	GEmblem *emblem;
	GIcon *eicon;
	GIcon *icon;

	g_clear_object (&self->pv->icon);
	icon = gcr_certificate_get_icon (GCR_CERTIFICATE (self));
	if (self->pv->private_key) {
		eicon = g_themed_icon_new (GCR_ICON_KEY);
		emblem = g_emblem_new (eicon);
		self->pv->icon = g_emblemed_icon_new (icon, emblem);
		g_object_unref (icon);
		g_object_unref (eicon);
		g_object_unref (emblem);
	} else {
		self->pv->icon = icon;
	}
}

static void
seahorse_certificate_init (SeahorseCertificate *self)
{
	self->pv = (G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_CERTIFICATE, SeahorseCertificatePrivate));
	self->pv->value = NULL;
	self->pv->flags = G_MAXUINT;
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
seahorse_certificate_dispose (GObject *obj)
{
	SeahorseCertificate *self = SEAHORSE_CERTIFICATE (obj);

	seahorse_certificate_set_partner (self, NULL);

	G_OBJECT_CLASS (seahorse_certificate_parent_class)->dispose (obj);
}

static void
seahorse_certificate_finalize (GObject *obj)
{
	SeahorseCertificate *self = SEAHORSE_CERTIFICATE (obj);

	if (self->pv->attributes)
		gck_attributes_unref (self->pv->attributes);
	self->pv->value = NULL;

	G_OBJECT_CLASS (seahorse_certificate_parent_class)->finalize (obj);
}

static guint
calc_is_personal_and_trusted (SeahorseCertificate *self)
{
	gulong category = 0;
	gboolean is_ca;

	/* If a matching private key, then this is personal*/
	if (self->pv->private_key)
		return SEAHORSE_FLAG_PERSONAL | SEAHORSE_FLAG_TRUSTED;

	if (gck_attributes_find_ulong (self->pv->attributes, CKA_CERTIFICATE_CATEGORY, &category)) {
		if (category == 2)
			return 0;
		else if (category == 1)
			return SEAHORSE_FLAG_PERSONAL;
	}

	if (gcr_certificate_get_basic_constraints (GCR_CERTIFICATE (self), &is_ca, NULL))
		return is_ca ? 0 : SEAHORSE_FLAG_PERSONAL;

	return SEAHORSE_FLAG_PERSONAL;
}

static void
ensure_flags (SeahorseCertificate *self)
{
	if (self->pv->flags != G_MAXUINT)
		return;

	self->pv->flags = SEAHORSE_FLAG_EXPORTABLE |
	                  calc_is_personal_and_trusted (self);
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
		ensure_flags (self);
		g_value_set_flags (value, self->pv->flags);
		break;
	case PROP_ACTIONS:
		g_value_set_object (value, NULL);
		break;
	case PROP_PARTNER:
		g_value_set_object (value, seahorse_certificate_get_partner (self));
		break;
	case PROP_ICON:
		g_value_set_object (value, seahorse_certificate_get_icon (self));
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, seahorse_certificate_get_description (self));
		break;
	case PROP_EXPORTABLE:
		g_value_set_boolean (value, gck_attributes_find (self->pv->attributes, CKA_VALUE) != NULL);
		break;
	case PROP_DELETABLE:
		g_value_set_boolean (value, seahorse_token_is_deletable (self->pv->token, GCK_OBJECT (self)));
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
		g_return_if_fail (self->pv->attributes);
		self->pv->value = gck_attributes_find (self->pv->attributes, CKA_VALUE);
		g_return_if_fail (self->pv->value != NULL);
		g_return_if_fail (!gck_attribute_is_invalid (self->pv->value));
		break;
	case PROP_PARTNER:
		seahorse_certificate_set_partner (self, g_value_get_object (value));
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

	gobject_class->dispose = seahorse_certificate_dispose;
	gobject_class->finalize = seahorse_certificate_finalize;
	gobject_class->set_property = seahorse_certificate_set_property;
	gobject_class->get_property = seahorse_certificate_get_property;
	gobject_class->notify = seahorse_certificate_notify;

	g_object_class_install_property (gobject_class, PROP_PLACE,
	         g_param_spec_object ("place", "place", "place", SEAHORSE_TYPE_TOKEN,
	                              G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_FLAGS,
	         g_param_spec_flags ("object-flags", "flags", "flags", SEAHORSE_TYPE_FLAGS, SEAHORSE_FLAG_NONE,
	                              G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_ACTIONS,
	         g_param_spec_object ("actions", "Actions", "Actions", GTK_TYPE_ACTION_GROUP,
	                              G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_PARTNER,
	            g_param_spec_object ("partner", "Partner", "Private key associated with certificate",
	                                 SEAHORSE_TYPE_PRIVATE_KEY, G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

	g_object_class_override_property (gobject_class, PROP_ATTRIBUTES, "attributes");

	g_object_class_override_property (gobject_class, PROP_DELETABLE, "deletable");

	g_object_class_override_property (gobject_class, PROP_EXPORTABLE, "exportable");

	g_object_class_override_property (gobject_class, PROP_ICON, "icon");
	g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");

	gcr_certificate_mixin_class_init (gobject_class);
}

static const guchar *
seahorse_certificate_get_der_data (GcrCertificate *cert,
                                   gsize *n_length)
{
	SeahorseCertificate *self = SEAHORSE_CERTIFICATE (cert);

	g_return_val_if_fail (self->pv->value != NULL &&
	                      !gck_attribute_is_invalid (self->pv->value), NULL);
	*n_length = self->pv->value->length;
	return self->pv->value->value;
}

static void
seahorse_certificate_certificate_iface (GcrCertificateIface *iface)
{
	iface->get_der_data = (gpointer)seahorse_certificate_get_der_data;
}

static void
seahorse_certificate_fill (GckObjectCache *object,
                           GckAttributes *attributes)
{
	SeahorseCertificate *self = SEAHORSE_CERTIFICATE (object);
	GckBuilder builder = GCK_BUILDER_INIT;

	if (self->pv->attributes)
		gck_builder_add_all (&builder, self->pv->attributes);
	gck_builder_set_all (&builder, attributes);
	gck_attributes_unref (self->pv->attributes);
	self->pv->attributes = gck_builder_steal (&builder);
	gck_builder_clear (&builder);

	g_object_notify (G_OBJECT (object), "attributes");
}

static void
seahorse_certificate_object_cache_iface (GckObjectCacheIface *iface)
{
	iface->default_types = REQUIRED_ATTRS;
	iface->n_default_types = G_N_ELEMENTS (REQUIRED_ATTRS);
	iface->fill = seahorse_certificate_fill;
}

static GList *
seahorse_certificate_create_exporters (SeahorseExportable *exportable,
                                       SeahorseExporterType type)
{
	SeahorseCertificateDerExporter *exporter;
	gboolean can_export = FALSE;

	g_object_get (exportable, "exportable", &can_export, NULL);
	if (!can_export)
		return NULL;

	exporter = seahorse_certificate_der_exporter_new (GCR_CERTIFICATE (exportable));
	return g_list_append (NULL, exporter);
}

static gboolean
seahorse_certificate_get_exportable (SeahorseExportable *exportable)
{
	gboolean can;
	g_object_get (exportable, "exportable", &can, NULL);
	return can;
}

static void
seahorse_certificate_exportable_iface (SeahorseExportableIface *iface)
{
	iface->create_exporters = seahorse_certificate_create_exporters;
	iface->get_exportable = seahorse_certificate_get_exportable;
}

static SeahorseDeleter *
seahorse_certificate_create_deleter (SeahorseDeletable *deletable)
{
	SeahorseCertificate *self = SEAHORSE_CERTIFICATE (deletable);
	SeahorseDeleter *deleter = NULL;

	if (self->pv->private_key)
		deleter = seahorse_deletable_create_deleter (SEAHORSE_DELETABLE (self->pv->private_key));

	if (seahorse_token_is_deletable (self->pv->token, GCK_OBJECT (self))) {
		if (deleter == NULL)
			deleter = seahorse_pkcs11_deleter_new (self);
		else if (!seahorse_deleter_add_object (deleter, G_OBJECT (self)))
			g_return_val_if_reached (NULL);
	}

	return deleter;
}

static gboolean
seahorse_certificate_get_deletable (SeahorseDeletable *deletable)
{
	gboolean can;
	g_object_get (deletable, "deletable", &can, NULL);
	return can;
}

static void
seahorse_certificate_deletable_iface (SeahorseDeletableIface *iface)
{
	iface->create_deleter = seahorse_certificate_create_deleter;
	iface->get_deletable = seahorse_certificate_get_deletable;
}

static GtkWindow *
seahorse_certificate_create_viewer (SeahorseViewable *viewable,
                                    GtkWindow *parent)
{
	GtkWindow *viewer = seahorse_pkcs11_properties_new (G_OBJECT (viewable), parent);
	gtk_widget_show (GTK_WIDGET (viewer));
	return viewer;
}

static void
seahorse_certificate_viewable_iface (SeahorseViewableIface *iface)
{
	iface->create_viewer = seahorse_certificate_create_viewer;
}

GIcon *
seahorse_certificate_get_icon (SeahorseCertificate *self)
{
	g_return_val_if_fail (SEAHORSE_IS_CERTIFICATE (self), NULL);

	if (!self->pv->icon)
		update_icon (self);

	return self->pv->icon;
}

const gchar *
seahorse_certificate_get_description (SeahorseCertificate *self)
{
	g_return_val_if_fail (SEAHORSE_IS_CERTIFICATE (self), NULL);

	ensure_flags (self);

	if (self->pv->private_key)
		return _("Personal certificate and key");

	if (self->pv->flags & SEAHORSE_FLAG_PERSONAL)
		return _("Personal certificate");
	else
		return _("Certificate");
}

SeahorsePrivateKey *
seahorse_certificate_get_partner (SeahorseCertificate *self)
{
	g_return_val_if_fail (SEAHORSE_IS_CERTIFICATE (self), NULL);
	return self->pv->private_key;
}

void
seahorse_certificate_set_partner (SeahorseCertificate *self,
                                  SeahorsePrivateKey *private_key)
{
	GObject *obj;

	g_return_if_fail (SEAHORSE_IS_CERTIFICATE (self));
	g_return_if_fail (private_key == NULL || SEAHORSE_IS_PRIVATE_KEY (private_key));

	if (self->pv->private_key)
		g_object_remove_weak_pointer (G_OBJECT (self->pv->private_key),
		                              (gpointer *)self->pv->private_key);
	self->pv->private_key = private_key;
	if (self->pv->private_key)
		g_object_add_weak_pointer (G_OBJECT (private_key),
		                           (gpointer *)self->pv->private_key);

	obj = G_OBJECT (self);
	g_object_notify (obj, "partner");
	g_clear_object (&self->pv->icon);
	g_object_notify (obj, "icon");
	g_object_notify (obj, "description");
}
