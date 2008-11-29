/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
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

#include "seahorse-pkcs11-certificate.h"

#include "seahorse-object.h"
#include "seahorse-pkcs11.h"
#include "seahorse-util.h"
#include "seahorse-validity.h"

#include "libcryptui/crui-x509-cert.h"

#include <pkcs11.h>
#include <pkcs11g.h>

#include <glib/gi18n-lib.h>

enum {
	PROP_0,
	PROP_PKCS11_OBJECT,
	PROP_PKCS11_ATTRIBUTES,
	PROP_FINGERPRINT,
	PROP_VALIDITY,
	PROP_VALIDITY_STR,
	PROP_TRUST,
	PROP_TRUST_STR,
	PROP_EXPIRES,
	PROP_EXPIRES_STR
};

struct _SeahorsePkcs11CertificatePrivate {
	GP11Object* pkcs11_object;
	GP11Attributes* pkcs11_attributes;
	GP11Attribute der_value;
};

static void seahorse_pkcs11_certificate_iface (CruiX509CertIface *iface);

G_DEFINE_TYPE_EXTENDED (SeahorsePkcs11Certificate, seahorse_pkcs11_certificate, SEAHORSE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (CRUI_TYPE_X509_CERT, seahorse_pkcs11_certificate_iface));

#define SEAHORSE_PKCS11_CERTIFICATE_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_PKCS11_TYPE_CERTIFICATE, SeahorsePkcs11CertificatePrivate))

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static gchar* 
calc_display_name (SeahorsePkcs11Certificate *self) 
{
	SeahorsePkcs11CertificatePrivate *pv = SEAHORSE_PKCS11_CERTIFICATE_GET_PRIVATE (self);
	gchar *label = NULL;
	
	if (pv->pkcs11_attributes != NULL) {
		if (gp11_attributes_find_string (pv->pkcs11_attributes, CKA_LABEL, &label))
			return label;
	}
	
	/* TODO: Calculate something from the subject? */
	return g_strdup (_("Certificate"));
}

static gchar* 
calc_display_id (SeahorsePkcs11Certificate* self) 
{
	gsize len;
	gchar *id, *ret;
	
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	
	id = seahorse_pkcs11_certificate_get_fingerprint (self);
	g_return_val_if_fail (id, NULL);
	
	len = strlen (id);
	if (len <= 8)
		return id;

	ret = g_strndup (id + (len - 8), 8);
	g_free (id);
	return ret;
}

static void 
certificate_rebuild (SeahorsePkcs11Certificate* self) 
{
	SeahorsePkcs11CertificatePrivate *pv;
	gboolean exportable;
	gchar *name, *identifier;
	guint flags;
	
	g_assert (SEAHORSE_PKCS11_IS_CERTIFICATE (self));
	pv = SEAHORSE_PKCS11_CERTIFICATE_GET_PRIVATE (self);
	
	if (pv->pkcs11_attributes == NULL) {
		
		g_object_set (self,
		              "id", 0,
		              "label", "",
		              "icon", NULL,
		              "usage", SEAHORSE_USAGE_NONE,
		              "description", "",
		              "identifier", "",
		              "location", SEAHORSE_LOCATION_INVALID,
		              "flags", SEAHORSE_FLAG_DISABLED,
		              NULL);
		return;
		
	} 

	flags = 0;
	if (gp11_attributes_find_boolean (pv->pkcs11_attributes, CKA_EXTRACTABLE, &exportable) && exportable)
		flags |= SEAHORSE_FLAG_EXPORTABLE;

	/* TODO: Expiry, revoked, disabled etc... */
	if (seahorse_pkcs11_certificate_get_trust (self) >= SEAHORSE_VALIDITY_MARGINAL)
		flags |= SEAHORSE_FLAG_TRUSTED;

	name = calc_display_name (self);
	identifier = calc_display_id (self);
	
	g_object_set (self,
		      "id", seahorse_pkcs11_id_from_attributes (pv->pkcs11_attributes),
		      "label", name,
		      "icon", "",
		      "usage", SEAHORSE_USAGE_PUBLIC_KEY,
		      "description", _("Certificate"),
		      "identifier", identifier,
		      "location", SEAHORSE_LOCATION_LOCAL,
		      "flags", 0,
		      NULL);

	g_free (name);
	g_free (identifier);
}


