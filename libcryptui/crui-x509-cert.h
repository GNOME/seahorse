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

#ifndef __CRUI_X509_CERT_H__
#define __CRUI_X509_CERT_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define CRUI_TYPE_X509_CERT                 (crui_x509_cert_get_type())
#define CRUI_X509_CERT(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), CRUI_TYPE_X509_CERT, CruiX509Cert))
#define CRUI_IS_X509_CERT(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CRUI_TYPE_X509_CERT))
#define CRUI_X509_CERT_GET_INTERFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), CRUI_TYPE_X509_CERT, CruiX509CertIface))

typedef struct _CruiX509Cert      CruiX509Cert;
typedef struct _CruiX509CertIface CruiX509CertIface;

struct _CruiX509CertIface {
	GTypeInterface parent;
	
	const guchar* (*get_der_data)   (CruiX509Cert *self, gsize *n_length);
	
	gpointer dummy1;
	gpointer dummy2;
	gpointer dummy3;
	gpointer dummy5;
	gpointer dummy6;
	gpointer dummy7;
	gpointer dummy8;
};

GType                  crui_x509_cert_get_type                          (void) G_GNUC_CONST;

const guchar*          crui_x509_cert_get_der_data                      (CruiX509Cert *self, gsize *n_length);

gchar*                 crui_x509_cert_get_issuer_cn                     (CruiX509Cert *self);

gchar*                 crui_x509_cert_get_issuer_dn                     (CruiX509Cert *self);

gchar*                 crui_x509_cert_get_issuer_part                   (CruiX509Cert *self, const gchar *part);

gchar*                 crui_x509_cert_get_subject_cn                    (CruiX509Cert *self);

gchar*                 crui_x509_cert_get_subject_dn                    (CruiX509Cert *self);

gchar*                 crui_x509_cert_get_subject_part                  (CruiX509Cert *self, const gchar *part);

GDate*                 crui_x509_cert_get_issued_date                   (CruiX509Cert *self);

GDate*                 crui_x509_cert_get_expiry_date                   (CruiX509Cert *self);

guchar*                crui_x509_cert_get_serial_number                 (CruiX509Cert *self, gsize *n_length);

gchar*                 crui_x509_cert_get_serial_number_hex             (CruiX509Cert *self);

guchar*                crui_x509_cert_get_fingerprint                   (CruiX509Cert *self, GChecksumType type, gsize *n_length);

gchar*                 crui_x509_cert_get_fingerprint_hex               (CruiX509Cert *self, GChecksumType type);

G_END_DECLS

#endif /* __CRUI_X509_CERT_H__ */

