/* 
 * Seahorse
 * 
 * Copyright (C) 2011 Collabora Ltd.
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"

#include "seahorse-certificate.h"
#include "seahorse-pkcs11-deleter.h"
#include "seahorse-pkcs11-key-deleter.h"
#include "seahorse-private-key.h"
#include "seahorse-token.h"

#include "seahorse-delete-dialog.h"

#include <gck/gck.h>

#include <glib/gi18n.h>

#define SEAHORSE_PKCS11_KEY_DELETER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PKCS11_KEY_DELETER, SeahorsePkcs11KeyDeleterClass))
#define SEAHORSE_IS_PKCS11_KEY_DELETER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PKCS11_KEY_DELETER))
#define SEAHORSE_PKCS11_KEY_DELETER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PKCS11_KEY_DELETER, SeahorsePkcs11KeyDeleterClass))

typedef struct _SeahorsePkcs11KeyDeleterClass SeahorsePkcs11KeyDeleterClass;

struct _SeahorsePkcs11KeyDeleter {
	SeahorsePkcs11Deleter parent;
	SeahorseCertificate *cert;
	SeahorsePrivateKey *key;
	gchar *label;
};

struct _SeahorsePkcs11KeyDeleterClass {
	SeahorsePkcs11DeleterClass parent_class;
};

G_DEFINE_TYPE (SeahorsePkcs11KeyDeleter, seahorse_pkcs11_key_deleter, SEAHORSE_TYPE_PKCS11_DELETER);

static void
seahorse_pkcs11_key_deleter_init (SeahorsePkcs11KeyDeleter *self)
{

}

static void
seahorse_pkcs11_key_deleter_finalize (GObject *obj)
{
	SeahorsePkcs11KeyDeleter *self = SEAHORSE_PKCS11_KEY_DELETER (obj);

	g_free (self->label);

	G_OBJECT_CLASS (seahorse_pkcs11_key_deleter_parent_class)->finalize (obj);
}

static GtkDialog *
seahorse_pkcs11_key_deleter_create_confirm (SeahorseDeleter *deleter,
                                            GtkWindow *parent)
{
	SeahorsePkcs11KeyDeleter *self = SEAHORSE_PKCS11_KEY_DELETER (deleter);
	GtkDialog *dialog;
	gchar *prompt;

	prompt = g_strdup_printf (_("Are you sure you want to permanently delete %s?"), self->label);

	dialog = seahorse_delete_dialog_new (parent, "%s", prompt);

	seahorse_delete_dialog_set_check_label (SEAHORSE_DELETE_DIALOG (dialog), _("I understand that this key will be permanently deleted."));
	seahorse_delete_dialog_set_check_require (SEAHORSE_DELETE_DIALOG (dialog), TRUE);

	g_free (prompt);
	return dialog;
}

static gboolean
seahorse_pkcs11_key_deleter_add_object (SeahorseDeleter *deleter,
                                        GObject *object)
{
	SeahorsePkcs11KeyDeleter *self = SEAHORSE_PKCS11_KEY_DELETER (deleter);
	SeahorsePkcs11Deleter *base = SEAHORSE_PKCS11_DELETER (deleter);
	gpointer partner = NULL;

	if (SEAHORSE_IS_PRIVATE_KEY (object)) {
		if (self->key != NULL)
			return FALSE;
		if (self->cert != NULL) {
			partner = seahorse_certificate_get_partner (SEAHORSE_CERTIFICATE (self->cert));
			if (partner != object)
				return FALSE;
		}
		self->key = SEAHORSE_PRIVATE_KEY (object);
		base->objects = g_list_prepend (base->objects, g_object_ref (self->key));

	} else if (SEAHORSE_IS_CERTIFICATE (object)) {
		if (self->cert != NULL)
			return FALSE;
		if (self->key != NULL) {
			partner = seahorse_private_key_get_partner (SEAHORSE_PRIVATE_KEY (self->key));
			if (partner != object)
				return FALSE;
		}
		self->cert = SEAHORSE_CERTIFICATE (object);
		base->objects = g_list_append (base->objects, g_object_ref (self->cert));
	}

	if (self->label == NULL)
		g_object_get (object, "label", &self->label, NULL);
	return TRUE;
}

static void
seahorse_pkcs11_key_deleter_class_init (SeahorsePkcs11KeyDeleterClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseDeleterClass *deleter_class = SEAHORSE_DELETER_CLASS (klass);

	deleter_class->add_object = seahorse_pkcs11_key_deleter_add_object;
	deleter_class->create_confirm = seahorse_pkcs11_key_deleter_create_confirm;

	gobject_class->finalize = seahorse_pkcs11_key_deleter_finalize;
}

SeahorseDeleter *
seahorse_pkcs11_key_deleter_new (GObject *cert_or_key)
{
	SeahorseDeleter *deleter;

	deleter = g_object_new (SEAHORSE_TYPE_PKCS11_KEY_DELETER, NULL);
	if (!seahorse_deleter_add_object (deleter, cert_or_key))
		g_assert_not_reached ();

	return deleter;
}
