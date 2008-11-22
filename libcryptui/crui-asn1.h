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

#ifndef CRUIASN1_H_
#define CRUIASN1_H_

#include <glib.h>

#include <libtasn1.h>

ASN1_TYPE          _crui_asn1_get_definitions               (const gchar *type);

ASN1_TYPE          _crui_asn1_decode                        (const gchar *type, const guchar *data, gsize n_data);

gboolean           _crui_asn1_have_value                    (ASN1_TYPE asn, const gchar *part);

guchar*            _crui_asn1_read_value                    (ASN1_TYPE asn, const gchar *part, gsize *len);

gchar*             _crui_asn1_read_string                   (ASN1_TYPE asn, const gchar *part);

GQuark             _crui_asn1_read_oid                      (ASN1_TYPE asn, const gchar *part);

gchar*             _crui_asn1_read_dn                       (ASN1_TYPE asn, const gchar *part);

gchar*             _crui_asn1_read_dn_part                  (ASN1_TYPE asn, const gchar *part, const gchar *dnpart);

gboolean           _crui_asn1_read_boolean                  (ASN1_TYPE asn, const gchar *part, gboolean *val);

gboolean           _crui_asn1_read_uint                     (ASN1_TYPE asn, const gchar *part, guint *val);

GDate*             _crui_asn1_read_date                     (ASN1_TYPE asn, const gchar *part);

GDate*             _crui_asn1_parse_date                    (const gchar* value, gboolean full_year);

#endif /* CRUIASN1_H_ */
