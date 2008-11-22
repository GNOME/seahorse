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

#ifndef __CRUI_X509_CERT_SIMPLE_H__
#define __CRUI_X509_CERT_SIMPLE_H__

#include <glib-object.h>

#define CRUI_TYPE_X509_CERT_SIMPLE               (crui_x509_cert_simple_get_type ())
#define CRUI_X509_CERT_SIMPLE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CRUI_TYPE_X509_CERT_SIMPLE, CruiX509CertSimple))
#define CRUI_X509_CERT_SIMPLE_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CRUI_TYPE_X509_CERT_SIMPLE, CruiX509CertSimpleClass))
#define CRUI_IS_X509_CERT_SIMPLE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CRUI_TYPE_X509_CERT_SIMPLE))
#define CRUI_IS_X509_CERT_SIMPLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CRUI_TYPE_X509_CERT_SIMPLE))
#define CRUI_X509_CERT_SIMPLE_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CRUI_TYPE_X509_CERT_SIMPLE, CruiX509CertSimpleClass))

typedef struct _CruiX509CertSimple CruiX509CertSimple;
typedef struct _CruiX509CertSimpleClass CruiX509CertSimpleClass;
typedef struct _CruiX509CertSimplePrivate CruiX509CertSimplePrivate;
    
struct _CruiX509CertSimple {
	GObject parent;
};

struct _CruiX509CertSimpleClass {
	GObjectClass parent_class;
};

GType               crui_x509_cert_simple_get_type               (void);

CruiX509CertSimple* crui_x509_cert_simple_new                    (const guchar *der, gsize n_der);

CruiX509CertSimple* crui_x509_cert_simple_new_from_file          (const gchar *filename, GError **err);

#endif /* __CRUI_X509_CERT_SIMPLE_H__ */
