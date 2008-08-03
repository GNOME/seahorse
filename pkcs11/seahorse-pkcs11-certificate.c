
#include "seahorse-pkcs11-certificate.h"
#include <seahorse-types.h>
#include <seahorse-key.h>
#include <pkcs11.h>
#include <pkcs11g.h>
#include <glib/gi18n-lib.h>
#include <seahorse-util.h>
#include <seahorse-validity.h>
#include <time.h>
#include "seahorse-pkcs11.h"




struct _SeahorsePkcs11CertificatePrivate {
	GP11Object* _pkcs11_object;
	GP11Attributes* _pkcs11_attributes;
};

#define SEAHORSE_PKCS11_CERTIFICATE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_PKCS11_TYPE_CERTIFICATE, SeahorsePkcs11CertificatePrivate))
enum  {
	SEAHORSE_PKCS11_CERTIFICATE_DUMMY_PROPERTY,
	SEAHORSE_PKCS11_CERTIFICATE_PKCS11_OBJECT,
	SEAHORSE_PKCS11_CERTIFICATE_PKCS11_ATTRIBUTES,
	SEAHORSE_PKCS11_CERTIFICATE_DISPLAY_NAME,
	SEAHORSE_PKCS11_CERTIFICATE_DISPLAY_ID,
	SEAHORSE_PKCS11_CERTIFICATE_MARKUP,
	SEAHORSE_PKCS11_CERTIFICATE_SIMPLE_NAME,
	SEAHORSE_PKCS11_CERTIFICATE_FINGERPRINT,
	SEAHORSE_PKCS11_CERTIFICATE_VALIDITY,
	SEAHORSE_PKCS11_CERTIFICATE_VALIDITY_STR,
	SEAHORSE_PKCS11_CERTIFICATE_TRUST,
	SEAHORSE_PKCS11_CERTIFICATE_TRUST_STR,
	SEAHORSE_PKCS11_CERTIFICATE_EXPIRES,
	SEAHORSE_PKCS11_CERTIFICATE_EXPIRES_STR,
	SEAHORSE_PKCS11_CERTIFICATE_STOCK_ID
};
static void seahorse_pkcs11_certificate_rebuild (SeahorsePkcs11Certificate* self);
static gpointer seahorse_pkcs11_certificate_parent_class = NULL;
static void seahorse_pkcs11_certificate_dispose (GObject * obj);



SeahorsePkcs11Certificate* seahorse_pkcs11_certificate_new (GP11Object* object, GP11Attributes* attributes) {
	SeahorsePkcs11Certificate * self;
	g_return_val_if_fail (GP11_IS_OBJECT (object), NULL);
	g_return_val_if_fail (attributes != NULL, NULL);
	self = g_object_newv (SEAHORSE_PKCS11_TYPE_CERTIFICATE, 0, NULL);
	seahorse_pkcs11_certificate_set_pkcs11_object (self, object);
	seahorse_pkcs11_certificate_set_pkcs11_attributes (self, attributes);
	return self;
}


static void seahorse_pkcs11_certificate_rebuild (SeahorsePkcs11Certificate* self) {
	g_return_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self));
	SEAHORSE_OBJECT (self)->_id = ((GQuark) (0));
	SEAHORSE_OBJECT (self)->_tag = SEAHORSE_PKCS11_TYPE;
	if (self->priv->_pkcs11_attributes == NULL) {
		SEAHORSE_OBJECT (self)->_location = SEAHORSE_LOCATION_INVALID;
		SEAHORSE_OBJECT (self)->_usage = SEAHORSE_USAGE_NONE;
		SEAHORSE_OBJECT (self)->_flags = ((guint) (SKEY_FLAG_DISABLED));
	} else {
		SEAHORSE_OBJECT (self)->_id = seahorse_pkcs11_id_from_attributes (self->priv->_pkcs11_attributes);
		SEAHORSE_OBJECT (self)->_location = SEAHORSE_LOCATION_LOCAL;
		SEAHORSE_OBJECT (self)->_usage = SEAHORSE_USAGE_PUBLIC_KEY;
		SEAHORSE_OBJECT (self)->_flags = ((guint) (0));
		/* TODO: Expiry, revoked, disabled etc... */
		if (seahorse_pkcs11_certificate_get_trust (self) >= ((gint) (SEAHORSE_VALIDITY_MARGINAL))) {
			SEAHORSE_OBJECT (self)->_flags = SEAHORSE_OBJECT (self)->_flags | (SKEY_FLAG_TRUSTED);
		}
	}
	seahorse_object_fire_changed (SEAHORSE_OBJECT (self), SEAHORSE_OBJECT_CHANGE_ALL);
}


