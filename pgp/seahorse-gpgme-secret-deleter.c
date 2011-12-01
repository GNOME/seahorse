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

#include "seahorse-gpgme.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-gpgme-secret-deleter.h"

#include "seahorse-delete-dialog.h"

#include <glib/gi18n.h>

#define SEAHORSE_GPGME_SECRET_DELETER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GPGME_SECRET_DELETER, SeahorseGpgmeSecretDeleterClass))
#define SEAHORSE_IS_GPGME_SECRET_DELETER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GPGME_SECRET_DELETER))
#define SEAHORSE_GPGME_SECRET_DELETER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GPGME_SECRET_DELETER, SeahorseGpgmeSecretDeleterClass))

typedef struct _SeahorseGpgmeSecretDeleterClass SeahorseGpgmeSecretDeleterClass;

struct _SeahorseGpgmeSecretDeleter {
	SeahorseDeleter parent;
	SeahorseGpgmeKey *key;
	GList *keys;
};

struct _SeahorseGpgmeSecretDeleterClass {
	SeahorseDeleterClass parent_class;
};

G_DEFINE_TYPE (SeahorseGpgmeSecretDeleter, seahorse_gpgme_secret_deleter, SEAHORSE_TYPE_DELETER);

static void
seahorse_gpgme_secret_deleter_init (SeahorseGpgmeSecretDeleter *self)
{

}

static void
seahorse_gpgme_secret_deleter_finalize (GObject *obj)
{
	SeahorseGpgmeSecretDeleter *self = SEAHORSE_GPGME_SECRET_DELETER (obj);

	g_clear_object (&self->key);
	g_list_free (self->keys);

	G_OBJECT_CLASS (seahorse_gpgme_secret_deleter_parent_class)->finalize (obj);
}

static GtkDialog *
seahorse_gpgme_secret_deleter_create_confirm (SeahorseDeleter *deleter,
                                           GtkWindow *parent)
{
	SeahorseGpgmeSecretDeleter *self = SEAHORSE_GPGME_SECRET_DELETER (deleter);
	GtkDialog *dialog;
	gchar *prompt;

	prompt = g_strdup_printf (_("Are you sure you want to permanently delete %s?"),
	                          seahorse_object_get_label (SEAHORSE_OBJECT (self->key)));

	dialog = seahorse_delete_dialog_new (parent, prompt);

	seahorse_delete_dialog_set_check_label (SEAHORSE_DELETE_DIALOG (dialog), _("I understand that this secret key will be permanently deleted."));
	seahorse_delete_dialog_set_check_require (SEAHORSE_DELETE_DIALOG (dialog), TRUE);

	g_free (prompt);
	return dialog;
}

static GList *
seahorse_gpgme_secret_deleter_get_objects (SeahorseDeleter *deleter)
{
	SeahorseGpgmeSecretDeleter *self = SEAHORSE_GPGME_SECRET_DELETER (deleter);
	return self->keys;
}

static gboolean
seahorse_gpgme_secret_deleter_add_object (SeahorseDeleter *deleter,
                                          GObject *object)
{
	SeahorseGpgmeSecretDeleter *self = SEAHORSE_GPGME_SECRET_DELETER (deleter);

	if (!SEAHORSE_IS_GPGME_KEY (object))
		return FALSE;
	if (self->key)
		return FALSE;
	if (seahorse_object_get_usage (SEAHORSE_OBJECT (object)) != SEAHORSE_USAGE_PRIVATE_KEY)
		return FALSE;
	self->key = g_object_ref (object);
	self->keys = g_list_append (self->keys, object);
	return TRUE;
}

static void
seahorse_gpgme_secret_deleter_delete_async (SeahorseDeleter *deleter,
                                            GCancellable *cancellable,
                                            GAsyncReadyCallback callback,
                                            gpointer user_data)
{
	SeahorseGpgmeSecretDeleter *self = SEAHORSE_GPGME_SECRET_DELETER (deleter);
	GSimpleAsyncResult *res;
	GError *error = NULL;
	gpgme_error_t gerr;

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_gpgme_secret_deleter_delete_async);

	gerr = seahorse_gpgme_key_op_delete_pair (self->key);
	if (seahorse_gpgme_propagate_error (gerr, &error))
		g_simple_async_result_take_error (res, error);

	g_simple_async_result_complete_in_idle (res);
	g_object_unref (res);
}

static gboolean
seahorse_gpgme_secret_deleter_delete_finish (SeahorseDeleter *deleter,
                                             GAsyncResult *result,
                                             GError **error)
{
	SeahorseGpgmeSecretDeleter *self = SEAHORSE_GPGME_SECRET_DELETER (deleter);

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
	                      seahorse_gpgme_secret_deleter_delete_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}


static void
seahorse_gpgme_secret_deleter_class_init (SeahorseGpgmeSecretDeleterClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseDeleterClass *deleter_class = SEAHORSE_DELETER_CLASS (klass);

	gobject_class->finalize = seahorse_gpgme_secret_deleter_finalize;

	deleter_class->add_object = seahorse_gpgme_secret_deleter_add_object;
	deleter_class->create_confirm = seahorse_gpgme_secret_deleter_create_confirm;
	deleter_class->delete_async = seahorse_gpgme_secret_deleter_delete_async;
	deleter_class->delete_finish = seahorse_gpgme_secret_deleter_delete_finish;
	deleter_class->get_objects = seahorse_gpgme_secret_deleter_get_objects;
}

SeahorseDeleter *
seahorse_gpgme_secret_deleter_new (SeahorseGpgmeKey *key)
{
	SeahorseDeleter *deleter;

	deleter = g_object_new (SEAHORSE_TYPE_GPGME_SECRET_DELETER, NULL);
	if (!seahorse_deleter_add_object (deleter, G_OBJECT (key)))
		g_assert_not_reached ();

	return deleter;
}
