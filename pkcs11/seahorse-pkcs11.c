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
#include <stdlib.h>
#include <string.h>
#include <pkcs11.h>
#include <pkcs11g.h>




static char* seahorse_pkcs11_klass_to_string (gulong klass);
static gulong seahorse_pkcs11_string_to_klass (const char* str);
static void _vala_array_free (gpointer array, gint array_length, GDestroyNotify destroy_func);
static int _vala_strcmp0 (const char * str1, const char * str2);



GQuark seahorse_pkcs11_id_from_attributes (GP11Attributes* attrs) {
	gulong klass;
	GP11Attribute* attr;
	char* _tmp3;
	char* _tmp2;
	char* _tmp4;
	char* value;
	GQuark _tmp5;
	g_return_val_if_fail (attrs != NULL, 0U);
	/* These cases should have been covered by the programmer */
	klass = 0UL;
	if (!gp11_attributes_find_ulong (attrs, CKA_CLASS, &klass)) {
		g_warning ("seahorse-pkcs11.vala:36: Cannot create object id. PKCS#11 attribute set did not contain CKA_CLASS.");
		return ((GQuark) (0));
	}
	attr = gp11_attributes_find (attrs, CKA_ID);
	if (attr == NULL) {
		g_warning ("seahorse-pkcs11.vala:42: Cannot create object id. PKCS#11 attribute set did not contain CKA_ID");
		return ((GQuark) (0));
	}
	_tmp3 = NULL;
	_tmp2 = NULL;
	_tmp4 = NULL;
	value = (_tmp4 = g_strdup_printf ("%s:%s/%s", SEAHORSE_PKCS11_TYPE_STR, (_tmp2 = seahorse_pkcs11_klass_to_string (klass)), (_tmp3 = g_base64_encode (attr->value, ((gsize) (attr->length))))), (_tmp3 = (g_free (_tmp3), NULL)), (_tmp2 = (g_free (_tmp2), NULL)), _tmp4);
	return (_tmp5 = g_quark_from_string (value), (value = (g_free (value), NULL)), _tmp5);
}


gboolean seahorse_pkcs11_id_to_attributes (GQuark id, GP11Attributes* attrs) {
	const char* value;
	char** _tmp1;
	gint parts_length1;
	char** parts;
	gulong klass;
	gsize len;
	guchar* _tmp4;
	gint ckid_length1;
	guchar* ckid;
	gboolean _tmp6;
	g_return_val_if_fail (attrs != NULL, FALSE);
	if (id == 0) {
		return FALSE;
	}
	value = g_quark_to_string (id);
	_tmp1 = NULL;
	parts = (_tmp1 = g_strsplit_set (value, ":/", 3), parts_length1 = -1, _tmp1);
	if (parts_length1 != 3 || _vala_strcmp0 (parts[0], SEAHORSE_PKCS11_TYPE_STR) != 0) {
		gboolean _tmp2;
		return (_tmp2 = FALSE, (parts = (_vala_array_free (parts, parts_length1, ((GDestroyNotify) (g_free))), NULL)), _tmp2);
	}
	klass = seahorse_pkcs11_string_to_klass (parts[1]);
	if (klass == -1) {
		gboolean _tmp3;
		return (_tmp3 = FALSE, (parts = (_vala_array_free (parts, parts_length1, ((GDestroyNotify) (g_free))), NULL)), _tmp3);
	}
	len = 0UL;
	_tmp4 = NULL;
	ckid = (_tmp4 = g_base64_decode (parts[2], &len), ckid_length1 = -1, _tmp4);
	if (ckid == NULL) {
		gboolean _tmp5;
		return (_tmp5 = FALSE, (parts = (_vala_array_free (parts, parts_length1, ((GDestroyNotify) (g_free))), NULL)), (ckid = (g_free (ckid), NULL)), _tmp5);
	}
	gp11_attributes_add_data (attrs, CKA_ID, ckid, ckid_length1);
	gp11_attributes_add_ulong (attrs, CKA_CLASS, klass);
	return (_tmp6 = TRUE, (parts = (_vala_array_free (parts, parts_length1, ((GDestroyNotify) (g_free))), NULL)), (ckid = (g_free (ckid), NULL)), _tmp6);
}


static char* seahorse_pkcs11_klass_to_string (gulong klass) {
	gulong _tmp5;
	_tmp5 = klass;
	if (_tmp5 == CKO_DATA)
	do {
		return g_strdup ("data");
	} while (0); else if (_tmp5 == CKO_CERTIFICATE)
	do {
		return g_strdup ("certificate");
	} while (0); else if (_tmp5 == CKO_PRIVATE_KEY)
	do {
		return g_strdup ("private-key");
	} while (0); else if (_tmp5 == CKO_PUBLIC_KEY)
	do {
		return g_strdup ("public-key");
	} while (0); else
	do {
		return g_strdup_printf ("%lu", klass);
	} while (0);
}


static gulong seahorse_pkcs11_string_to_klass (const char* str) {
	g_return_val_if_fail (str != NULL, 0UL);
	if (_vala_strcmp0 (str, "data") == 0) {
		return CKO_DATA;
	} else {
		if (_vala_strcmp0 (str, "certificate") == 0) {
			return CKO_CERTIFICATE;
		} else {
			if (_vala_strcmp0 (str, "private-key") == 0) {
				return CKO_PRIVATE_KEY;
			} else {
				if (_vala_strcmp0 (str, "public-key") == 0) {
					return CKO_PUBLIC_KEY;
				} else {
					char* end;
					char* _tmp6;
					gulong _tmp5;
					char* _tmp4;
					gulong ret;
					gulong _tmp8;
					end = NULL;
					_tmp6 = NULL;
					_tmp4 = NULL;
					ret = (_tmp5 = strtoul (str, &_tmp4, 0), end = (_tmp6 = _tmp4, (end = (g_free (end), NULL)), _tmp6), _tmp5);
					if (g_utf8_strlen (end, -1) > 0) {
						gulong _tmp7;
						g_warning ("seahorse-pkcs11.vala:101: unrecognized and unparsable PKCS#11 class: %s", str);
						return (_tmp7 = ((gulong) (-1)), (end = (g_free (end), NULL)), _tmp7);
					}
					return (_tmp8 = ret, (end = (g_free (end), NULL)), _tmp8);
				}
			}
		}
	}
}


static void _vala_array_free (gpointer array, gint array_length, GDestroyNotify destroy_func) {
	if (array != NULL && destroy_func != NULL) {
		int i;
		if (array_length >= 0)
		for (i = 0; i < array_length; i = i + 1) {
			if (((gpointer*) (array))[i] != NULL)
			destroy_func (((gpointer*) (array))[i]);
		}
		else
		for (i = 0; ((gpointer*) (array))[i] != NULL; i = i + 1) {
			destroy_func (((gpointer*) (array))[i]);
		}
	}
	g_free (array);
}


static int _vala_strcmp0 (const char * str1, const char * str2) {
	if (str1 == NULL) {
		return -(str1 != str2);
	}
	if (str2 == NULL) {
		return (str1 != str2);
	}
	return strcmp (str1, str2);
}