GP11Object* seahorse_pkcs11_certificate_get_pkcs11_object (SeahorsePkcs11Certificate* self) {
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	return self->priv->_pkcs11_object;
}


void seahorse_pkcs11_certificate_set_pkcs11_object (SeahorsePkcs11Certificate* self, GP11Object* value) {
	GP11Object* _tmp2;
	GP11Object* _tmp1;
	g_return_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self));
	_tmp2 = NULL;
	_tmp1 = NULL;
	self->priv->_pkcs11_object = (_tmp2 = (_tmp1 = value, (_tmp1 == NULL ? NULL : g_object_ref (_tmp1))), (self->priv->_pkcs11_object == NULL ? NULL : (self->priv->_pkcs11_object = (g_object_unref (self->priv->_pkcs11_object), NULL))), _tmp2);
	g_object_notify (((GObject *) (self)), "pkcs11-object");
}


GP11Attributes* seahorse_pkcs11_certificate_get_pkcs11_attributes (SeahorsePkcs11Certificate* self) {
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	return self->priv->_pkcs11_attributes;
}


void seahorse_pkcs11_certificate_set_pkcs11_attributes (SeahorsePkcs11Certificate* self, GP11Attributes* value) {
	GP11Attributes* _tmp2;
	GP11Attributes* _tmp1;
	g_return_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self));
	_tmp2 = NULL;
	_tmp1 = NULL;
	self->priv->_pkcs11_attributes = (_tmp2 = (_tmp1 = value, (_tmp1 == NULL ? NULL : gp11_attributes_ref (_tmp1))), (self->priv->_pkcs11_attributes == NULL ? NULL : (self->priv->_pkcs11_attributes = (gp11_attributes_unref (self->priv->_pkcs11_attributes), NULL))), _tmp2);
	seahorse_pkcs11_certificate_rebuild (self);
	g_object_notify (((GObject *) (self)), "pkcs11-attributes");
}


static char* seahorse_pkcs11_certificate_real_get_display_name (SeahorsePkcs11Certificate* self) {
	const char* _tmp4;
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	if (self->priv->_pkcs11_attributes != NULL) {
		char* label;
		char* _tmp2;
		gboolean _tmp1;
		char* _tmp0;
		label = NULL;
		_tmp2 = NULL;
		_tmp0 = NULL;
		if ((_tmp1 = gp11_attributes_find_string (self->priv->_pkcs11_attributes, CKA_LABEL, &_tmp0), label = (_tmp2 = _tmp0, (label = (g_free (label), NULL)), _tmp2), _tmp1)) {
			return label;
		}
		label = (g_free (label), NULL);
	}
	/* TODO: Calculate something from the subject? */
	_tmp4 = NULL;
	return (_tmp4 = _ ("Certificate"), (_tmp4 == NULL ? NULL : g_strdup (_tmp4)));
}


