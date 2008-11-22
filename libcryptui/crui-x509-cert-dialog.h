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

#ifndef __CRUI_X509_CERT_DIALOG_H__
#define __CRUI_X509_CERT_DIALOG_H__

#include "crui-x509-cert.h"

#include <gtk/gtk.h>

#include <glib-object.h>
#include <glib/gi18n.h>

#define CRUI_TYPE_X509_CERT_DIALOG               (crui_x509_cert_dialog_get_type ())
#define CRUI_X509_CERT_DIALOG(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CRUI_TYPE_X509_CERT_DIALOG, CruiX509CertDialog))
#define CRUI_X509_CERT_DIALOG_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CRUI_TYPE_X509_CERT_DIALOG, CruiX509CertDialogClass))
#define CRUI_IS_X509_CERT_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CRUI_TYPE_X509_CERT_DIALOG))
#define CRUI_IS_X509_CERT_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CRUI_TYPE_X509_CERT_DIALOG))
#define CRUI_X509_CERT_DIALOG_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CRUI_TYPE_X509_CERT_DIALOG, CruiX509CertDialogClass))

typedef struct _CruiX509CertDialog CruiX509CertDialog;
typedef struct _CruiX509CertDialogClass CruiX509CertDialogClass;
typedef struct _CruiX509CertDialogPrivate CruiX509CertDialogPrivate;
    
struct _CruiX509CertDialog {
	GtkDialog parent;
};

struct _CruiX509CertDialogClass {
	GtkDialogClass parent_class;
};

GType                      crui_x509_cert_dialog_get_type               (void);

CruiX509CertDialog*        crui_x509_cert_dialog_new                    (CruiX509Cert *cert);

void                       crui_x509_cert_dialog_set_certificate        (CruiX509CertDialog *self, CruiX509Cert *cert);

CruiX509Cert*              crui_x509_cert_dialog_get_certificate        (CruiX509CertDialog *self);

void                       crui_x509_cert_dialog_add_view               (CruiX509CertDialog *self, const gchar *title, 
                                                                         GtkWidget *view);

void                       crui_x509_cert_dialog_insert_view            (CruiX509CertDialog *self, const gchar *title, 
                                                                         GtkWidget *view, gint position);

void                       crui_x509_cert_dialog_focus_view             (CruiX509CertDialog *self, GtkWidget *view);

void                       crui_x509_cert_dialog_remove_view            (CruiX509CertDialog *self, GtkWidget *view);

#endif /* __CRUI_X509_CERT_DIALOG_H__ */
