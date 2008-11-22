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

#include "crui-x509-cert-basics.h"

#include "crui-x509-cert.h"

#include <glib/gi18n-lib.h>

enum {
	PROP_0,
	PROP_CERTIFICATE
};

struct _CruiX509CertBasicsPrivate {
	CruiX509Cert *certificate;
	GtkBuilder *builder;
};

G_DEFINE_TYPE (CruiX509CertBasics, crui_x509_cert_basics, GTK_TYPE_ALIGNMENT);

#define CRUI_X509_CERT_BASICS_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), CRUI_TYPE_X509_CERT_BASICS, CruiX509CertBasicsPrivate))

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static void
set_certificate_part_label (GtkBuilder *builder, const char *name, const gchar *value)
{
	GtkWidget *widget;
	gchar *markup;
	
	widget = GTK_WIDGET (gtk_builder_get_object (builder, name));
	g_return_if_fail (GTK_IS_LABEL (widget));
	if(value)
	{
		markup = g_markup_escape_text (value, -1);
		gtk_label_set_markup (GTK_LABEL (widget), markup);
		g_free (markup);
	}
	else
	{
		gtk_label_set_markup (GTK_LABEL (widget), _("<i>Not Part of Certificate</i>"));
	}
}

static void
set_certificate_part_date (GtkBuilder *builder, const char *name, const GDate *value)
{
	GtkWidget *widget;
	gchar *formatted;
	
	widget = GTK_WIDGET (gtk_builder_get_object (builder, name));
	g_return_if_fail (GTK_IS_LABEL (widget));
	if(value)
	{
		formatted = g_new (gchar, 11);
		g_date_strftime (formatted, 11, "%Y-%m-%d", value);
		gtk_label_set_text (GTK_LABEL (widget), formatted);
		g_free (formatted);
	}
	else
	{
		gtk_label_set_markup (GTK_LABEL (widget), _("<i>unknown</i>"));
	}
}

static void
refresh_display (CruiX509CertBasics *self)
{
	CruiX509CertBasicsPrivate *pv = CRUI_X509_CERT_BASICS_GET_PRIVATE (self);
	gchar *value;
	GDate *date;
	
	/* Issued To / Subject */
	
	value = NULL;
	if (pv->certificate)
		value = crui_x509_cert_get_subject_cn (pv->certificate);
	set_certificate_part_label (pv->builder, "issued-to-cn", value);
	g_free (value);
	
	value = NULL;
	if (pv->certificate)
		value = crui_x509_cert_get_subject_part (pv->certificate, "o");
	set_certificate_part_label (pv->builder, "issued-to-o", value);
	g_free (value);

	value = NULL;
	if (pv->certificate)
		value = crui_x509_cert_get_subject_part (pv->certificate, "ou");
	set_certificate_part_label (pv->builder, "issued-to-ou", value);
	g_free (value);

	value = NULL;
	if (pv->certificate)
		value = crui_x509_cert_get_serial_number_hex (pv->certificate);
	set_certificate_part_label (pv->builder, "issued-to-serial", value);
	g_free (value);
	
	
	/* Issued By / Issuer */
	
	value = NULL;
	if (pv->certificate)
		value = crui_x509_cert_get_issuer_cn (pv->certificate);
	set_certificate_part_label (pv->builder, "issued-by-cn", value);
	g_free (value);
	
	value = NULL;
	if (pv->certificate)
		value = crui_x509_cert_get_issuer_part (pv->certificate, "o");
	set_certificate_part_label (pv->builder, "issued-by-o", value);
	g_free (value);

	value = NULL;
	if (pv->certificate)
		value = crui_x509_cert_get_issuer_part (pv->certificate, "ou");
	set_certificate_part_label (pv->builder, "issued-by-ou", value);
	g_free (value);

	
	/* Expiry */
	
	date = NULL;
	if (pv->certificate)
		date = crui_x509_cert_get_issued_date (pv->certificate);
	set_certificate_part_date (pv->builder, "validity-issued-on", date);
	if (date)
		g_date_free (date);
	
	date = NULL;
	if (pv->certificate)
		date = crui_x509_cert_get_expiry_date (pv->certificate);
	set_certificate_part_date (pv->builder, "validity-expires-on", date);
	if (date)
		g_date_free (date);

	
	/* Fingerprints */
	value = NULL;
	if (pv->certificate)
		value = crui_x509_cert_get_fingerprint_hex (pv->certificate, G_CHECKSUM_SHA1);
	set_certificate_part_label (pv->builder, "fingerprints-sha1", value);
	g_free (value);
	
	value = NULL;
	if (pv->certificate)
		value = crui_x509_cert_get_fingerprint_hex (pv->certificate, G_CHECKSUM_SHA1);
	set_certificate_part_label (pv->builder, "fingerprints-md5", value);
	g_free (value);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */


static GObject* 
crui_x509_cert_basics_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	GObject *obj = G_OBJECT_CLASS (crui_x509_cert_basics_parent_class)->constructor (type, n_props, props);
	CruiX509CertBasics *self = NULL;
	CruiX509CertBasicsPrivate *pv;
	GtkWidget *widget;
	
	if (obj) {
		pv = CRUI_X509_CERT_BASICS_GET_PRIVATE (obj);
		self = CRUI_X509_CERT_BASICS (obj);
		
		if (!gtk_builder_add_from_file (pv->builder, UIDIR "crui-x509-cert-basics.ui", NULL))
			g_return_val_if_reached (obj);
	
		widget = GTK_WIDGET (gtk_builder_get_object (pv->builder, "crui-x509-cert-basics"));
		g_return_val_if_fail (GTK_IS_WIDGET (widget), obj);
		gtk_container_add (GTK_CONTAINER (self), widget);
		gtk_widget_show (widget);
	}
	
	return obj;
}

