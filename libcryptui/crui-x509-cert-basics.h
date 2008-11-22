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

#ifndef __CRUI_X509_CERT_BASICS_H__
#define __CRUI_X509_CERT_BASICS_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#include "crui-x509-cert.h"

#define CRUI_TYPE_X509_CERT_BASICS               (crui_x509_cert_basics_get_type ())
#define CRUI_X509_CERT_BASICS(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CRUI_TYPE_X509_CERT_BASICS, CruiX509CertBasics))
#define CRUI_X509_CERT_BASICS_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CRUI_TYPE_X509_CERT_BASICS, CruiX509CertBasicsClass))
#define CRUI_IS_X509_CERT_BASICS(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CRUI_TYPE_X509_CERT_BASICS))
#define CRUI_IS_X509_CERT_BASICS_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CRUI_TYPE_X509_CERT_BASICS))
#define CRUI_X509_CERT_BASICS_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CRUI_TYPE_X509_CERT_BASICS, CruiX509CertBasicsClass))

typedef struct _CruiX509CertBasics CruiX509CertBasics;
typedef struct _CruiX509CertBasicsClass CruiX509CertBasicsClass;
typedef struct _CruiX509CertBasicsPrivate CruiX509CertBasicsPrivate;
    
struct _CruiX509CertBasics {
	GtkAlignment parent;
};

struct _CruiX509CertBasicsClass {
	GtkAlignmentClass parent_class;
};

GType                     crui_x509_cert_basics_get_type               (void);

CruiX509CertBasics*       crui_x509_cert_basics_new                    (CruiX509Cert *cert);

CruiX509Cert*             crui_x509_cert_basics_get_certificate        (CruiX509CertBasics *basics);

void                      crui_x509_cert_basics_set_certificate        (CruiX509CertBasics *basics, CruiX509Cert *cert);

#endif /* __CRUI_X509_CERT_BASICS_H__ */