/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_pkcs11_certificate_init (SeahorsePkcs11Certificate *self)
{
	SeahorsePkcs11CertificatePrivate *pv = SEAHORSE_PKCS11_CERTIFICATE_GET_PRIVATE (self);
	gp11_attribute_init_invalid (&pv->der_value, CKA_VALUE);
}

static void
seahorse_pkcs11_certificate_dispose (GObject *obj)
{
	SeahorsePkcs11Certificate *self = SEAHORSE_PKCS11_CERTIFICATE (obj);
	SeahorsePkcs11CertificatePrivate *pv = SEAHORSE_PKCS11_CERTIFICATE_GET_PRIVATE (self);
    
	if (pv->pkcs11_object)
		g_object_unref (pv->pkcs11_object);
	pv->pkcs11_object = NULL;
	
	G_OBJECT_CLASS (seahorse_pkcs11_certificate_parent_class)->dispose (obj);
}

static void
seahorse_pkcs11_certificate_finalize (GObject *obj)
{
	SeahorsePkcs11Certificate *self = SEAHORSE_PKCS11_CERTIFICATE (obj);
	SeahorsePkcs11CertificatePrivate *pv = SEAHORSE_PKCS11_CERTIFICATE_GET_PRIVATE (self);

	g_assert (pv->pkcs11_object == NULL);
	
	if (pv->pkcs11_attributes)
		gp11_attributes_unref (pv->pkcs11_attributes);
	pv->pkcs11_attributes = NULL;
	
	gp11_attribute_clear (&pv->der_value);
	
	G_OBJECT_CLASS (seahorse_pkcs11_certificate_parent_class)->finalize (obj);
}

