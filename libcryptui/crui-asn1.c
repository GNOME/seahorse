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

#include "crui-asn1.h"

#include <libtasn1.h>

#include <string.h>

/* 
 * HACK: asn1Parser defines these arrays as extern const, which gives 
 * gcc a fit. So we def it out. 
 */
 
#define extern 
#include "pk.tab"
#include "pkix.tab"
#undef extern 

static gboolean asn1_initialized = FALSE;
static ASN1_TYPE asn1_pk = NULL; 
static ASN1_TYPE asn1_pkix = NULL;

typedef struct _PrintableOid {
	GQuark oid;
	const gchar *oidstr;
	const gchar *display;
	gboolean is_choice;
} PrintableOid;

static PrintableOid printable_oids[] = {
	{ 0, "0.9.2342.19200300.100.1.25", "DC", FALSE },
	{ 0, "0.9.2342.19200300.100.1.1", "UID", TRUE },

	{ 0, "1.2.840.113549.1.9.1", "EMAIL", FALSE },
	{ 0, "1.2.840.113549.1.9.7", NULL, TRUE },
	{ 0, "1.2.840.113549.1.9.20", NULL, FALSE },
	
	{ 0, "1.3.6.1.5.5.7.9.1", "dateOfBirth", FALSE },
	{ 0, "1.3.6.1.5.5.7.9.2", "placeOfBirth", FALSE },
	{ 0, "1.3.6.1.5.5.7.9.3", "gender", FALSE },
        { 0, "1.3.6.1.5.5.7.9.4", "countryOfCitizenship", FALSE },
        { 0, "1.3.6.1.5.5.7.9.5", "countryOfResidence", FALSE },

	{ 0, "2.5.4.3", "CN", TRUE },
	{ 0, "2.5.4.4", "surName", TRUE },
	{ 0, "2.5.4.5", "serialNumber", FALSE },
	{ 0, "2.5.4.6", "C", FALSE, },
	{ 0, "2.5.4.7", "L", TRUE },
	{ 0, "2.5.4.8", "ST", TRUE },
	{ 0, "2.5.4.9", "STREET", TRUE },
	{ 0, "2.5.4.10", "O", TRUE },
	{ 0, "2.5.4.11", "OU", TRUE },
	{ 0, "2.5.4.12", "T", TRUE },
	{ 0, "2.5.4.20", "telephoneNumber", FALSE },
	{ 0, "2.5.4.42", "givenName", TRUE },
	{ 0, "2.5.4.43", "initials", TRUE },
	{ 0, "2.5.4.44", "generationQualifier", TRUE },
	{ 0, "2.5.4.46", "dnQualifier", FALSE },
	{ 0, "2.5.4.65", "pseudonym", TRUE },

	{ 0, NULL, NULL, FALSE }
};

ASN1_TYPE 
_crui_asn1_get_definitions (const char *type)
{
	ASN1_TYPE *where = NULL;
	const ASN1_ARRAY_TYPE *tab = NULL;
	int res;

	if (!asn1_initialized) {
		asn1_check_version (LIBTASN1_VERSION);
		asn1_initialized = TRUE;
	}
	
	if (strncmp (type, "PKIX1.", 6) == 0) {
		tab = pkix_asn1_tab;
		where = &asn1_pkix;
	} else if (strncmp (type, "PK.", 3) == 0) {
		tab = pk_asn1_tab;
		where = &asn1_pk;
	} else {
		g_return_val_if_reached (NULL);
	}
	
	if (!*where) {
		res = asn1_array2tree (tab, where, NULL);
		g_return_val_if_fail (res == ASN1_SUCCESS, NULL);
	}
	
	return *where;
}

ASN1_TYPE
_crui_asn1_decode (const gchar *type, const guchar *data, gsize n_data)
{
	ASN1_TYPE definitions = _crui_asn1_get_definitions (type);
	ASN1_TYPE asn;
	int res;

	res = asn1_create_element (definitions, type, &asn); 
	g_return_val_if_fail (res == ASN1_SUCCESS, NULL);
	
	res = asn1_der_decoding (&asn, data, n_data, NULL);
	if (res != ASN1_SUCCESS) {
		asn1_delete_structure (&asn);
		return NULL;
	}
	
	return asn;
}

