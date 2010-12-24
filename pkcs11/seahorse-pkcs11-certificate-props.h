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

#ifndef __SEAHORSE_PKCS11_CERTIFICATE_PROPS_H__
#define __SEAHORSE_PKCS11_CERTIFICATE_PROPS_H__

#include <gcr/gcr.h>

#include <gtk/gtk.h>

#include <glib-object.h>
#include <glib/gi18n.h>

#define SEAHORSE_TYPE_PKCS11_CERTIFICATE_PROPS               (seahorse_pkcs11_certificate_props_get_type ())
#define SEAHORSE_PKCS11_CERTIFICATE_PROPS(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PKCS11_CERTIFICATE_PROPS, SeahorsePkcs11CertificateProps))
#define SEAHORSE_PKCS11_CERTIFICATE_PROPS_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PKCS11_CERTIFICATE_PROPS, SeahorsePkcs11CertificatePropsClass))
#define SEAHORSE_IS_PKCS11_CERTIFICATE_PROPS(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PKCS11_CERTIFICATE_PROPS))
#define SEAHORSE_IS_PKCS11_CERTIFICATE_PROPS_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PKCS11_CERTIFICATE_PROPS))
#define SEAHORSE_PKCS11_CERTIFICATE_PROPS_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PKCS11_CERTIFICATE_PROPS, SeahorsePkcs11CertificatePropsClass))

typedef struct _SeahorsePkcs11CertificateProps SeahorsePkcs11CertificateProps;
typedef struct _SeahorsePkcs11CertificatePropsClass SeahorsePkcs11CertificatePropsClass;
typedef struct _SeahorsePkcs11CertificatePropsPrivate SeahorsePkcs11CertificatePropsPrivate;
    
struct _SeahorsePkcs11CertificateProps {
	GtkDialog parent;
};

struct _SeahorsePkcs11CertificatePropsClass {
	GtkDialogClass parent_class;
};

GType                      seahorse_pkcs11_certificate_props_get_type               (void);

GtkDialog*                 seahorse_pkcs11_certificate_props_new                    (GcrCertificate *cert);

void                       seahorse_pkcs11_certificate_props_set_certificate        (SeahorsePkcs11CertificateProps *self, 
                                                                                     GcrCertificate *cert);

GcrCertificate*            seahorse_pkcs11_certificate_props_get_certificate        (SeahorsePkcs11CertificateProps *self);

void                       seahorse_pkcs11_certificate_props_add_view               (SeahorsePkcs11CertificateProps *self, 
                                                                                     const gchar *title, 
                                                                                     GtkWidget *view);

void                       seahorse_pkcs11_certificate_props_insert_view            (SeahorsePkcs11CertificateProps *self, 
                                                                                     const gchar *title, 
                                                                                     GtkWidget *view, 
                                                                                     gint position);

void                       seahorse_pkcs11_certificate_props_focus_view             (SeahorsePkcs11CertificateProps *self, 
                                                                                     GtkWidget *view);

void                       seahorse_pkcs11_certificate_props_remove_view            (SeahorsePkcs11CertificateProps *self, 
                                                                                     GtkWidget *view);

#endif /* __SEAHORSE_PKCS11_CERTIFICATE_PROPS_H__ */
