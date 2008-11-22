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

#include "crui-x509-cert-dialog.h"

#include "crui-x509-cert-basics.h"

enum {
	PROP_0,
	PROP_CERTIFICATE
};

struct _CruiX509CertDialogPrivate {
	GtkNotebook *tabs;
	CruiX509CertBasics *basics;
};

G_DEFINE_TYPE (CruiX509CertDialog, crui_x509_cert_dialog, GTK_TYPE_DIALOG);

#define CRUI_X509_CERT_DIALOG_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), CRUI_TYPE_X509_CERT_DIALOG, CruiX509CertDialogPrivate))

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

/* -----------------------------------------------------------------------------
 * OBJECT 
 */


static void
crui_x509_cert_dialog_init (CruiX509CertDialog *self)
{
	CruiX509CertDialogPrivate *pv = CRUI_X509_CERT_DIALOG_GET_PRIVATE (self);
	GtkWidget *button;
	
	pv->tabs = GTK_NOTEBOOK (gtk_notebook_new ());
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (self)->vbox), GTK_WIDGET (pv->tabs));
	gtk_container_set_border_width (GTK_CONTAINER (self), 5);
	gtk_container_set_border_width (GTK_CONTAINER (pv->tabs), 5);
	gtk_widget_show (GTK_WIDGET (pv->tabs));
	
	pv->basics = crui_x509_cert_basics_new (NULL);
	crui_x509_cert_dialog_add_view (self, _("Certificate"), GTK_WIDGET (pv->basics));
	gtk_widget_show (GTK_WIDGET (pv->basics));
	
	button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	gtk_dialog_add_action_widget (GTK_DIALOG (self), button, GTK_RESPONSE_CLOSE);
	gtk_widget_show (button);
	
	gtk_dialog_set_has_separator (GTK_DIALOG (self), FALSE);
}

static void
crui_x509_cert_dialog_dispose (GObject *obj)
{
	CruiX509CertDialog *self = CRUI_X509_CERT_DIALOG (obj);
	CruiX509CertDialogPrivate *pv = CRUI_X509_CERT_DIALOG_GET_PRIVATE (self);
	
	if (pv->basics) {
		crui_x509_cert_dialog_remove_view (self, GTK_WIDGET (pv->basics));
		pv->basics = NULL;
	}
    
	G_OBJECT_CLASS (crui_x509_cert_dialog_parent_class)->dispose (obj);
}

static void
crui_x509_cert_dialog_finalize (GObject *obj)
{
	CruiX509CertDialog *self = CRUI_X509_CERT_DIALOG (obj);
	CruiX509CertDialogPrivate *pv = CRUI_X509_CERT_DIALOG_GET_PRIVATE (self);

	g_assert (pv->basics == NULL);
	
	G_OBJECT_CLASS (crui_x509_cert_dialog_parent_class)->finalize (obj);
}

static void
crui_x509_cert_dialog_set_property (GObject *obj, guint prop_id, const GValue *value, 
                           GParamSpec *pspec)
{
	CruiX509CertDialog *self = CRUI_X509_CERT_DIALOG (obj);
	
	switch (prop_id) {
	case PROP_CERTIFICATE:
		crui_x509_cert_dialog_set_certificate (self, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
crui_x509_cert_dialog_get_property (GObject *obj, guint prop_id, GValue *value, 
                           GParamSpec *pspec)
{
	CruiX509CertDialog *self = CRUI_X509_CERT_DIALOG (obj);
	
	switch (prop_id) {
	case PROP_CERTIFICATE:
		g_value_set_object (value, crui_x509_cert_dialog_get_certificate (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
crui_x509_cert_dialog_class_init (CruiX509CertDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	crui_x509_cert_dialog_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (CruiX509CertDialogPrivate));

	gobject_class->dispose = crui_x509_cert_dialog_dispose;
	gobject_class->finalize = crui_x509_cert_dialog_finalize;
	gobject_class->set_property = crui_x509_cert_dialog_set_property;
	gobject_class->get_property = crui_x509_cert_dialog_get_property;

	g_object_class_install_property (gobject_class, PROP_CERTIFICATE,
	           g_param_spec_object ("certificate", "Certificate", "Certificate to display", 
	                                CRUI_TYPE_X509_CERT, G_PARAM_READWRITE));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

CruiX509CertDialog*
crui_x509_cert_dialog_new (CruiX509Cert *cert)
{
	return g_object_new (CRUI_TYPE_X509_CERT_DIALOG, "certificate", cert, NULL);
}

void
crui_x509_cert_dialog_set_certificate (CruiX509CertDialog *self, CruiX509Cert *cert)
{
	CruiX509CertDialogPrivate *pv = CRUI_X509_CERT_DIALOG_GET_PRIVATE (self);
	g_return_if_fail (CRUI_IS_X509_CERT_DIALOG (self));
	
	crui_x509_cert_basics_set_certificate (pv->basics, cert);
	g_object_notify (G_OBJECT (self), "certificate");
}

CruiX509Cert*
crui_x509_cert_dialog_get_certificate (CruiX509CertDialog *self)
{
	CruiX509CertDialogPrivate *pv = CRUI_X509_CERT_DIALOG_GET_PRIVATE (self);
	g_return_val_if_fail (CRUI_IS_X509_CERT_DIALOG (self), NULL);
	
	return crui_x509_cert_basics_get_certificate (pv->basics);
}


void
crui_x509_cert_dialog_add_view (CruiX509CertDialog *self, const gchar *title, GtkWidget *view)
{
	g_return_if_fail (CRUI_IS_X509_CERT_DIALOG (self));
	crui_x509_cert_dialog_insert_view (self, title, view, -1);
}

void
crui_x509_cert_dialog_insert_view (CruiX509CertDialog *self, const gchar *title, 
                                   GtkWidget *view, gint position)
{
	CruiX509CertDialogPrivate *pv = CRUI_X509_CERT_DIALOG_GET_PRIVATE (self);
	
	g_return_if_fail (CRUI_IS_X509_CERT_DIALOG (self));
	g_return_if_fail (title);
	
	g_return_if_fail (GTK_IS_WIDGET (view));
	g_return_if_fail (gtk_notebook_page_num (pv->tabs, view) == -1);
	
	gtk_notebook_insert_page (pv->tabs, view, gtk_label_new (title), position);
}

void
crui_x509_cert_dialog_focus_view (CruiX509CertDialog *self, GtkWidget *view)
{
	CruiX509CertDialogPrivate *pv = CRUI_X509_CERT_DIALOG_GET_PRIVATE (self);
	gint page;
	
	g_return_if_fail (CRUI_IS_X509_CERT_DIALOG (self));
	g_return_if_fail (GTK_IS_WIDGET (view));
	
	page = gtk_notebook_page_num (pv->tabs, view);
	g_return_if_fail (page != -1);
	
	gtk_notebook_set_current_page (pv->tabs, page);
}

void
crui_x509_cert_dialog_remove_view (CruiX509CertDialog *self, GtkWidget *view)
{
	CruiX509CertDialogPrivate *pv = CRUI_X509_CERT_DIALOG_GET_PRIVATE (self);
	gint page;
	
	g_return_if_fail (CRUI_IS_X509_CERT_DIALOG (self));
	g_return_if_fail (GTK_IS_WIDGET (view));
	
	page = gtk_notebook_page_num (pv->tabs, view);
	g_return_if_fail (page != -1);
	
	gtk_notebook_remove_page (pv->tabs, page);
}