gboolean
_crui_asn1_have_value (ASN1_TYPE asn, const gchar *part)
{
	int l, res;
	
	g_return_val_if_fail (asn, FALSE);
	g_return_val_if_fail (part, FALSE);
	
	l = 0;
	res = asn1_read_value (asn, part, NULL, &l);
	g_return_val_if_fail (res != ASN1_SUCCESS, FALSE);
	if (res != ASN1_MEM_ERROR)
		return FALSE;
	
	return TRUE;
}

guchar*
_crui_asn1_read_value (ASN1_TYPE asn, const gchar *part, gsize *len)
{
	int l, res;
	guchar *buf;
	
	g_return_val_if_fail (asn, NULL);
	g_return_val_if_fail (part, NULL);
	g_return_val_if_fail (len, NULL);
	
	*len = 0;

	l = 0;
	res = asn1_read_value (asn, part, NULL, &l);
	g_return_val_if_fail (res != ASN1_SUCCESS, NULL);
	if (res != ASN1_MEM_ERROR)
		return NULL;
		
	/* Always null terminate it, just for convenience */
	buf = g_malloc0 (l + 1);
	
	res = asn1_read_value (asn, part, buf, &l);
	if (res != ASN1_SUCCESS) {
		g_free (buf);
		buf = NULL;
	} else {
		*len = l;
	}
	
	return buf;
}

gchar*
_crui_asn1_read_string (ASN1_TYPE asn, const gchar *part)
{
	int l, res;
	gchar *buf;
	
	g_return_val_if_fail (asn, NULL);
	g_return_val_if_fail (part, NULL);
	
	l = 0;
	res = asn1_read_value (asn, part, NULL, &l);
	g_return_val_if_fail (res != ASN1_SUCCESS, NULL);
	if (res != ASN1_MEM_ERROR)
		return NULL;
		
	/* Always null terminate it, just for convenience */
	buf = g_malloc0 (l + 1);
	
	res = asn1_read_value (asn, part, buf, &l);
	if (res != ASN1_SUCCESS) {
		g_free (buf);
		buf = NULL;
	}
	
	return buf;
}

GQuark
_crui_asn1_read_oid (ASN1_TYPE asn, const gchar *part)
{
	GQuark quark;
	guchar *buf;
	gsize n_buf;
	
	buf = _crui_asn1_read_value (asn, part, &n_buf);
	if (!buf)
		return 0;
		
	quark = g_quark_from_string ((gchar*)buf);
	g_free (buf);
	
	return quark;
}

gboolean
_crui_asn1_read_boolean (ASN1_TYPE asn, const gchar *part, gboolean *val)
{
	gchar buffer[32];
	int n_buffer = sizeof (buffer) - 1;
	int res;
	
	g_return_val_if_fail (asn, FALSE);
	g_return_val_if_fail (part, FALSE);
	g_return_val_if_fail (val, FALSE);
	
	memset (buffer, 0, sizeof (buffer));
	
	res = asn1_read_value (asn, part, buffer, &n_buffer);
	if (res != ASN1_SUCCESS)
		return FALSE;
		
	if (g_ascii_strcasecmp (buffer, "TRUE") == 0)
		*val = TRUE;
	else
		*val = FALSE;
		
	return TRUE;
}

gboolean
_crui_asn1_read_uint (ASN1_TYPE asn, const gchar *part, guint *val)
{
	guchar buf[4];
	int n_buf = sizeof (buf);
	gsize i;
	int res;
	
	g_return_val_if_fail (asn, FALSE);
	g_return_val_if_fail (part, FALSE);
	g_return_val_if_fail (val, FALSE);
	
	res = asn1_read_value (asn, part, buf, &n_buf);
	if(res != ASN1_SUCCESS)
		return FALSE;

	if (n_buf > 4 || n_buf < 1)
		return FALSE;

	*val = 0;
	for (i = 0; i < n_buf; ++i)
		*val |= buf[i] << (8 * ((n_buf - 1) - i));

	return TRUE;
}

/* -------------------------------------------------------------------------------
 * Reading Dates
 */

