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

#include "crui-x509-cert.h"

#include "crui-asn1.h"
#include "crui-util.h"

#include <libtasn1.h>

#include <string.h>

typedef struct _Asn1Cache {
	ASN1_TYPE asn1;
	gconstpointer der;
	gsize length;
} Asn1Cache;

static GQuark ASN1_CACHE = 0;

static void
free_asn1_cache (gpointer data)
{
	Asn1Cache *cache = (Asn1Cache*)data;
	if (cache) {
		g_assert (cache->asn1);
		asn1_delete_structure (&cache->asn1);
		g_free (cache);
	}
}

static ASN1_TYPE
parse_certificate_asn1 (CruiX509Cert *cert)
{
	Asn1Cache *cache;
	ASN1_TYPE asn1;
	const guchar *der;
	gsize n_der;
	
	g_assert (cert);
	
	der = crui_x509_cert_get_der_data (cert, &n_der);
	g_return_val_if_fail (der, NULL);

	cache = (Asn1Cache*)g_object_get_qdata (G_OBJECT (cert), ASN1_CACHE);
	if (cache) {
		if (n_der == cache->length && memcmp (der, cache->der, n_der) == 0)
			return cache->asn1;
	}
	
	/* Cache is invalid or non existent */
	asn1 = _crui_asn1_decode ("PKIX1.Certificate", der, n_der);
	if (asn1 == NULL) {
		g_warning ("a derived class provided an invalid or unparseable X509 DER certificate data.");
		return NULL;
	}
	
	cache = g_new0 (Asn1Cache, 1);
	cache->der = der;
	cache->length = n_der;
	cache->asn1 = asn1;
	
	g_object_set_qdata_full (G_OBJECT (cert), ASN1_CACHE, cache, free_asn1_cache);
	return asn1;
}

static GChecksum*
digest_certificate (CruiX509Cert *cert, GChecksumType type)
{
	GChecksum *digest;
	const guchar *der;
	gsize n_der;
	
	g_assert (cert);

	der = crui_x509_cert_get_der_data (cert, &n_der);
	g_return_val_if_fail (der, NULL);
	
	digest = g_checksum_new (type);
	g_return_val_if_fail (digest, NULL);
	
	g_checksum_update (digest, der, n_der);
	return digest;
}

/* ---------------------------------------------------------------------------------
 * INTERFACE
 */

static void
crui_x509_cert_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;
	if (!initialized) {
		ASN1_CACHE = g_quark_from_static_string ("_crui_x509_cert_asn1_cache");
		
		/* Add properties and signals to the interface */
		
		
		initialized = TRUE;
	}
}

GType
crui_x509_cert_get_type (void)
{
	static GType type = 0;
	if (!type) {
		static const GTypeInfo info = {
			sizeof (CruiX509CertIface),
			crui_x509_cert_base_init,               /* base init */
			NULL,             /* base finalize */
			NULL,             /* class_init */
			NULL,             /* class finalize */
			NULL,             /* class data */
			0,
			0,                /* n_preallocs */
			NULL,             /* instance init */
		};
		type = g_type_register_static (G_TYPE_INTERFACE, "CruiX509CertIface", &info, 0);
		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}
	
	return type;
}

/* -----------------------------------------------------------------------------
 * PUBLIC
 */

const guchar*
crui_x509_cert_get_der_data (CruiX509Cert *self, gsize *n_length)
{
	g_return_val_if_fail (CRUI_IS_X509_CERT (self), NULL);
	g_return_val_if_fail (CRUI_X509_CERT_GET_INTERFACE (self)->get_der_data, NULL);
	return CRUI_X509_CERT_GET_INTERFACE (self)->get_der_data (self, n_length);
}

gchar*
crui_x509_cert_get_issuer_cn (CruiX509Cert *self)
{
	return crui_x509_cert_get_issuer_part (self, "cn");
}

gchar*
crui_x509_cert_get_issuer_part (CruiX509Cert *self, const char *part)
{
	ASN1_TYPE asn1;
	
	g_return_val_if_fail (CRUI_IS_X509_CERT (self), NULL);
	
	asn1 = parse_certificate_asn1 (self);
	g_return_val_if_fail (asn1, NULL);
	
	return _crui_asn1_read_dn_part (asn1, "tbsCertificate.issuer.rdnSequence", part); 	
}

