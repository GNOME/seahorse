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

#include "crui-x509-cert-simple.h"

#include "crui-x509-cert.h"

struct _CruiX509CertSimplePrivate {
	guchar *der;
	gsize n_der;
};

static void crui_x509_cert_iface (CruiX509CertIface *iface);

G_DEFINE_TYPE_EXTENDED (CruiX509CertSimple, crui_x509_cert_simple, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (CRUI_TYPE_X509_CERT, crui_x509_cert_iface));

#define CRUI_X509_CERT_SIMPLE_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), CRUI_TYPE_X509_CERT_SIMPLE, CruiX509CertSimplePrivate))

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
crui_x509_cert_simple_init (CruiX509CertSimple *self)
{
#if 0
	CruiX509CertSimplePrivate *pv = CRUI_X509_CERT_SIMPLE_GET_PRIVATE (self);
#endif
}

static void
crui_x509_cert_simple_finalize (GObject *obj)
{
	CruiX509CertSimple *self = CRUI_X509_CERT_SIMPLE (obj);
	CruiX509CertSimplePrivate *pv = CRUI_X509_CERT_SIMPLE_GET_PRIVATE (self);
	
	g_free (pv->der);
	pv->der = NULL;
	pv->n_der = 0;

	G_OBJECT_CLASS (crui_x509_cert_simple_parent_class)->finalize (obj);
}

static void
crui_x509_cert_simple_set_property (GObject *obj, guint prop_id, const GValue *value, 
                           GParamSpec *pspec)
{
#if 0
	CruiX509CertSimplePrivate *pv = CRUI_X509_CERT_SIMPLE_GET_PRIVATE (obj);
	CruiX509CertSimple *self = CRUI_X509_CERT_SIMPLE (obj);
#endif
	
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
crui_x509_cert_simple_get_property (GObject *obj, guint prop_id, GValue *value, 
                           GParamSpec *pspec)
{
#if 0
	CruiX509CertSimplePrivate *pv = CRUI_X509_CERT_SIMPLE_GET_PRIVATE (obj);
	CruiX509CertSimple *self = CRUI_X509_CERT_SIMPLE (obj);
#endif
	
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
crui_x509_cert_simple_class_init (CruiX509CertSimpleClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    
	crui_x509_cert_simple_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (CruiX509CertSimplePrivate));

	gobject_class->finalize = crui_x509_cert_simple_finalize;
	gobject_class->set_property = crui_x509_cert_simple_set_property;
	gobject_class->get_property = crui_x509_cert_simple_get_property;
}

const guchar*
crui_x509_cert_simple_get_der_data (CruiX509Cert *self, gsize *n_length)
{
	CruiX509CertSimplePrivate *pv = CRUI_X509_CERT_SIMPLE_GET_PRIVATE (self);
	g_return_val_if_fail (pv->der && pv->n_der, NULL);
	*n_length = pv->n_der;
	return pv->der;
}

static void 
crui_x509_cert_iface (CruiX509CertIface *iface) 
{
	iface->get_der_data = (gpointer)crui_x509_cert_simple_get_der_data;
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

CruiX509CertSimple*
crui_x509_cert_simple_new (const guchar *der, gsize n_der)
{
	CruiX509CertSimple *simple;
	CruiX509CertSimplePrivate *pv;
	
	g_return_val_if_fail (der, NULL);
	g_return_val_if_fail (n_der, NULL);
	
	simple = g_object_new (CRUI_TYPE_X509_CERT_SIMPLE, NULL);
	if (simple) {
		pv = CRUI_X509_CERT_SIMPLE_GET_PRIVATE (simple);
		pv->der = g_memdup (der, n_der);
		pv->n_der = n_der;
	}
	
	return simple;
}

CruiX509CertSimple* 
crui_x509_cert_simple_new_from_file (const gchar *filename, GError **err)
{
	CruiX509CertSimple *simple;
	gchar *contents;
	gsize n_contents;
	
	g_return_val_if_fail (filename, NULL);
	g_return_val_if_fail (!err || !*err, NULL);
	
	if (!g_file_get_contents (filename, &contents, &n_contents, err))
		return NULL;
	
	simple = crui_x509_cert_simple_new ((guchar*)contents, n_contents);
	g_free (contents);
	return simple;
}