#define SECS_PER_DAY  86400
#define SECS_PER_HOUR 3600
#define SECS_PER_MIN  60

static int
atoin (const char *p, int digits)
{
	int ret = 0, base = 1;
	while(--digits >= 0) {
		if (p[digits] < '0' || p[digits] > '9')
			return -1;
		ret += (p[digits] - '0') * base;
		base *= 10;
	}
	return ret;
}

static int
two_to_four_digit_year (int year)
{
	time_t now;
	struct tm tm;
	int century, current;
	
	g_return_val_if_fail (year > 0 && year <= 99, -1);
	
	/* Get the current year */
	now = time (NULL);
	g_return_val_if_fail (now >= 0, -1);
	if (!gmtime_r (&now, &tm))
		g_return_val_if_reached (-1);

	current = (tm.tm_year % 100);
	century = (tm.tm_year + 1900) - current;

	/* 
	 * Check if it's within 40 years before the 
	 * current date. 
	 */
	if (current < 40) {
		if (year < current)
			return century + year;
		if (year > 100 - (40 - current))
			return (century - 100) + year;
	} else {
		if (year < current && year > (current - 40))
			return century + year;
	}
	
	/* 
	 * If it's after then adjust for overflows to
	 * the next century.
	 */
	if (year < current)
		return century + 100 + year;
	else
		return century + year;
}

GDate*
_crui_asn1_parse_date (const gchar *time, gboolean full_year)
{
	GDate date;
	guint n_time;
	const char *p, *e;
	int seconds;

	g_assert (time);	
	n_time = strlen (time);
	
	/* 
	 * YYMMDDhhmmss.ffff Z | +0000 
	 * 
	 * or 
	 * 
	 * YYYYMMDDhhmmss.ffff Z | +0000
	 */
	
	/* Reset everything to default legal values */
	g_date_clear (&date, 1);
	seconds = 0;
	
	/* Select the digits part of it */
	p = time;
	for (e = p; *e >= '0' && *e <= '9'; ++e);
	
	/* A four digit year */
	if (full_year) {
		if (p + 4 <= e) {
			g_date_set_year (&date, atoin (p, 4));
			p += 4;
		}
		
	/* A two digit year */
	} else {
		if (p + 2 <= e) {
			int year = atoin (p, 2);
			p += 2;
		
			/* 
			 * 40 years in the past is our century. 60 years
			 * in the future is the next century. 
			 */
			g_date_set_year (&date, two_to_four_digit_year (year));
		}
	}
	
	/* The month */
	if (p + 2 <= e) {
		g_date_set_month (&date, atoin (p, 2));
		p += 2;
	}
	
	/* The day of month */
	if (p + 2 <= e) {
		g_date_set_day (&date, atoin (p, 2));
		p += 2;
	}
	
	/* The hour */
	if (p + 2 <= e) {
		seconds += (atoin (p, 2) * SECS_PER_HOUR);
		p += 2;
	}
	
	/* The minute */
	if (p + 2 <= e) {
		seconds += (atoin (p, 2) * SECS_PER_MIN);
		p += 2;
	}
	
	/* The seconds */
	if (p + 2 <= e) {
		seconds += atoin (p, 2);
		p += 2;
	}

	if (!g_date_valid (&date) || seconds > SECS_PER_DAY)
		return NULL;
	    	
	/* Make sure all that got parsed */
	if (p != e)
		return NULL;

	/* Now the remaining optional stuff */
	e = time + n_time;
		
	/* See if there's a fraction, and discard it if so */
	if (p < e && *p == '.' && p + 5 <= e)
		p += 5;
		
	/* See if it's UTC */
	if (p < e && *p == 'Z') {
		p += 1;

	/* See if it has a timezone */	
	} else if ((*p == '-' || *p == '+') && p + 3 <= e) { 
		int off, neg;
		
		neg = *p == '-';
		++p;
		
		off = atoin (p, 2) * SECS_PER_HOUR;
		if (off < 0 || off > SECS_PER_DAY)
			return NULL;
		p += 2;
		
		if (p + 2 <= e) {
			off += atoin (p, 2) * SECS_PER_MIN;
			p += 2;
		}

		/* See if TZ offset sends us to a different day */
		if (neg && off > seconds)
			g_date_subtract_days (&date, 1);
		else if (!neg && off > (SECS_PER_DAY - seconds))
			g_date_add_days (&date, 1);
	}

	/* Make sure everything got parsed */	
	if (p != e)
		return NULL;

	return g_date_new_dmy (g_date_get_day (&date),
	                       g_date_get_month (&date),
	                       g_date_get_year (&date));
}

