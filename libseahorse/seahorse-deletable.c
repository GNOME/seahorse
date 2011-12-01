/*
 * Seahorse
 *
 * Copyright (C) 2004,2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "seahorse-deletable.h"
#include "seahorse-deleter.h"

#include <glib/gi18n.h>

#include <string.h>

typedef SeahorseDeletableIface SeahorseDeletableInterface;

G_DEFINE_INTERFACE (SeahorseDeletable, seahorse_deletable, G_TYPE_OBJECT);

SeahorseDeleter *
seahorse_deletable_create_deleter (SeahorseDeletable *deletable)
{
	SeahorseDeletableIface *iface;

	g_return_val_if_fail (SEAHORSE_IS_DELETABLE (deletable), NULL);

	iface = SEAHORSE_DELETABLE_GET_INTERFACE (deletable);
	g_return_val_if_fail (iface->create_deleter, NULL);

	return iface->create_deleter (deletable);
}

gboolean
seahorse_deletable_can_delete (gpointer object)
{
	gboolean can = FALSE;

	if (!SEAHORSE_IS_DELETABLE (object))
		return FALSE;

	g_object_get (object, "deletable", &can, NULL);
	return can;
}

static void
seahorse_deletable_default_init (SeahorseDeletableIface *iface)
{
	static gboolean initialized = FALSE;
	if (!initialized) {
		initialized = TRUE;
		g_object_interface_install_property (iface,
		                g_param_spec_boolean ("deletable", "Deletable", "Is actually deletable",
		                                      FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
	}
}

typedef struct {
	GMainLoop *loop;
	GAsyncResult *result;
} WaitClosure;

static WaitClosure *
wait_closure_new (void)
{
	WaitClosure *closure;

	closure = g_slice_new0 (WaitClosure);
	closure->loop = g_main_loop_new (NULL, FALSE);

	return closure;
}

static void
wait_closure_free (WaitClosure *closure)
{
	g_clear_object (&closure->result);
	g_main_loop_unref (closure->loop);
	g_slice_free (WaitClosure, closure);
}

static void
on_wait_complete (GObject *source,
                  GAsyncResult *result,
                  gpointer user_data)
{
	WaitClosure *closure = user_data;
	g_assert (closure->result == NULL);
	closure->result = g_object_ref (result);
	g_main_loop_quit (closure->loop);
}

guint
seahorse_deletable_delete_with_prompt_wait (GList *objects,
                                            GtkWindow *parent,
                                            GError **error)
{
	SeahorseDeleter *deleter;
	WaitClosure *closure;
	GHashTable *pending;
	guint count = 0;
	gboolean ret;
	GList *l, *x;
	GList *deleted;

	g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), 0);
	g_return_val_if_fail (error == NULL || *error == NULL, 0);

	/* A table for monitoring which objects are still pending */
	pending = g_hash_table_new (g_direct_hash, g_direct_equal);
	for (l = objects; l != NULL; l = g_list_next (l))
		g_hash_table_replace (pending, l->data, l->data);

	closure = wait_closure_new ();

	for (l = objects; l != NULL; l = g_list_next (l)) {
		if (!g_hash_table_lookup (pending, l->data))
			continue;

		if (!seahorse_deletable_can_delete (l->data)) {
			g_hash_table_remove (pending, l->data);
			continue;
		}

		deleter = seahorse_deletable_create_deleter (SEAHORSE_DELETABLE (l->data));
		if (!deleter)
			continue;

		/* Now go and add all pending to each exporter */
		for (x = objects; x != NULL; x = g_list_next (x)) {
			if (x->data == l->data)
				continue;
			if (g_hash_table_lookup (pending, x->data))
				seahorse_deleter_add_object (deleter, x->data);
		}

		/* Now show a prompt choosing between the exporters */
		ret = seahorse_deleter_prompt (deleter, parent);

		if (!ret) {
			g_object_unref (deleter);
			break;
		}

		seahorse_deleter_delete_async (deleter, NULL, on_wait_complete, closure);

		g_main_loop_run (closure->loop);

		ret = seahorse_deleter_delete_finish (deleter, closure->result, error);
		g_object_unref (closure->result);
		closure->result = NULL;

		if (ret) {
			deleted = seahorse_deleter_get_objects (deleter);
			for (x = deleted; x != NULL; x = g_list_next (x)) {
				g_hash_table_remove (pending, x->data);
				count++;
			}
		}

		g_object_unref (deleter);

		if (!ret)
			break;
	}

	wait_closure_free (closure);
	g_hash_table_destroy (pending);
	return count;
}
