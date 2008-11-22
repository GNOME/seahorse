
#include "config.h"
#include "run-tests.h"

#include "crui-asn1.h"

#include <glib.h>

#include <string.h>

static ASN1_TYPE certificate = NULL;
static guchar *data = NULL;
static gsize n_data = 0;

DEFINE_SETUP (read_certificate)
{
	GError *err = NULL;
	gchar *contents;
	
	if (!g_file_get_contents ("files/test-certificate-1.der", &contents, &n_data, &err)) {
		g_warning ("couldn't read files/test-certificate-1.der: %s", err->message);
		return;
	}

	data = (guchar*)contents;
	certificate = _crui_asn1_decode ("PKIX1.Certificate", data, n_data);	
}

DEFINE_TEARDOWN (read_certificate)
{
	if(certificate)
		asn1_delete_structure (&certificate);
	g_free (data);
	data = NULL;
	n_data = 0;
}

DEFINE_TEST (definitions)
{
	ASN1_TYPE type = _crui_asn1_get_definitions ("PKIX1.Certificate");
	g_assert (type != NULL);
	
	type = _crui_asn1_get_definitions ("PK.*");
	g_assert (type != NULL);
}

DEFINE_TEST (decode)
{
	ASN1_TYPE type = _crui_asn1_decode ("PKIX1.Certificate", data, n_data);
	g_assert (type);
	asn1_delete_structure (&type);
}

DEFINE_TEST (have)
{
	gboolean have;
	
	have = _crui_asn1_have_value (certificate, "tbsCertificate.subject");
	g_assert (have);
	
	have = _crui_asn1_have_value (certificate, "tbsCertificate.issuerUniqueID");
	g_assert (!have);

	have = _crui_asn1_have_value (certificate, "blahblah");
	g_assert (!have);
}

DEFINE_TEST (value)
{
	#define NOT_BEFORE "960101000000Z"
	guchar *value;
	gsize n_value;
	
	value = _crui_asn1_read_value (certificate, "tbsCertificate.validity.notBefore.utcTime", &n_value);
	g_assert (value);

	/* Since it's a string value, the null termination is counted in the length */
	g_assert_cmpint (strlen (NOT_BEFORE) + 1, ==, n_value);
	if (memcmp (value, NOT_BEFORE, n_value) != 0)
		g_assert_not_reached ();
	
	g_free (value);
	
	value = _crui_asn1_read_value (certificate, "nonExistant", &n_value);
	g_assert (!value);
}

DEFINE_TEST (string)
{
	#define NOT_AFTER "201231235959Z"
	gchar *value;
	value = _crui_asn1_read_string (certificate, "tbsCertificate.validity.notAfter.utcTime");
	
	g_assert (value != NULL);
	g_assert_cmpstr (NOT_AFTER, ==, value);
	
	g_free (value);
	
	value = _crui_asn1_read_string (certificate, "nonExistant");
	g_assert (!value);
}

DEFINE_TEST (read_oid)
{
	GQuark oid;
	
	oid = _crui_asn1_read_oid (certificate, "tbsCertificate.signature.algorithm");
	
	g_assert (oid != 0);
	g_assert_cmpstr (g_quark_to_string (oid), ==, "1.2.840.113549.1.1.4");

	oid = _crui_asn1_read_oid (certificate, "nonExistant");
	g_assert (oid == 0);
}

DEFINE_TEST (read_dn)
{
	gchar *dn;
	
	dn = _crui_asn1_read_dn (certificate, "tbsCertificate.issuer.rdnSequence");
	g_assert (dn != NULL);
	g_assert_cmpstr (dn, ==, "C=ZA, ST=Western Cape, L=Cape Town, O=Thawte Consulting, OU=Certification Services Division, CN=Thawte Personal Premium CA, EMAIL=personal-premium@thawte.com");
	
	g_free (dn);
	
	dn = _crui_asn1_read_dn (certificate, "tbsCertificate.nonExistant");
	g_assert (dn == NULL);
}