GDate*
_crui_asn1_read_date (ASN1_TYPE asn, const gchar *part)
{
	#define MAX_TIME 1024
	gchar ttime[MAX_TIME];
	gchar *name;
	int len, res;
	gboolean full_year = TRUE;
	
	g_return_val_if_fail (asn, NULL);
	g_return_val_if_fail (part, NULL);

	len = sizeof (ttime) - 1;
	res = asn1_read_value (asn, part, ttime, &len);
	if (res != ASN1_SUCCESS)
		return NULL;
		
	/* CHOICE */
	if (strcmp (ttime, "generalTime") == 0) {
		name = g_strconcat (part, ".generalTime", NULL);
		full_year = TRUE;
		
	/* UTCTIME */
	} else {
		name = g_strconcat (part, ".utcTime", NULL);
		full_year = FALSE;
	}
	
	len = sizeof (ttime) - 1;
	res = asn1_read_value (asn, name, ttime, &len);
	g_free (name);
	if (res != ASN1_SUCCESS)
		return FALSE;

	return _crui_asn1_parse_date (ttime, full_year);
}

/* -------------------------------------------------------------------------------
 * Reading DN's
 */

static PrintableOid*
dn_find_printable (GQuark oid)
{
	PrintableOid *printable;
	int i;
	
	g_return_val_if_fail (oid != 0, NULL);
	
	for (i = 0; printable_oids[i].oidstr != NULL; ++i) {
		printable = &printable_oids[i];
		if (!printable->oid)
			printable->oid = g_quark_from_static_string (printable->oidstr);
		if (printable->oid == oid)
			return printable;
	}
	
	return NULL;
}

static const char HEXC[] = "0123456789ABCDEF";

static gchar*
dn_print_hex_value (const guchar *data, gsize len)
{
	GString *result = g_string_sized_new (len * 2 + 1);
	gsize i;
	
	g_string_append_c (result, '#');
	for (i = 0; i < len; ++i) {
		g_string_append_c (result, HEXC[data[i] >> 4 & 0xf]);
		g_string_append_c (result, HEXC[data[i] & 0xf]);
	}
	
	return g_string_free (result, FALSE);
}

static gchar* 
dn_print_oid_value_parsed (PrintableOid *printable, guchar *data, gsize len)
{
	const gchar *asn_name;
	ASN1_TYPE asn1;
	gchar *part;
	gchar *value;
	
	g_assert (printable);
	g_assert (data);
	g_assert (len);
	g_assert (printable->oid);
	
	asn_name = asn1_find_structure_from_oid (_crui_asn1_get_definitions ("PKIX1."), 
	                                         printable->oidstr);
	g_return_val_if_fail (asn_name, NULL);
	
	part = g_strdup_printf ("PKIX1.%s", asn_name);
	asn1 = _crui_asn1_decode (part, data, len);
	g_free (part);
	
	if (!asn1) {
		g_message ("couldn't decode value for OID: %s", printable->oidstr);
		return NULL;
	}

	value = _crui_asn1_read_string (asn1, "");
	
	/*
	 * If it's a choice element, then we have to read depending
	 * on what's there.
	 */
	if (value && printable->is_choice) {
		if (strcmp ("printableString", value) == 0 ||
		    strcmp ("ia5String", value) == 0 ||
		    strcmp ("utf8String", value) == 0 ||
		    strcmp ("teletexString", value) == 0) {
			part = value;
			value = _crui_asn1_read_string (asn1, part);
			g_free (part);
		} else {
			g_free (value);
			return NULL;
		}
	}

	if (!value) {
		g_message ("couldn't read value for OID: %s", printable->oidstr);
		return NULL;
	}

	/* 
	 * Now we make sure it's UTF-8. 
	 */
	if (!g_utf8_validate (value, -1, NULL)) {
		gchar *hex = dn_print_hex_value ((guchar*)value, strlen (value));
		g_free (value);
		value = hex;
	}
	
	return value;
}

