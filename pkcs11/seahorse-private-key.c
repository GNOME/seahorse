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
#include "seahorse-pkcs11-helpers.h"
#include "seahorse-pkcs11-key-deleter.h"
#include "seahorse-pkcs11-properties.h"
#include "seahorse-token.h"
#include "seahorse-types.h"

#include "seahorse-deletable.h"
#include "seahorse-exportable.h"
#include "seahorse-util.h"
#include "seahorse-viewable.h"

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
	CKA_MODIFIABLE,
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
	PROP_LABEL,
	PROP_MARKUP,
	PROP_DESCRIPTION,
	PROP_ICON
};

struct _SeahorsePrivateKeyPrivate {
	SeahorseToken *token;
	GckAttributes *attributes;
	SeahorseCertificate *certificate;
	GIcon *icon;
};

static void seahorse_private_key_deletable_iface (SeahorseDeletableIface *iface);

static void seahorse_private_key_exportable_iface (SeahorseExportableIface *iface);

static void seahorse_private_key_object_cache_iface (GckObjectCacheIface *iface);

static void seahorse_private_key_viewable_iface (SeahorseViewableIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorsePrivateKey, seahorse_private_key, GCK_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCK_TYPE_OBJECT_CACHE, seahorse_private_key_object_cache_iface);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_DELETABLE, seahorse_private_key_deletable_iface);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_EXPORTABLE, seahorse_private_key_exportable_iface);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_VIEWABLE, seahorse_private_key_viewable_iface);
);

static void
update_icon (SeahorsePrivateKey *self)
{
	g_clear_object (&self->pv->icon);
	self->pv->icon = g_themed_icon_new (GCR_ICON_KEY);
}

static void
seahorse_private_key_init (SeahorsePrivateKey *self)
{
	self->pv = (G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_PRIVATE_KEY, SeahorsePrivateKeyPrivate));
}

static void
seahorse_private_key_finalize (GObject *obj)
{
	SeahorsePrivateKey *self = SEAHORSE_PRIVATE_KEY (obj);

	g_clear_object (&self->pv->icon);

	if (self->pv->attributes)
		gck_attributes_unref (self->pv->attributes);

	G_OBJECT_CLASS (seahorse_private_key_parent_class)->finalize (obj);
}

static gchar *
calculate_label (SeahorsePrivateKey *self)
{
	gchar *label = NULL;

	if (self->pv->attributes) {
		if (gck_attributes_find_string (self->pv->attributes, CKA_LABEL, &label))
			return label;
	}

	if (self->pv->certificate) {
		g_object_get (self->pv->certificate, "label", &label, NULL);
		return label;
	}

	return g_strdup (_("Unnamed private key"));
}