DEFINE_TEST (read_dn_part)
{
	gchar *value;
	
	value = _crui_asn1_read_dn_part (certificate, "tbsCertificate.issuer.rdnSequence", "CN");
	g_assert (value != NULL);
	g_assert_cmpstr (value, ==, "Thawte Personal Premium CA");
	g_free (value);

	value = _crui_asn1_read_dn_part (certificate, "tbsCertificate.issuer.rdnSequence", "2.5.4.8");
	g_assert (value != NULL);
	g_assert_cmpstr (value, ==, "Western Cape");
	g_free (value);
	
	value = _crui_asn1_read_dn_part (certificate, "tbsCertificate.nonExistant", "CN");
	g_assert (value == NULL);

	value = _crui_asn1_read_dn_part (certificate, "tbsCertificate.issuer.rdnSequence", "DC");
	g_assert (value == NULL);

	value = _crui_asn1_read_dn_part (certificate, "tbsCertificate.issuer.rdnSequence", "0.0.0.0");
	g_assert (value == NULL);

	value = _crui_asn1_read_dn_part (certificate, "tbsCertificate.issuer.rdnSequence", "2.5.4.9");
	g_assert (value == NULL);
}

DEFINE_TEST (read_boolean)
{
	gboolean value;
	
	if (!_crui_asn1_read_boolean (certificate, "tbsCertificate.extensions.?1.critical", &value))
		g_assert_not_reached ();
	g_assert (value == TRUE);
	
	if (_crui_asn1_read_boolean (certificate, "tbsCertificate.nonExistant", &value))
		g_assert_not_reached ();
}

DEFINE_TEST (read_uint)
{
	guint value;
	
	if(!_crui_asn1_read_uint (certificate, "tbsCertificate.version", &value))
		g_assert_not_reached ();
	g_assert (value == 0x02);
	
	if (_crui_asn1_read_uint (certificate, "tbsCertificate.nonExistant", &value))
		g_assert_not_reached ();
}

typedef struct _TestDates {
	const gchar *value;
	gboolean full_year;
	guint year;
	guint month;
	guint day;
} TestDates;

const static TestDates test_dates[] = {
	{ "20070725130528Z", TRUE, 2007, 07, 25 },
	{ "20070725130528.2134Z", TRUE, 2007, 07, 25 },
	{ "19990725140528-0100", TRUE, 1999, 07, 25 },
	{ "20070725210528+0900", TRUE, 2007, 07, 26 },
	{ "20070725123528+1130", TRUE, 2007, 07, 26 },
	{ "20070725103528-1130", TRUE, 2007, 07, 24 },
	{ "20070725102959-1030", TRUE, 2007, 07, 24 },
	{ "20070725Z", TRUE, 2007, 07, 25 },
	{ "20070725+0000", TRUE, 2007, 07, 25 },
	
	{ "070725130528Z", FALSE, 2007, 07, 25 },
	{ "020125130528Z", FALSE, 2002, 01, 25 },
	{ "970725130528Z", FALSE, 1997, 07, 25 },
	{ "370725130528Z", FALSE, 2037, 07, 25},

	{ "070725130528.2134Z", FALSE, 2007, 07, 25 },
	{ "070725140528-0100", FALSE, 2007, 07, 25 },
	{ "070725040528+0900", FALSE, 2007, 07, 25 },
	{ "070725013528+1130", FALSE, 2007, 07, 25 },
	{ "070725Z", FALSE, 2007, 07, 25 },
	{ "070725+0000", FALSE, 2007, 07, 25 }
};

DEFINE_TEST (parse_date)
{
	GDate *date;
	int i;
	
	for (i = 0; i < G_N_ELEMENTS (test_dates); ++i)
	{
		date = _crui_asn1_parse_date (test_dates[i].value, test_dates[i].full_year);
		g_assert (date != NULL);
		if (g_date_get_year (date) != test_dates[i].year ||
		    g_date_get_month (date) != test_dates[i].month ||
		    g_date_get_day (date) != test_dates[i].day) {
			g_test_message ("%s != %04u%02u%02u", test_dates[i].value, 
			                test_dates[i].year, test_dates[i].month, test_dates[i].day);
			g_assert_not_reached ();
		}
	}
}

DEFINE_TEST (read_date)
{
	GDate *date;
	
	date = _crui_asn1_read_date (certificate, "tbsCertificate.validity.notBefore");
	g_assert (date != NULL);
	g_assert_cmpuint (g_date_get_year (date), ==, 1996);
	g_assert_cmpuint (g_date_get_month (date), ==, 1);
	g_assert_cmpuint (g_date_get_day (date), ==, 1);
}