char* seahorse_pkcs11_certificate_get_display_id (SeahorsePkcs11Certificate* self) {
	const char* _tmp0;
	char* id;
	char* _tmp2;
	char* _tmp3;
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	_tmp0 = NULL;
	id = (_tmp0 = seahorse_pkcs11_certificate_get_fingerprint (self), (_tmp0 == NULL ? NULL : g_strdup (_tmp0)));
	if (g_utf8_strlen (id, -1) <= 8) {
		return id;
	}
	_tmp2 = NULL;
	_tmp3 = NULL;
	return (_tmp3 = (_tmp2 = g_utf8_offset_to_pointer (id, g_utf8_strlen (id, -1) - 8), g_strndup (_tmp2, g_utf8_offset_to_pointer (_tmp2, ((glong) (8))) - _tmp2)), (id = (g_free (id), NULL)), _tmp3);
}


static char* seahorse_pkcs11_certificate_real_get_markup (SeahorsePkcs11Certificate* self) {
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	return g_markup_escape_text (seahorse_object_get_display_name (SEAHORSE_OBJECT (self)), -1);
}


const char* seahorse_pkcs11_certificate_get_simple_name (SeahorsePkcs11Certificate* self) {
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	return seahorse_object_get_display_name (SEAHORSE_OBJECT (self));
}


char* seahorse_pkcs11_certificate_get_fingerprint (SeahorsePkcs11Certificate* self) {
	GP11Attribute* attr;
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	/* TODO: We should be using the fingerprint off the key */
	if (self->priv->_pkcs11_attributes == NULL) {
		return g_strdup ("");
	}
	attr = gp11_attributes_find (self->priv->_pkcs11_attributes, CKA_ID);
	if (attr == NULL) {
		return g_strdup ("");
	}
	return seahorse_util_hex_encode (attr->value, attr->length);
}


gint seahorse_pkcs11_certificate_get_validity (SeahorsePkcs11Certificate* self) {
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), 0);
	/* TODO: We need to implement proper validity checking */
	;
	return ((gint) (SEAHORSE_VALIDITY_UNKNOWN));
}


char* seahorse_pkcs11_certificate_get_validity_str (SeahorsePkcs11Certificate* self) {
	const char* _tmp0;
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	_tmp0 = NULL;
	return (_tmp0 = seahorse_validity_get_string (((SeahorseValidity) (seahorse_pkcs11_certificate_get_validity (self)))), (_tmp0 == NULL ? NULL : g_strdup (_tmp0)));
}


gint seahorse_pkcs11_certificate_get_trust (SeahorsePkcs11Certificate* self) {
	gulong trust;
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), 0);
	trust = 0UL;
	if (self->priv->_pkcs11_attributes == NULL || !gp11_attributes_find_ulong (self->priv->_pkcs11_attributes, CKA_GNOME_USER_TRUST, &trust)) {
		return ((gint) (SEAHORSE_VALIDITY_UNKNOWN));
	}
	if (trust == CKT_GNOME_TRUSTED) {
		return ((gint) (SEAHORSE_VALIDITY_FULL));
	} else {
		if (trust == CKT_GNOME_UNTRUSTED) {
			return ((gint) (SEAHORSE_VALIDITY_NEVER));
		}
	}
	return ((gint) (SEAHORSE_VALIDITY_UNKNOWN));
}


char* seahorse_pkcs11_certificate_get_trust_str (SeahorsePkcs11Certificate* self) {
	const char* _tmp0;
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	_tmp0 = NULL;
	return (_tmp0 = seahorse_validity_get_string (((SeahorseValidity) (seahorse_pkcs11_certificate_get_trust (self)))), (_tmp0 == NULL ? NULL : g_strdup (_tmp0)));
}


gulong seahorse_pkcs11_certificate_get_expires (SeahorsePkcs11Certificate* self) {
	GDate date = {0};
	struct tm time = {0};
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), 0UL);
	if (self->priv->_pkcs11_attributes == NULL || !gp11_attributes_find_date (self->priv->_pkcs11_attributes, CKA_END_DATE, &date)) {
		return ((gulong) (0));
	}
	g_date_to_struct_tm (&date, &time);
	return ((gulong) (mktime (&time)));
}