static gchar*
dn_print_oid_value (PrintableOid *printable, guchar *data, gsize len)
{
	gchar *value;
	
	if (printable) {
		value = dn_print_oid_value_parsed (printable, data, len);
		if (value != NULL)
			return value;
	}
	
	return dn_print_hex_value (data, len);
}

static gchar* 
_crui_asn1_read_rdn (ASN1_TYPE asn, const gchar *part)
{
	PrintableOid *printable;
	GQuark oid;
	gchar *path;
	guchar *value;
	gsize n_value;
	gchar *display;
	gchar *result;
	
	path = g_strdup_printf ("%s.type", part);
	oid = _crui_asn1_read_oid (asn, path);
	g_free (path);

	if (!oid)
		return NULL;
	
	path = g_strdup_printf ("%s.value", part);
	value = _crui_asn1_read_value (asn, path, &n_value);
	g_free (path);
	
	printable = dn_find_printable (oid);
	
	g_return_val_if_fail (value, NULL);
	display = dn_print_oid_value (printable, value, n_value);
	
	result = g_strconcat (printable ? printable->display : g_quark_to_string (oid), 
			      "=", display, NULL);
	g_free (display);
	
	return result;
}

gchar*
_crui_asn1_read_dn (ASN1_TYPE asn, const gchar *part)
{
	gboolean done = FALSE;
	GString *result;
	gchar *path;
	gchar *rdn;
	gint i, j;
	
	g_return_val_if_fail (asn, NULL);
	result = g_string_sized_new (64);
	
	/* Each (possibly multi valued) RDN */
	for (i = 1; !done; ++i) {
		
		/* Each type=value pair of an RDN */
		for (j = 1; TRUE; ++j) {
			path = g_strdup_printf ("%s%s?%u.?%u", part ? part : "", 
			                        part ? "." : "", i, j);
			rdn = _crui_asn1_read_rdn (asn, path);
			g_free (path);

			if (!rdn) {
				done = j == 1;
				break;
			}
			
			/* Account for multi valued RDNs */
			if (j > 1)
				g_string_append (result, "+");
			else if (i > 1)
				g_string_append (result, ", ");
			
			g_string_append (result, rdn);
			g_free (rdn);
		}
	}

	/* Returns null when string is empty */
	return g_string_free (result, (result->len == 0));
}

gchar*
_crui_asn1_read_dn_part (ASN1_TYPE asn, const gchar *part, const gchar *match)
{
	PrintableOid *printable;
	gboolean done = FALSE;
	guchar *value;
	gsize n_value;
	gchar *path;
	GQuark oid;
	gint i, j;
	
	g_return_val_if_fail (asn, NULL);
	
	/* Each (possibly multi valued) RDN */
	for (i = 1; !done; ++i) {
		
		/* Each type=value pair of an RDN */
		for (j = 1; TRUE; ++j) {
			path = g_strdup_printf ("%s%s?%u.?%u.type", 
			                        part ? part : "", 
			                        part ? "." : "", i, j);
			oid = _crui_asn1_read_oid (asn, path);
			g_free (path);

			if (!oid) {
				done = j == 1;
				break;
			}
			
			/* Does it match either the OID or the displayable? */
			if (g_ascii_strcasecmp (g_quark_to_string (oid), match) != 0) {
				printable = dn_find_printable (oid);
				if (!printable || !printable->display || 
				    !g_ascii_strcasecmp (printable->display, match) == 0)
					continue;
			}

			path = g_strdup_printf ("%s%s?%u.?%u.value", 
			                        part ? part : "", 
			                        part ? "." : "", i, j);
			value = _crui_asn1_read_value (asn, path, &n_value);
			g_free (path);
			
			g_return_val_if_fail (value, NULL);
			return dn_print_oid_value (printable, value, n_value);
		}
	}
	
	return NULL;
}