static gchar *
calculate_markup (SeahorsePrivateKey *self)
{
	gchar *label = calculate_label (self);
	return g_markup_escape_text (label, -1);
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
		g_value_set_object (value, NULL);
		break;
	case PROP_PARTNER:
		g_value_set_object (value, seahorse_private_key_get_partner (self));
		break;
	case PROP_LABEL:
		g_value_take_string (value, calculate_label (self));
		break;
	case PROP_MARKUP:
		g_value_take_string (value, calculate_markup (self));
		break;
	case PROP_EXPORTABLE:
		g_value_set_boolean (value, FALSE);
		break;
	case PROP_DELETABLE:
		g_value_set_boolean (value, seahorse_token_is_deletable (self->pv->token, GCK_OBJECT (self)));
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, _("Private key"));
		break;
	case PROP_ICON:
		g_value_set_object (value, seahorse_private_key_get_icon (self));
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
	case PROP_PARTNER:
		seahorse_private_key_set_partner (self, g_value_get_object (value));
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
	         g_param_spec_flags ("object-flags", "flags", "flags", SEAHORSE_TYPE_FLAGS, SEAHORSE_FLAG_NONE,
	                              G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_ACTIONS,
	         g_param_spec_object ("actions", "Actions", "Actions", GTK_TYPE_ACTION_GROUP,
	                              G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_PARTNER,
	            g_param_spec_object ("partner", "Partner", "Certificate associated with this private key",
	                                 SEAHORSE_TYPE_CERTIFICATE, G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_LABEL,
	            g_param_spec_string ("label", "label", "label",
	                                 "", G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_MARKUP,
	            g_param_spec_string ("markup", "markup", "markup",
	                                 "", G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_DESCRIPTION,
	            g_param_spec_string ("description", "description", "description",
	                                 "", G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_ICON,
	            g_param_spec_object ("icon", "icon", "icon",
	                                 G_TYPE_ICON, G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

	g_object_class_override_property (gobject_class, PROP_ATTRIBUTES, "attributes");

	g_object_class_override_property (gobject_class, PROP_EXPORTABLE, "exportable");

	g_object_class_override_property (gobject_class, PROP_DELETABLE, "deletable");
}

static void
seahorse_private_key_fill (GckObjectCache *object,
                           GckAttributes *attributes)
{
	SeahorsePrivateKey *self = SEAHORSE_PRIVATE_KEY (object);
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
seahorse_private_key_object_cache_iface (GckObjectCacheIface *iface)
{
	iface->default_types = REQUIRED_ATTRS;
	iface->n_default_types = G_N_ELEMENTS (REQUIRED_ATTRS);
	iface->fill = seahorse_private_key_fill;
}

static SeahorseDeleter *
seahorse_private_key_create_deleter (SeahorseDeletable *deletable)
{
	SeahorsePrivateKey *self = SEAHORSE_PRIVATE_KEY (deletable);
	if (!seahorse_token_is_deletable (self->pv->token, GCK_OBJECT (self)))
		return NULL;
	return seahorse_pkcs11_key_deleter_new (G_OBJECT (self));
}

static void
seahorse_private_key_deletable_iface (SeahorseDeletableIface *iface)
{
	iface->create_deleter = seahorse_private_key_create_deleter;
}

static GList *
seahorse_private_key_create_exporters (SeahorseExportable *exportable,
                                       SeahorseExporterType type)
{
	/* In the future we may exporters here, but for now no exporting */
	return NULL;
}

static void
seahorse_private_key_exportable_iface (SeahorseExportableIface *iface)
{
	iface->create_exporters = seahorse_private_key_create_exporters;
}

static void
seahorse_private_key_show_viewer (SeahorseViewable *viewable,
                                  GtkWindow *parent)
{
	GtkWindow *viewer = seahorse_pkcs11_properties_show (G_OBJECT (viewable), parent);
	gtk_widget_show (GTK_WIDGET (viewer));
}

static void
seahorse_private_key_viewable_iface (SeahorseViewableIface *iface)
{
	iface->show_viewer = seahorse_private_key_show_viewer;
}

SeahorseCertificate *
seahorse_private_key_get_partner (SeahorsePrivateKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PRIVATE_KEY (self), NULL);
	return self->pv->certificate;
}

static void
notify_certificate_change (GObject *obj)
{
	g_object_notify (obj, "partner");
	g_object_notify (obj, "label");
	g_object_notify (obj, "markup");
}

static void
on_certificate_gone (gpointer data,
                     GObject *where_the_object_was)
{
	SeahorsePrivateKey *self = SEAHORSE_PRIVATE_KEY (data);
	self->pv->certificate = NULL;
	notify_certificate_change (G_OBJECT (self));
}

void
seahorse_private_key_set_partner (SeahorsePrivateKey *self,
                                      SeahorseCertificate *certificate)
{
	g_return_if_fail (SEAHORSE_IS_PRIVATE_KEY (self));
	g_return_if_fail (certificate == NULL || SEAHORSE_IS_CERTIFICATE (certificate));

	if (self->pv->certificate)
		g_object_weak_unref (G_OBJECT (self->pv->certificate), on_certificate_gone, self);
	self->pv->certificate = certificate;
	if (self->pv->certificate)
		g_object_weak_ref (G_OBJECT (self->pv->certificate), on_certificate_gone, self);

	notify_certificate_change (G_OBJECT (self));
}

GIcon *
seahorse_private_key_get_icon (SeahorsePrivateKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PRIVATE_KEY (self), NULL);

	if (!self->pv->icon)
		update_icon (self);

	return self->pv->icon;
}