static void
seahorse_pkcs11_certificate_get_property (GObject *obj, guint prop_id, GValue *value, 
                                          GParamSpec *pspec)
{
	SeahorsePkcs11Certificate *self = SEAHORSE_PKCS11_CERTIFICATE (obj);
	
	switch (prop_id) {
	case PROP_PKCS11_OBJECT:
		g_value_set_object (value, seahorse_pkcs11_certificate_get_pkcs11_object (self));
		break;
	case PROP_PKCS11_ATTRIBUTES:
		g_value_set_boxed (value, seahorse_pkcs11_certificate_get_pkcs11_attributes (self));
		break;
	case PROP_FINGERPRINT:
		g_value_take_string (value, seahorse_pkcs11_certificate_get_fingerprint (self));
		break;
	case PROP_VALIDITY:
		g_value_set_uint (value, seahorse_pkcs11_certificate_get_validity (self));
		break;
	case PROP_VALIDITY_STR:
		g_value_set_string (value, seahorse_pkcs11_certificate_get_validity_str (self));
		break;
	case PROP_TRUST:
		g_value_set_uint (value, seahorse_pkcs11_certificate_get_trust (self));
		break;
	case PROP_TRUST_STR:
		g_value_set_string (value, seahorse_pkcs11_certificate_get_trust_str (self));
		break;
	case PROP_EXPIRES:
		g_value_set_ulong (value, seahorse_pkcs11_certificate_get_expires (self));
		break;
	case PROP_EXPIRES_STR:
		g_value_take_string (value, seahorse_pkcs11_certificate_get_expires_str (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_pkcs11_certificate_set_property (GObject *obj, guint prop_id, const GValue *value, 
                                          GParamSpec *pspec)
{
	SeahorsePkcs11Certificate *self = SEAHORSE_PKCS11_CERTIFICATE (obj);
	
	switch (prop_id) {
	case PROP_PKCS11_OBJECT:
		seahorse_pkcs11_certificate_set_pkcs11_object (self, g_value_get_object (value));
		break;
	case PROP_PKCS11_ATTRIBUTES:
		seahorse_pkcs11_certificate_set_pkcs11_attributes (self, g_value_get_boxed (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_pkcs11_certificate_class_init (SeahorsePkcs11CertificateClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	seahorse_pkcs11_certificate_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorsePkcs11CertificatePrivate));

	gobject_class->dispose = seahorse_pkcs11_certificate_dispose;
	gobject_class->finalize = seahorse_pkcs11_certificate_finalize;
	gobject_class->set_property = seahorse_pkcs11_certificate_set_property;
	gobject_class->get_property = seahorse_pkcs11_certificate_get_property;

	g_object_class_install_property (gobject_class, PROP_PKCS11_OBJECT, 
	         g_param_spec_object ("pkcs11-object", "pkcs11-object", "pkcs11-object", GP11_TYPE_OBJECT, 
	                              G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	
	g_object_class_install_property (gobject_class, PROP_PKCS11_ATTRIBUTES, 
	         g_param_spec_boxed ("pkcs11-attributes", "pkcs11-attributes", "pkcs11-attributes", GP11_TYPE_ATTRIBUTES, 
	                             G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	
	g_object_class_install_property (gobject_class, PROP_FINGERPRINT, 
	         g_param_spec_string ("fingerprint", "fingerprint", "fingerprint", NULL, 
	                              G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	
	g_object_class_install_property (gobject_class, PROP_VALIDITY, 
	         g_param_spec_uint ("validity", "validity", "validity", 0, G_MAXUINT, 0U, 
	                            G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	
	g_object_class_install_property (gobject_class, PROP_VALIDITY_STR, 
	         g_param_spec_string ("validity-str", "validity-str", "validity-str", NULL, 
	                              G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	
	g_object_class_install_property (gobject_class, PROP_TRUST, 
	         g_param_spec_uint ("trust", "trust", "trust", 0, G_MAXUINT, 0U, 
	                            G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	
	g_object_class_install_property (gobject_class, PROP_TRUST_STR, 
	         g_param_spec_string ("trust-str", "trust-str", "trust-str", NULL, 
	                              G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	
	g_object_class_install_property (gobject_class, PROP_EXPIRES, 
	         g_param_spec_ulong ("expires", "expires", "expires", 0, G_MAXULONG, 0UL, 
	                             G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	
	g_object_class_install_property (gobject_class, PROP_EXPIRES_STR, 
	         g_param_spec_string ("expires-str", "expires-str", "expires-str", NULL, 
	                              G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
}

const guchar*
seahorse_pkcs11_certificate_get_der_data (CruiX509Cert *self, gsize *n_length)
{
	SeahorsePkcs11CertificatePrivate *pv;
	
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	
	pv = SEAHORSE_PKCS11_CERTIFICATE_GET_PRIVATE (self);
	g_return_val_if_fail (pv->pkcs11_attributes, NULL);
	
	if (gp11_attribute_is_invalid (&pv->der_value)) {
		GP11Attribute *attr = gp11_attributes_find (pv->pkcs11_attributes, CKA_VALUE);
		g_return_val_if_fail (attr, NULL);
		gp11_attribute_clear (&pv->der_value);
		gp11_attribute_init_copy (&pv->der_value, attr);
	}
	
	g_return_val_if_fail (!gp11_attribute_is_invalid (&pv->der_value), NULL);
	*n_length = pv->der_value.length;
	return pv->der_value.value;
}

static void 
seahorse_pkcs11_certificate_iface (CruiX509CertIface *iface)
{
	iface->get_der_data = (gpointer)seahorse_pkcs11_certificate_get_der_data;
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorsePkcs11Certificate*
seahorse_pkcs11_certificate_new (GP11Object* object, GP11Attributes* attributes)
{
	return g_object_new (SEAHORSE_PKCS11_TYPE_CERTIFICATE, 
	                     "pkcs11-object", object, 
	                     "pkcs11-attributes", attributes, NULL);
}

GP11Object* 
seahorse_pkcs11_certificate_get_pkcs11_object (SeahorsePkcs11Certificate* self) 
{
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	return SEAHORSE_PKCS11_CERTIFICATE_GET_PRIVATE (self)->pkcs11_object;
}

void 
seahorse_pkcs11_certificate_set_pkcs11_object (SeahorsePkcs11Certificate* self, GP11Object* value) 
{
	SeahorsePkcs11CertificatePrivate *pv;
	
	g_return_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self));
	pv = SEAHORSE_PKCS11_CERTIFICATE_GET_PRIVATE (self);
	
	if (pv->pkcs11_object)
		g_object_unref (pv->pkcs11_object);
	pv->pkcs11_object = value;
	if (pv->pkcs11_object)
		g_object_ref (pv->pkcs11_object);
	
	certificate_rebuild (self);
	g_object_notify (G_OBJECT (self), "pkcs11-object");
}

GP11Attributes* 
seahorse_pkcs11_certificate_get_pkcs11_attributes (SeahorsePkcs11Certificate* self) 
{
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	return SEAHORSE_PKCS11_CERTIFICATE_GET_PRIVATE (self)->pkcs11_attributes;
}


void 
seahorse_pkcs11_certificate_set_pkcs11_attributes (SeahorsePkcs11Certificate* self, GP11Attributes* value) 
{
	SeahorsePkcs11CertificatePrivate *pv;
	
	g_return_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self));
	pv = SEAHORSE_PKCS11_CERTIFICATE_GET_PRIVATE (self);
	
	if (pv->pkcs11_attributes)
		gp11_attributes_unref (pv->pkcs11_attributes);
	pv->pkcs11_attributes = value;
	if (pv->pkcs11_attributes)
		gp11_attributes_ref (pv->pkcs11_attributes);
	
	certificate_rebuild (self);
	g_object_notify (G_OBJECT (self), "pkcs11-attributes");
}

char* 
seahorse_pkcs11_certificate_get_fingerprint (SeahorsePkcs11Certificate* self) 
{
	SeahorsePkcs11CertificatePrivate *pv;
	GP11Attribute* attr;
	
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	pv = SEAHORSE_PKCS11_CERTIFICATE_GET_PRIVATE (self);
	
	/* TODO: We should be using the fingerprint off the key */
	if (pv->pkcs11_attributes == NULL) 
		return g_strdup ("");

	attr = gp11_attributes_find (pv->pkcs11_attributes, CKA_ID);
	if (attr == NULL)
		return g_strdup ("");

	return seahorse_util_hex_encode (attr->value, attr->length);
}

guint 
seahorse_pkcs11_certificate_get_validity (SeahorsePkcs11Certificate* self) 
{
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), 0U);
	
	/* TODO: We need to implement proper validity checking */
	return SEAHORSE_VALIDITY_UNKNOWN;
}

const char* 
seahorse_pkcs11_certificate_get_validity_str (SeahorsePkcs11Certificate* self) 
{
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	return seahorse_validity_get_string (seahorse_pkcs11_certificate_get_validity (self));
}

guint 
seahorse_pkcs11_certificate_get_trust (SeahorsePkcs11Certificate* self) 
{
	SeahorsePkcs11CertificatePrivate *pv;
	gulong trust;
	
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), 0U);
	pv = SEAHORSE_PKCS11_CERTIFICATE_GET_PRIVATE (self);
	
	trust = 0UL;
	if (pv->pkcs11_attributes == NULL || 
	    !gp11_attributes_find_ulong (pv->pkcs11_attributes, CKA_GNOME_USER_TRUST, &trust)) 
		return SEAHORSE_VALIDITY_UNKNOWN;
	
	if (trust == CKT_GNOME_TRUSTED)
		return SEAHORSE_VALIDITY_FULL;
	else if (trust == CKT_GNOME_UNTRUSTED)
		return SEAHORSE_VALIDITY_NEVER;
	else
		return SEAHORSE_VALIDITY_UNKNOWN;
}

const char* 
seahorse_pkcs11_certificate_get_trust_str (SeahorsePkcs11Certificate* self) 
{
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	return seahorse_validity_get_string (seahorse_pkcs11_certificate_get_trust (self));
}


gulong
seahorse_pkcs11_certificate_get_expires (SeahorsePkcs11Certificate* self) 
{
	SeahorsePkcs11CertificatePrivate *pv;

	GDate date = {0};
	struct tm time = {0};

	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), 0UL);
	pv = SEAHORSE_PKCS11_CERTIFICATE_GET_PRIVATE (self);
	
	if (pv->pkcs11_attributes == NULL || 
	    !gp11_attributes_find_date (pv->pkcs11_attributes, CKA_END_DATE, &date))
		return 0;

	g_date_to_struct_tm (&date, &time);
	return (gulong)(mktime (&time));
}

char* 
seahorse_pkcs11_certificate_get_expires_str (SeahorsePkcs11Certificate* self) 
{
	gulong expiry;
	
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	
	/* TODO: When expired return Expired */
	expiry = seahorse_pkcs11_certificate_get_expires (self);
	if (expiry == 0)
		return g_strdup ("");
	return seahorse_util_get_date_string (expiry);
}

