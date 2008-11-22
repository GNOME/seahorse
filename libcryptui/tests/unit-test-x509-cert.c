
#include "config.h"
#include "run-tests.h"

#include "crui-x509-cert.h"
#include "crui-x509-cert-simple.h"

#include <glib.h>

#include <string.h>

static CruiX509Cert *certificate = NULL;

DEFINE_SETUP (certificate)
{
	GError *err = NULL;
	gchar *contents;
	gsize n_contents;
	
	if (!g_file_get_contents ("files/test-certificate-1.der", &contents, &n_contents, &err)) {
		g_warning ("couldn't read files/test-certificate-1.der: %s", err->message);
		return;
	}

	certificate = CRUI_X509_CERT (crui_x509_cert_simple_new ((const guchar*)contents, n_contents));
	g_assert (certificate);
	g_free (contents);
}

DEFINE_TEARDOWN (certificate)
{
	if (certificate)
		g_object_unref (certificate);
	certificate = NULL;
}

DEFINE_TEST (issuer_cn)
{
	gchar *cn = crui_x509_cert_get_issuer_cn (certificate);
	g_assert (cn);
	g_assert_cmpstr (cn, ==, "Thawte Personal Premium CA");
	g_free (cn);
}

DEFINE_TEST (issuer_dn)
{
	gchar *dn = crui_x509_cert_get_issuer_dn (certificate);
	g_assert (dn);
	g_assert_cmpstr (dn, ==, "C=ZA, ST=Western Cape, L=Cape Town, O=Thawte Consulting, OU=Certification Services Division, CN=Thawte Personal Premium CA, EMAIL=personal-premium@thawte.com");
	g_free (dn);
}

DEFINE_TEST (issuer_part)
{
	gchar *part = crui_x509_cert_get_issuer_part (certificate, "st");
	g_assert (part);
	g_assert_cmpstr (part, ==, "Western Cape");
	g_free (part);
}

DEFINE_TEST (subject_cn)
{
	gchar *cn = crui_x509_cert_get_subject_cn (certificate);
	g_assert (cn);
	g_assert_cmpstr (cn, ==, "Thawte Personal Premium CA");
	g_free (cn);
}

DEFINE_TEST (subject_dn)
{
	gchar *dn = crui_x509_cert_get_subject_dn (certificate);
	g_assert (dn);
	g_assert_cmpstr (dn, ==, "C=ZA, ST=Western Cape, L=Cape Town, O=Thawte Consulting, OU=Certification Services Division, CN=Thawte Personal Premium CA, EMAIL=personal-premium@thawte.com");
	g_free (dn);
}

DEFINE_TEST (subject_part)
{
	gchar *part = crui_x509_cert_get_subject_part (certificate, "St");
	g_assert (part);
	g_assert_cmpstr (part, ==, "Western Cape");
	g_free (part);
}

DEFINE_TEST (issued_date)
{
	GDate *date = crui_x509_cert_get_issued_date (certificate);
	g_assert (date);
	g_assert_cmpuint (g_date_get_year (date), ==, 1996);
	g_assert_cmpuint (g_date_get_month (date), ==, 1);
	g_assert_cmpuint (g_date_get_day (date), ==, 1);
	g_date_free (date);
}

DEFINE_TEST (expiry_date)
{
	GDate *date = crui_x509_cert_get_expiry_date (certificate);
	g_assert (date);
	g_assert_cmpuint (g_date_get_year (date), ==, 2020);
	g_assert_cmpuint (g_date_get_month (date), ==, 12);
	g_assert_cmpuint (g_date_get_day (date), ==, 31);
	g_date_free (date);
}

DEFINE_TEST (serial_number)
{
	gsize n_serial;
	guchar *serial = crui_x509_cert_get_serial_number (certificate, &n_serial);
	g_assert (serial);
	g_assert_cmpuint (n_serial, ==, 1);
	g_assert (memcmp (serial, "\0", n_serial) == 0);
	g_free (serial);
}

DEFINE_TEST (serial_number_hex)
{
	gchar *serial = crui_x509_cert_get_serial_number_hex (certificate);
	g_assert (serial);
	g_assert_cmpstr (serial, ==, "00");
	g_free (serial);
}

DEFINE_TEST (fingerprint)
{
	gsize n_print;
	guchar *print = crui_x509_cert_get_fingerprint (certificate, G_CHECKSUM_MD5, &n_print);
	g_assert (print);
	g_assert_cmpuint (n_print, ==, g_checksum_type_get_length (G_CHECKSUM_MD5));
	g_assert (memcmp (print, "\x3a\xb2\xde\x22\x9a\x20\x93\x49\xf9\xed\xc8\xd2\x8a\xe7\x68\x0d", n_print) == 0);
	g_free (print);
}

DEFINE_TEST (fingerprint_hex)
{
	gchar *print = crui_x509_cert_get_fingerprint_hex (certificate, G_CHECKSUM_MD5);
	g_assert (print);
	g_assert_cmpstr (print, ==, "3ab2de229a209349f9edc8d28ae7680d");
	g_free (print);
}