gchar*
crui_x509_cert_get_issuer_dn (CruiX509Cert *self)
{
	ASN1_TYPE asn1;
	
	g_return_val_if_fail (CRUI_IS_X509_CERT (self), NULL);
	
	asn1 = parse_certificate_asn1 (self);
	g_return_val_if_fail (asn1, NULL);
	
	return _crui_asn1_read_dn (asn1, "tbsCertificate.issuer.rdnSequence"); 
}

gchar* 
crui_x509_cert_get_subject_cn (CruiX509Cert *self)
{
	return crui_x509_cert_get_subject_part (self, "cn");
}

gchar* 
crui_x509_cert_get_subject_part (CruiX509Cert *self, const char *part)
{
	ASN1_TYPE asn1;
	
	g_return_val_if_fail (CRUI_IS_X509_CERT (self), NULL);
	
	asn1 = parse_certificate_asn1 (self);
	g_return_val_if_fail (asn1, NULL);
	
	return _crui_asn1_read_dn_part (asn1, "tbsCertificate.subject.rdnSequence", part); 
}

gchar* 
crui_x509_cert_get_subject_dn (CruiX509Cert *self)
{
	ASN1_TYPE asn1;
	
	g_return_val_if_fail (CRUI_IS_X509_CERT (self), NULL);
	
	asn1 = parse_certificate_asn1 (self);
	g_return_val_if_fail (asn1, NULL);
	
	return _crui_asn1_read_dn (asn1, "tbsCertificate.issuer.rdnSequence"); 	
}

GDate* 
crui_x509_cert_get_issued_date (CruiX509Cert *self)
{
	ASN1_TYPE asn1;
	
	g_return_val_if_fail (CRUI_IS_X509_CERT (self), NULL);
	
	asn1 = parse_certificate_asn1 (self);
	g_return_val_if_fail (asn1, NULL);
	
	return _crui_asn1_read_date (asn1, "tbsCertificate.validity.notBefore"); 
}

GDate* 
crui_x509_cert_get_expiry_date (CruiX509Cert *self)
{
	ASN1_TYPE asn1;
	
	g_return_val_if_fail (CRUI_IS_X509_CERT (self), NULL);
	
	asn1 = parse_certificate_asn1 (self);
	g_return_val_if_fail (asn1, NULL);
	
	return _crui_asn1_read_date (asn1, "tbsCertificate.validity.notAfter"); 
}

guchar*
crui_x509_cert_get_fingerprint (CruiX509Cert *self, GChecksumType type, gsize *n_digest)
{
	GChecksum *sum;
	guchar *digest;
	gssize length;
	
	g_return_val_if_fail (CRUI_IS_X509_CERT (self), NULL);
	g_return_val_if_fail (n_digest, NULL);
	
	sum = digest_certificate (self, type);
	g_return_val_if_fail (sum, NULL);
	length = g_checksum_type_get_length (type);
	g_return_val_if_fail (length > 0, NULL);
	digest = g_malloc (length);
	*n_digest = length;
	g_checksum_get_digest (sum, digest, n_digest);
	g_checksum_free (sum);
	
	return digest;
}

gchar*
crui_x509_cert_get_fingerprint_hex (CruiX509Cert *self, GChecksumType type)
{
	GChecksum *sum;
	gchar *hex;
	
	g_return_val_if_fail (CRUI_IS_X509_CERT (self), NULL);
	
	sum = digest_certificate (self, type);
	g_return_val_if_fail (sum, NULL);
	hex = g_strdup (g_checksum_get_string (sum));
	g_checksum_free (sum);
	return hex;
}

guchar*
crui_x509_cert_get_serial_number (CruiX509Cert *self, gsize *n_length)
{
	ASN1_TYPE asn1;
	
	g_return_val_if_fail (CRUI_IS_X509_CERT (self), NULL);
	
	asn1 = parse_certificate_asn1 (self);
	g_return_val_if_fail (asn1, NULL);
	
	return _crui_asn1_read_value (asn1, "tbsCertificate.serialNumber", n_length); 
}

gchar*
crui_x509_cert_get_serial_number_hex (CruiX509Cert *self)
{
	guchar *serial;
	gsize n_serial;
	gchar *hex;
	
	g_return_val_if_fail (CRUI_IS_X509_CERT (self), NULL);
	
	serial = crui_x509_cert_get_serial_number (self, &n_serial);
	if (serial == NULL)
		return NULL;
	
	hex = _crui_util_encode_hex (serial, n_serial);
	g_free (serial);
	return hex;
}
