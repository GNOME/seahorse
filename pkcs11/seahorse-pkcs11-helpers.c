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

#include "seahorse-pkcs11.h"
#include "seahorse-pkcs11-helpers.h"

#include <stdlib.h>
#include <string.h>
#include <pkcs11.h>

static char* 
seahorse_pkcs11_klass_to_string (gulong klass) 
{
	switch (klass)
	{
	case CKO_DATA:
		return g_strdup ("data");
	case CKO_CERTIFICATE:
		return g_strdup ("certificate");
	case CKO_PRIVATE_KEY:
		return g_strdup ("private-key");
	case CKO_PUBLIC_KEY:
		return g_strdup ("public-key");
	default:
		return g_strdup_printf ("%lu", klass);
	}
}

static gulong 
seahorse_pkcs11_string_to_klass (const char* str) 
{
	gulong klass;
	gchar *end;
	
	g_return_val_if_fail (str != NULL, 0UL);
	
	if (g_str_equal (str, "data"))
		return CKO_DATA;
	if (g_str_equal (str, "certificate"))
		return CKO_CERTIFICATE;
	if (g_str_equal (str, "private-key"))
		return CKO_PRIVATE_KEY;
	if (g_str_equal (str, "public-key"))
		return CKO_PUBLIC_KEY;
	
	klass = strtoul (str, &end, 10);
	if (!*end) {
		g_warning ("unrecognized and unparsable PKCS#11 class: %s", str);
		return (gulong)-1;
	}
	
	return klass;
}

GQuark 
seahorse_pkcs11_id_from_attributes (GckAttributes* attrs)
{
	gulong klass;
	GckAttribute* attr;
	gchar *id, *encoded, *klass_str;
	GQuark quark;
	
	g_return_val_if_fail (attrs != NULL, 0U);
	
	/* These cases should have been covered by the programmer */

	klass = 0;
	if (!gck_attributes_find_ulong (attrs, CKA_CLASS, &klass)) {
		g_warning ("Cannot create object id. PKCS#11 attribute set did not contain CKA_CLASS.");
		return 0;
	}
	
	attr = gck_attributes_find (attrs, CKA_ID);
	if (attr == NULL) {
		g_warning ("Cannot create object id. PKCS#11 attribute set did not contain CKA_ID");
		return 0;
	}
	
	encoded = g_base64_encode (attr->value, attr->length);
	klass_str = seahorse_pkcs11_klass_to_string (klass);
	id = g_strdup_printf ("%s:%s/%s", SEAHORSE_PKCS11_TYPE_STR, klass_str, encoded);
	g_free (encoded);
	g_free (klass_str);
	
	quark = g_quark_from_string (id);
	g_free (id);
	
	return quark;
}


gboolean 
seahorse_pkcs11_id_to_attributes (GQuark id, GckAttributes* attrs)
{
	const gchar* value;
	guchar *ckid;
	gsize n_ckid;
	gulong klass;
	gchar **parts;

	g_return_val_if_fail (attrs != NULL, FALSE);
	
	if (id == 0)
		return FALSE;

	value = g_quark_to_string (id);
	
	parts = g_strsplit_set (value, ":/", 3);
	if (!parts[0] || !g_str_equal (parts[0], SEAHORSE_PKCS11_TYPE_STR) ||
	    !parts[1] || !parts[2]) {
		g_strfreev (parts);
		return FALSE;
	}
	
	klass = seahorse_pkcs11_string_to_klass (parts[1]);
	if (klass == -1) {
		g_strfreev (parts);
		return FALSE;
	}

	ckid = g_base64_decode (parts[2], &n_ckid);
	g_strfreev (parts);

	if (!id) 
		return FALSE;
		
	gck_attributes_add_ulong (attrs, CKA_CLASS, klass);
	gck_attributes_add_data (attrs, CKA_ID, ckid, n_ckid);
	g_free (ckid);

	return TRUE;
}


