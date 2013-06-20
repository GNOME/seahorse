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

#include "seahorse-ssh-key.h"
#include "seahorse-ssh-deleter.h"
#include "seahorse-ssh-operation.h"

#include "seahorse-common.h"

#include <glib/gi18n.h>

#define SEAHORSE_SSH_DELETER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SSH_DELETER, SeahorseSshDeleterClass))
#define SEAHORSE_IS_SSH_DELETER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SSH_DELETER))
#define SEAHORSE_SSH_DELETER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SSH_DELETER, SeahorseSshDeleterClass))

typedef struct _SeahorseSshDeleterClass SeahorseSshDeleterClass;

struct _SeahorseSshDeleter {
	SeahorseDeleter parent;
	gboolean have_private;
	GList *keys;
};

struct _SeahorseSshDeleterClass {
	SeahorseDeleterClass parent_class;
};

G_DEFINE_TYPE (SeahorseSshDeleter, seahorse_ssh_deleter, SEAHORSE_TYPE_DELETER);

static void
seahorse_ssh_deleter_init (SeahorseSshDeleter *self)
{

}

static void
seahorse_ssh_deleter_finalize (GObject *obj)
{
	SeahorseSshDeleter *self = SEAHORSE_SSH_DELETER (obj);

	g_list_free_full (self->keys, g_object_unref);

	G_OBJECT_CLASS (seahorse_ssh_deleter_parent_class)->finalize (obj);
}

static GtkDialog *
seahorse_ssh_deleter_create_confirm (SeahorseDeleter *deleter,
                                           GtkWindow *parent)
{
	SeahorseSshDeleter *self = SEAHORSE_SSH_DELETER (deleter);
	const gchar *confirm;
	GtkDialog *dialog;
	gchar *prompt;
	guint num;

	num = g_list_length (self->keys);
	if (self->have_private) {
		g_return_val_if_fail (num == 1, NULL);
		prompt = g_strdup_printf (_("Are you sure you want to delete the secure shell key '%s'?"),
		                          seahorse_object_get_label (SEAHORSE_OBJECT (self->keys->data)));
		confirm = _("I understand that this secret key will be permanently deleted.");

	} else if (num == 1) {
		prompt = g_strdup_printf (_("Are you sure you want to delete the secure shell key '%s'?"),
		                          seahorse_object_get_label (SEAHORSE_OBJECT (self->keys->data)));
		confirm = NULL;

	} else {
		prompt = g_strdup_printf (ngettext ("Are you sure you want to delete %d secure shell key?",
		                                    "Are you sure you want to delete %d secure shell keys?",
		                                    num),
		                          num);
		confirm = NULL;
	}

	dialog = seahorse_delete_dialog_new (parent, "%s", prompt);
	g_free (prompt);

	if (confirm) {
		seahorse_delete_dialog_set_check_label (SEAHORSE_DELETE_DIALOG (dialog), confirm);
		seahorse_delete_dialog_set_check_require (SEAHORSE_DELETE_DIALOG (dialog), TRUE);
	}

	return g_object_ref (dialog);
}

static GList *
seahorse_ssh_deleter_get_objects (SeahorseDeleter *deleter)
{
	SeahorseSshDeleter *self = SEAHORSE_SSH_DELETER (deleter);
	return self->keys;
}

static gboolean
seahorse_ssh_deleter_add_object (SeahorseDeleter *deleter,
                                 GObject *object)
{
	SeahorseSshDeleter *self = SEAHORSE_SSH_DELETER (deleter);
	SeahorseUsage usage;

	if (!SEAHORSE_IS_SSH_KEY (object))
		return FALSE;
	if (self->have_private)
		return FALSE;
	usage = seahorse_object_get_usage (SEAHORSE_OBJECT (object));
	if (usage == SEAHORSE_USAGE_PRIVATE_KEY) {
		if (self->keys != NULL)
			return FALSE;
		self->have_private = TRUE;
	}

	self->keys = g_list_append (self->keys, g_object_ref (object));
	return TRUE;
}

static void
seahorse_ssh_deleter_delete_async (SeahorseDeleter *deleter,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
	SeahorseSshDeleter *self = SEAHORSE_SSH_DELETER (deleter);
	GSimpleAsyncResult *res;
	GError *error = NULL;
	GList *l;

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_ssh_deleter_delete_async);

	for (l = self->keys; l != NULL && !g_cancellable_is_cancelled (cancellable);
	     l = g_list_next (l)) {
		if (!seahorse_ssh_op_delete_sync (l->data, &error)) {
			g_simple_async_result_take_error (res, error);
			break;
		}
	}

	g_simple_async_result_complete_in_idle (res);
	g_object_unref (res);
}

static gboolean
seahorse_ssh_deleter_delete_finish (SeahorseDeleter *deleter,
                                    GAsyncResult *result,
                                    GError **error)
{
	SeahorseSshDeleter *self = SEAHORSE_SSH_DELETER (deleter);

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
	                      seahorse_ssh_deleter_delete_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

static void
seahorse_ssh_deleter_class_init (SeahorseSshDeleterClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseDeleterClass *deleter_class = SEAHORSE_DELETER_CLASS (klass);

	gobject_class->finalize = seahorse_ssh_deleter_finalize;

	deleter_class->add_object = seahorse_ssh_deleter_add_object;
	deleter_class->create_confirm = seahorse_ssh_deleter_create_confirm;
	deleter_class->delete = seahorse_ssh_deleter_delete_async;
	deleter_class->delete_finish = seahorse_ssh_deleter_delete_finish;
	deleter_class->get_objects = seahorse_ssh_deleter_get_objects;
}

SeahorseDeleter *
seahorse_ssh_deleter_new (SeahorseSSHKey *item)
{
	SeahorseDeleter *deleter;

	deleter = g_object_new (SEAHORSE_TYPE_SSH_DELETER, NULL);
	if (!seahorse_deleter_add_object (deleter, G_OBJECT (item)))
		g_assert_not_reached ();

	return deleter;
}