static void
crui_x509_cert_basics_init (CruiX509CertBasics *self)
{
	CruiX509CertBasicsPrivate *pv = CRUI_X509_CERT_BASICS_GET_PRIVATE (self);
	pv->builder = gtk_builder_new ();
}

static void
crui_x509_cert_basics_dispose (GObject *obj)
{
#if 0
	CruiX509CertBasics *self = CRUI_X509_CERT_BASICS (obj);
	CruiX509CertBasicsPrivate *pv = CRUI_X509_CERT_BASICS_GET_PRIVATE (self);
#endif 
	
	G_OBJECT_CLASS (crui_x509_cert_basics_parent_class)->dispose (obj);
}

static void
crui_x509_cert_basics_finalize (GObject *obj)
{
#if 0
	CruiX509CertBasics *self = CRUI_X509_CERT_BASICS (obj);
	CruiX509CertBasicsPrivate *pv = CRUI_X509_CERT_BASICS_GET_PRIVATE (self);
#endif
	
	G_OBJECT_CLASS (crui_x509_cert_basics_parent_class)->finalize (obj);
}

static void
crui_x509_cert_basics_set_property (GObject *obj, guint prop_id, const GValue *value, 
                                    GParamSpec *pspec)
{
	CruiX509CertBasics *self = CRUI_X509_CERT_BASICS (obj);
	CruiX509CertBasicsPrivate *pv = CRUI_X509_CERT_BASICS_GET_PRIVATE (self);
	
	switch (prop_id) {
	case PROP_CERTIFICATE:
		crui_x509_cert_basics_set_certificate (self, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
crui_x509_cert_basics_get_property (GObject *obj, guint prop_id, GValue *value, 
                                    GParamSpec *pspec)
{
	CruiX509CertBasics *self = CRUI_X509_CERT_BASICS (obj);
	CruiX509CertBasicsPrivate *pv = CRUI_X509_CERT_BASICS_GET_PRIVATE (self);
	
	switch (prop_id) {
	case PROP_CERTIFICATE:
		g_value_set_object (value, pv->certificate);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
crui_x509_cert_basics_class_init (CruiX509CertBasicsClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    
	crui_x509_cert_basics_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (CruiX509CertBasicsPrivate));

	gobject_class->constructor = crui_x509_cert_basics_constructor;
	gobject_class->dispose = crui_x509_cert_basics_dispose;
	gobject_class->finalize = crui_x509_cert_basics_finalize;
	gobject_class->set_property = crui_x509_cert_basics_set_property;
	gobject_class->get_property = crui_x509_cert_basics_get_property;
    
	g_object_class_install_property (gobject_class, PROP_CERTIFICATE,
	           g_param_spec_object("certificate", "Certificate", "Certificate to display.", 
	                               CRUI_TYPE_X509_CERT, G_PARAM_READWRITE));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

CruiX509CertBasics*
crui_x509_cert_basics_new (CruiX509Cert *certificate)
{
	return g_object_new (CRUI_TYPE_X509_CERT_BASICS, "certificate", certificate, NULL);
}

CruiX509Cert*
crui_x509_cert_basics_get_certificate (CruiX509CertBasics *self)
{
	g_return_val_if_fail (CRUI_IS_X509_CERT_BASICS (self), NULL);
	return CRUI_X509_CERT_BASICS_GET_PRIVATE (self)->certificate;
}

void
crui_x509_cert_basics_set_certificate (CruiX509CertBasics *self, CruiX509Cert *cert)
{
	CruiX509CertBasicsPrivate *pv; 
	g_return_if_fail (CRUI_IS_X509_CERT_BASICS (self));
	
	pv = CRUI_X509_CERT_BASICS_GET_PRIVATE (self);
	if (pv->certificate)
		g_object_unref (pv->certificate);
	pv->certificate = cert;
	if (pv->certificate)
		g_object_ref (pv->certificate);	
	
	refresh_display (self);
	g_object_notify (G_OBJECT (self), "certificate");
}