char* seahorse_pkcs11_certificate_get_expires_str (SeahorsePkcs11Certificate* self) {
	gulong expiry;
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	/* TODO: When expired return Expired */
	expiry = seahorse_pkcs11_certificate_get_expires (self);
	if (expiry == 0) {
		return g_strdup ("");
	}
	return seahorse_util_get_date_string (expiry);
}


static char* seahorse_pkcs11_certificate_real_get_stock_id (SeahorsePkcs11Certificate* self) {
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (self), NULL);
	/* TODO: A certificate icon */
	return g_strdup ("");
}


static void seahorse_pkcs11_certificate_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SeahorsePkcs11Certificate * self;
	self = SEAHORSE_PKCS11_CERTIFICATE (object);
	switch (property_id) {
		case SEAHORSE_PKCS11_CERTIFICATE_PKCS11_OBJECT:
		g_value_set_object (value, seahorse_pkcs11_certificate_get_pkcs11_object (self));
		break;
		case SEAHORSE_PKCS11_CERTIFICATE_PKCS11_ATTRIBUTES:
		g_value_set_pointer (value, seahorse_pkcs11_certificate_get_pkcs11_attributes (self));
		break;
		case SEAHORSE_PKCS11_CERTIFICATE_DISPLAY_NAME:
		g_value_set_string (value, seahorse_pkcs11_certificate_real_get_display_name (self));
		break;
		case SEAHORSE_PKCS11_CERTIFICATE_DISPLAY_ID:
		g_value_set_string (value, seahorse_pkcs11_certificate_get_display_id (self));
		break;
		case SEAHORSE_PKCS11_CERTIFICATE_MARKUP:
		g_value_set_string (value, seahorse_pkcs11_certificate_real_get_markup (self));
		break;
		case SEAHORSE_PKCS11_CERTIFICATE_SIMPLE_NAME:
		g_value_set_string (value, seahorse_pkcs11_certificate_get_simple_name (self));
		break;
		case SEAHORSE_PKCS11_CERTIFICATE_FINGERPRINT:
		g_value_set_string (value, seahorse_pkcs11_certificate_get_fingerprint (self));
		break;
		case SEAHORSE_PKCS11_CERTIFICATE_VALIDITY:
		g_value_set_int (value, seahorse_pkcs11_certificate_get_validity (self));
		break;
		case SEAHORSE_PKCS11_CERTIFICATE_VALIDITY_STR:
		g_value_set_string (value, seahorse_pkcs11_certificate_get_validity_str (self));
		break;
		case SEAHORSE_PKCS11_CERTIFICATE_TRUST:
		g_value_set_int (value, seahorse_pkcs11_certificate_get_trust (self));
		break;
		case SEAHORSE_PKCS11_CERTIFICATE_TRUST_STR:
		g_value_set_string (value, seahorse_pkcs11_certificate_get_trust_str (self));
		break;
		case SEAHORSE_PKCS11_CERTIFICATE_EXPIRES:
		g_value_set_ulong (value, seahorse_pkcs11_certificate_get_expires (self));
		break;
		case SEAHORSE_PKCS11_CERTIFICATE_EXPIRES_STR:
		g_value_set_string (value, seahorse_pkcs11_certificate_get_expires_str (self));
		break;
		case SEAHORSE_PKCS11_CERTIFICATE_STOCK_ID:
		g_value_set_string (value, seahorse_pkcs11_certificate_real_get_stock_id (self));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_pkcs11_certificate_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
	SeahorsePkcs11Certificate * self;
	self = SEAHORSE_PKCS11_CERTIFICATE (object);
	switch (property_id) {
		case SEAHORSE_PKCS11_CERTIFICATE_PKCS11_OBJECT:
		seahorse_pkcs11_certificate_set_pkcs11_object (self, g_value_get_object (value));
		break;
		case SEAHORSE_PKCS11_CERTIFICATE_PKCS11_ATTRIBUTES:
		seahorse_pkcs11_certificate_set_pkcs11_attributes (self, g_value_get_pointer (value));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_pkcs11_certificate_class_init (SeahorsePkcs11CertificateClass * klass) {
	seahorse_pkcs11_certificate_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorsePkcs11CertificatePrivate));
	G_OBJECT_CLASS (klass)->get_property = seahorse_pkcs11_certificate_get_property;
	G_OBJECT_CLASS (klass)->set_property = seahorse_pkcs11_certificate_set_property;
	G_OBJECT_CLASS (klass)->dispose = seahorse_pkcs11_certificate_dispose;
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_CERTIFICATE_PKCS11_OBJECT, g_param_spec_object ("pkcs11-object", "pkcs11-object", "pkcs11-object", GP11_TYPE_OBJECT, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_CERTIFICATE_PKCS11_ATTRIBUTES, g_param_spec_pointer ("pkcs11-attributes", "pkcs11-attributes", "pkcs11-attributes", G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_CERTIFICATE_DISPLAY_NAME, "display-name");
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_CERTIFICATE_DISPLAY_ID, g_param_spec_string ("display-id", "display-id", "display-id", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_CERTIFICATE_MARKUP, "markup");
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_CERTIFICATE_SIMPLE_NAME, g_param_spec_string ("simple-name", "simple-name", "simple-name", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_CERTIFICATE_FINGERPRINT, g_param_spec_string ("fingerprint", "fingerprint", "fingerprint", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_CERTIFICATE_VALIDITY, g_param_spec_int ("validity", "validity", "validity", G_MININT, G_MAXINT, 0, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_CERTIFICATE_VALIDITY_STR, g_param_spec_string ("validity-str", "validity-str", "validity-str", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_CERTIFICATE_TRUST, g_param_spec_int ("trust", "trust", "trust", G_MININT, G_MAXINT, 0, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_CERTIFICATE_TRUST_STR, g_param_spec_string ("trust-str", "trust-str", "trust-str", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_CERTIFICATE_EXPIRES, g_param_spec_ulong ("expires", "expires", "expires", 0, G_MAXULONG, 0UL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_CERTIFICATE_EXPIRES_STR, g_param_spec_string ("expires-str", "expires-str", "expires-str", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_CERTIFICATE_STOCK_ID, "stock-id");
}


static void seahorse_pkcs11_certificate_instance_init (SeahorsePkcs11Certificate * self) {
	self->priv = SEAHORSE_PKCS11_CERTIFICATE_GET_PRIVATE (self);
}


static void seahorse_pkcs11_certificate_dispose (GObject * obj) {
	SeahorsePkcs11Certificate * self;
	self = SEAHORSE_PKCS11_CERTIFICATE (obj);
	(self->priv->_pkcs11_object == NULL ? NULL : (self->priv->_pkcs11_object = (g_object_unref (self->priv->_pkcs11_object), NULL)));
	(self->priv->_pkcs11_attributes == NULL ? NULL : (self->priv->_pkcs11_attributes = (gp11_attributes_unref (self->priv->_pkcs11_attributes), NULL)));
	G_OBJECT_CLASS (seahorse_pkcs11_certificate_parent_class)->dispose (obj);
}


GType seahorse_pkcs11_certificate_get_type (void) {
	static GType seahorse_pkcs11_certificate_type_id = 0;
	if (G_UNLIKELY (seahorse_pkcs11_certificate_type_id == 0)) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorsePkcs11CertificateClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_pkcs11_certificate_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorsePkcs11Certificate), 0, (GInstanceInitFunc) seahorse_pkcs11_certificate_instance_init };
		seahorse_pkcs11_certificate_type_id = g_type_register_static (SEAHORSE_TYPE_OBJECT, "SeahorsePkcs11Certificate", &g_define_type_info, 0);
	}
	return seahorse_pkcs11_certificate_type_id;
}




