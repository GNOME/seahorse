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

#include "seahorse-pkcs11-deleter.h"
#include "seahorse-token.h"

#include "seahorse-delete-dialog.h"

#include <gck/gck.h>

#include <glib/gi18n.h>

G_DEFINE_TYPE (SeahorsePkcs11Deleter, seahorse_pkcs11_deleter, SEAHORSE_TYPE_DELETER);

static void
seahorse_pkcs11_deleter_init (SeahorsePkcs11Deleter *self)
{

}

static void
seahorse_pkcs11_deleter_finalize (GObject *obj)
{
	SeahorsePkcs11Deleter *self = SEAHORSE_PKCS11_DELETER (obj);

	g_list_free_full (self->objects, g_object_unref);

	G_OBJECT_CLASS (seahorse_pkcs11_deleter_parent_class)->finalize (obj);
}

static GtkDialog *
seahorse_pkcs11_deleter_create_confirm (SeahorseDeleter *deleter,
                                        GtkWindow *parent)
{
	SeahorsePkcs11Deleter *self = SEAHORSE_PKCS11_DELETER (deleter);
	GtkDialog *dialog;
	gchar *prompt;
	gchar *label;
	guint num;

	num = g_list_length (self->objects);
	if (num == 1) {
		g_object_get (self->objects->data, "label", &label, NULL);
		prompt = g_strdup_printf (_("Are you sure you want to permanently delete %s?"), label);
		g_free (label);
	} else {
		prompt = g_strdup_printf (ngettext ("Are you sure you want to permanently delete %d certificate?",
		                                    "Are you sure you want to permanently delete %d certificates?",
		                                    num),
		                          num);
	}

	dialog = seahorse_delete_dialog_new (parent, "%s", prompt);
	g_free (prompt);

	return dialog;
}

static GList *
seahorse_pkcs11_deleter_get_objects (SeahorseDeleter *deleter)
{
	SeahorsePkcs11Deleter *self = SEAHORSE_PKCS11_DELETER (deleter);
	return self->objects;
}

static gboolean
seahorse_pkcs11_deleter_add_object (SeahorseDeleter *deleter,
                                    GObject *object)
{
	SeahorsePkcs11Deleter *self = SEAHORSE_PKCS11_DELETER (deleter);

	if (!SEAHORSE_IS_CERTIFICATE (object))
		return FALSE;

	self->objects = g_list_append (self->objects, g_object_ref (object));
	return TRUE;
}

typedef struct {
	GQueue *objects;
	GCancellable *cancellable;
} DeleteClosure;


static void
delete_closure_free (gpointer data)
{
	DeleteClosure *closure = data;
	g_queue_foreach (closure->objects, (GFunc)g_object_unref, NULL);
	g_queue_free (closure->objects);
	g_clear_object (&closure->cancellable);
	g_free (closure);
}

static void pkcs11_delete_one_object (GSimpleAsyncResult *res);

static void
on_delete_object_completed (GObject *source,
                            GAsyncResult *result,
                            gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	DeleteClosure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;
	GckObject *object;
	SeahorseToken *pkcs11_token;

	object = g_queue_pop_head (closure->objects);

	if (!gck_object_destroy_finish (GCK_OBJECT (source), result, &error)) {

		/* Ignore objects that have gone away */
		if (g_error_matches (error, GCK_ERROR, CKR_OBJECT_HANDLE_INVALID)) {
			g_clear_error (&error);
		} else {
			g_simple_async_result_take_error (res, error);
			g_simple_async_result_complete (res);
		}

	}

	if (error == NULL) {
		g_object_get (object, "place", &pkcs11_token, NULL);
		g_return_if_fail (SEAHORSE_IS_TOKEN (pkcs11_token));
		seahorse_token_remove_object (pkcs11_token, GCK_OBJECT (object));
		pkcs11_delete_one_object (res);
	}

	g_object_unref (object);
	g_object_unref (res);
}

static void
pkcs11_delete_one_object (GSimpleAsyncResult *res)
{
	DeleteClosure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GckObject *object;

	if (g_queue_is_empty (closure->objects)) {
		g_simple_async_result_complete_in_idle (res);
		return;
	}

	object = g_queue_peek_head (closure->objects);

	gck_object_destroy_async (object, closure->cancellable,
	                          on_delete_object_completed, g_object_ref (res));
}

static void
seahorse_pkcs11_deleter_delete_async (SeahorseDeleter *deleter,
                                      GCancellable *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
	SeahorsePkcs11Deleter *self = SEAHORSE_PKCS11_DELETER (deleter);
	GSimpleAsyncResult *res;
	DeleteClosure *closure;
	GList *l;

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_pkcs11_deleter_delete_async);
	closure = g_new0 (DeleteClosure, 1);
	closure->objects = g_queue_new ();
	for (l = self->objects; l != NULL; l = g_list_next (l))
		g_queue_push_tail (closure->objects, g_object_ref (l->data));
	g_simple_async_result_set_op_res_gpointer (res, closure, delete_closure_free);

	pkcs11_delete_one_object (res);

	g_object_unref (res);
}

static gboolean
seahorse_pkcs11_deleter_delete_finish (SeahorseDeleter *deleter,
                                       GAsyncResult *result,
                                       GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (deleter),
	                      seahorse_pkcs11_deleter_delete_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

static void
seahorse_pkcs11_deleter_class_init (SeahorsePkcs11DeleterClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseDeleterClass *deleter_class = SEAHORSE_DELETER_CLASS (klass);

	gobject_class->finalize = seahorse_pkcs11_deleter_finalize;

	deleter_class->add_object = seahorse_pkcs11_deleter_add_object;
	deleter_class->create_confirm = seahorse_pkcs11_deleter_create_confirm;
	deleter_class->delete_async = seahorse_pkcs11_deleter_delete_async;
	deleter_class->delete_finish = seahorse_pkcs11_deleter_delete_finish;
	deleter_class->get_objects = seahorse_pkcs11_deleter_get_objects;
}

SeahorseDeleter *
seahorse_pkcs11_deleter_new (SeahorseCertificate *certificate)
{
	SeahorseDeleter *deleter;

	deleter = g_object_new (SEAHORSE_TYPE_PKCS11_DELETER, NULL);
	if (!seahorse_deleter_add_object (deleter, G_OBJECT (certificate)))
		g_assert_not_reached ();

	return deleter;
}
