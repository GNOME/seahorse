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

#ifndef __SEAHORSE_PKCS11_CERTIFICATE_H__
#define __SEAHORSE_PKCS11_CERTIFICATE_H__

#include <gck/gck.h>

#include <glib-object.h>

#include "seahorse-pkcs11-object.h"

#define SEAHORSE_PKCS11_TYPE_CERTIFICATE               (seahorse_pkcs11_certificate_get_type ())
#define SEAHORSE_PKCS11_CERTIFICATE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_PKCS11_TYPE_CERTIFICATE, SeahorsePkcs11Certificate))
#define SEAHORSE_PKCS11_CERTIFICATE_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_PKCS11_TYPE_CERTIFICATE, SeahorsePkcs11CertificateClass))
#define SEAHORSE_PKCS11_IS_CERTIFICATE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_PKCS11_TYPE_CERTIFICATE))
#define SEAHORSE_PKCS11_IS_CERTIFICATE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_PKCS11_TYPE_CERTIFICATE))
#define SEAHORSE_PKCS11_CERTIFICATE_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_PKCS11_TYPE_CERTIFICATE, SeahorsePkcs11CertificateClass))

typedef struct _SeahorsePkcs11Certificate SeahorsePkcs11Certificate;
typedef struct _SeahorsePkcs11CertificateClass SeahorsePkcs11CertificateClass;
typedef struct _SeahorsePkcs11CertificatePrivate SeahorsePkcs11CertificatePrivate;
    
struct _SeahorsePkcs11Certificate {
	SeahorsePkcs11Object parent;
	SeahorsePkcs11CertificatePrivate *pv;
};

struct _SeahorsePkcs11CertificateClass {
	SeahorsePkcs11ObjectClass parent_class;
};

GType                       seahorse_pkcs11_certificate_get_type               (void);

SeahorsePkcs11Certificate*  seahorse_pkcs11_certificate_new                    (GckObject* object);

void                        seahorse_pkcs11_certificate_realize                (SeahorsePkcs11Certificate *self);

gchar*                      seahorse_pkcs11_certificate_get_fingerprint        (SeahorsePkcs11Certificate* self);

guint                       seahorse_pkcs11_certificate_get_validity           (SeahorsePkcs11Certificate* self);

guint                       seahorse_pkcs11_certificate_get_trust              (SeahorsePkcs11Certificate* self);

gulong                      seahorse_pkcs11_certificate_get_expires            (SeahorsePkcs11Certificate* self);

#endif /* __SEAHORSE_PKCS11_CERTIFICATE_H__ */
