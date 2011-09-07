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

#include "seahorse-gkr-backend.h"
#include "seahorse-gkr-dialogs.h"
#include "seahorse-gkr-item-commands.h"
#include "seahorse-gkr-keyring-commands.h"
#include "seahorse-gkr-operation.h"

#include "seahorse-progress.h"

#include <gnome-keyring.h>

static SeahorseGkrBackend *gkr_backend = NULL;

struct _SeahorseGkrBackend {
	GObject parent;
	GHashTable *keyrings;
};

struct _SeahorseGkrBackendClass {
	GObjectClass parent_class;
};

static void         seahorse_gkr_backend_collection_init  (GcrCollectionIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseGkrBackend, seahorse_gkr_backend, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_COLLECTION, seahorse_gkr_backend_collection_init));

static void
seahorse_gkr_backend_init (SeahorseGkrBackend *self)
{
	g_return_if_fail (gkr_backend == NULL);
	gkr_backend = self;

	self->keyrings = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                        g_free, g_object_unref);

	/* Let these classes register themselves, when the backend is created */
	g_type_class_unref (g_type_class_ref (SEAHORSE_TYPE_GKR_ITEM_COMMANDS));
	g_type_class_unref (g_type_class_ref (SEAHORSE_TYPE_GKR_KEYRING_COMMANDS));
}

static void
seahorse_gkr_backend_constructed (GObject *obj)
{
	SeahorseGkrBackend *self = SEAHORSE_GKR_BACKEND (obj);

	G_OBJECT_CLASS (seahorse_gkr_backend_parent_class)->constructed (obj);

	seahorse_gkr_backend_load_async (self, NULL, NULL, NULL);
}

static void
seahorse_gkr_backend_dispose (GObject *obj)
{
	SeahorseGkrBackend *self = SEAHORSE_GKR_BACKEND (obj);

	g_hash_table_remove_all (self->keyrings);

	G_OBJECT_CLASS (seahorse_gkr_backend_parent_class)->finalize (obj);
}

static void
seahorse_gkr_backend_finalize (GObject *obj)
{
	SeahorseGkrBackend *self = SEAHORSE_GKR_BACKEND (obj);

	g_hash_table_destroy (self->keyrings);
	g_return_if_fail (gkr_backend == self);
	gkr_backend = NULL;

	G_OBJECT_CLASS (seahorse_gkr_backend_parent_class)->finalize (obj);
}

static void
seahorse_gkr_backend_class_init (SeahorseGkrBackendClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->constructed = seahorse_gkr_backend_constructed;
	gobject_class->dispose = seahorse_gkr_backend_dispose;
	gobject_class->finalize = seahorse_gkr_backend_finalize;
}

static guint
seahorse_gkr_backend_get_length (GcrCollection *collection)
{
	SeahorseGkrBackend *self = SEAHORSE_GKR_BACKEND (collection);
	return g_hash_table_size (self->keyrings);
}

static GList *
seahorse_gkr_backend_get_objects (GcrCollection *collection)
{
	SeahorseGkrBackend *self = SEAHORSE_GKR_BACKEND (collection);
	return seahorse_gkr_backend_get_keyrings (self);
}

static gboolean
seahorse_gkr_backend_contains (GcrCollection *collection,
                               GObject *object)
{
	SeahorseGkrBackend *self = SEAHORSE_GKR_BACKEND (collection);
	const gchar *keyring_name;

	if (!SEAHORSE_IS_GKR_KEYRING (object))
		return FALSE;

	keyring_name = seahorse_gkr_keyring_get_name (SEAHORSE_GKR_KEYRING (object));
	return g_hash_table_lookup (self->keyrings, keyring_name) == object;
}

static void
seahorse_gkr_backend_collection_init (GcrCollectionIface *iface)
{
	iface->contains = seahorse_gkr_backend_contains;
	iface->get_length = seahorse_gkr_backend_get_length;
	iface->get_objects = seahorse_gkr_backend_get_objects;
}

GcrCollection *
seahorse_gkr_backend_initialize (void)
{
	SeahorseGkrBackend *self;
	GcrCollection *collection;

	self = g_object_new (SEAHORSE_TYPE_GKR_BACKEND, NULL);

	/*
	 * For now, the keyrings themselves are the objects, so the
	 * backend is the source
	 */

	collection = gcr_simple_collection_new ();
	gcr_simple_collection_add (GCR_SIMPLE_COLLECTION (collection), G_OBJECT (self));
	g_object_unref (self);

	return collection;
}

SeahorseGkrBackend *
seahorse_gkr_backend_get (void)
{
	g_return_val_if_fail (gkr_backend, NULL);
	return gkr_backend;
}

SeahorseGkrKeyring *
seahorse_gkr_backend_get_keyring (SeahorseGkrBackend *self,
                                 const gchar *keyring_name)
{
	self = self ? self : seahorse_gkr_backend_get ();
	g_return_val_if_fail (SEAHORSE_IS_GKR_BACKEND (self), NULL);
	g_return_val_if_fail (keyring_name != NULL, NULL);
	return g_hash_table_lookup (self->keyrings, keyring_name);
}

GList *
seahorse_gkr_backend_get_keyrings (SeahorseGkrBackend *self)
{
	self = self ? self : seahorse_gkr_backend_get ();
	g_return_val_if_fail (SEAHORSE_IS_GKR_BACKEND (self), NULL);
	return g_hash_table_get_values (self->keyrings);
}

void
seahorse_gkr_backend_remove_keyring (SeahorseGkrBackend *self,
                                     SeahorseGkrKeyring *keyring)
{
	const gchar *keyring_name;

	self = self ? self : seahorse_gkr_backend_get ();
	g_return_if_fail (SEAHORSE_IS_GKR_BACKEND (self));
	g_return_if_fail (SEAHORSE_IS_GKR_KEYRING (keyring));

	keyring_name = seahorse_gkr_keyring_get_name (keyring);
	g_return_if_fail (g_hash_table_lookup (self->keyrings, keyring_name) == keyring);

	g_object_ref (keyring);

	g_hash_table_remove (self->keyrings, keyring_name);
	gcr_collection_emit_removed (GCR_COLLECTION (self), G_OBJECT (keyring));

	g_object_unref (keyring);
}


typedef struct {
	SeahorseGkrBackend *backend;
	GCancellable *cancellable;
	gulong cancelled_sig;
	gpointer request;
	gint num_loads;
} backend_load_closure;

static void
backend_load_free (gpointer data)
{
	backend_load_closure *closure = data;
	g_assert (closure->request == NULL);
	g_assert (closure->num_loads == 0);
	if (closure->cancellable && closure->cancelled_sig)
		g_signal_handler_disconnect (closure->cancellable,
		                             closure->cancelled_sig);
	g_clear_object (&closure->cancellable);
	g_clear_object (&closure->backend);
	g_free (closure);
}

static void
on_backend_load_default_keyring (GnomeKeyringResult result,
                                 const gchar *default_name,
                                 gpointer user_data)
{
	SeahorseGkrBackend *self = SEAHORSE_GKR_BACKEND (user_data);
	const gchar *keyring_name;
	gboolean is_default;
	GList *keyrings, *l;

	if (result != GNOME_KEYRING_RESULT_OK) {
		if (result != GNOME_KEYRING_RESULT_CANCELLED)
			g_warning ("couldn't get default keyring name: %s", gnome_keyring_result_to_message (result));
		return;
	}

	keyrings = seahorse_gkr_backend_get_keyrings (self);
	for (l = keyrings; l != NULL; l = g_list_next (l)) {
		keyring_name = seahorse_gkr_keyring_get_name (l->data);
		g_return_if_fail (keyring_name);

		/* Remember default keyring could be null in strange circumstances */
		is_default = default_name && g_str_equal (keyring_name, default_name);
		g_object_set (l->data, "is-default", is_default, NULL);
	}
	g_list_free (keyrings);
}

static void
on_backend_load_keyring_complete (GObject *object,
                                  GAsyncResult *result,
                                  gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	SeahorseGkrKeyring *keyring = SEAHORSE_GKR_KEYRING (object);
	backend_load_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	g_assert (closure->num_loads > 0);
	closure->num_loads--;
	seahorse_progress_end (closure->cancellable, keyring);

	if (!seahorse_gkr_keyring_load_finish (keyring, result, &error))
		g_simple_async_result_take_error (res, error);

	if (closure->num_loads == 0)
		g_simple_async_result_complete (res);

	g_object_unref (res);
}

static void
on_backend_load_list_keyring_names_complete (GnomeKeyringResult result,
                                            GList *list,
                                            gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	backend_load_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	SeahorseGkrKeyring *keyring;
	GError *error = NULL;
	gchar *keyring_name;
	GHashTableIter iter;
	GHashTable *checks;
	GList *l;

	closure->request = NULL;

	if (seahorse_gkr_propagate_error (result, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);
		return;
	}

	/* Load up a list of all the current names */
	checks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	g_hash_table_iter_init (&iter, closure->backend->keyrings);
	while (g_hash_table_iter_next (&iter, (gpointer *)&keyring_name, (gpointer *)&keyring))
		g_hash_table_insert (checks, g_strdup (keyring_name), g_object_ref (keyring));

	for (l = list; l; l = g_list_next (l)) {
		keyring_name = l->data;

		/* Don't show the 'session' keyring */
		if (g_str_equal (keyring_name, "session"))
			continue;

		keyring = g_hash_table_lookup (checks, keyring_name);

		/* Already have a keyring */
		if (keyring != NULL) {
			g_object_ref (keyring);
			g_hash_table_remove (checks, keyring_name);

		/* Create a new keyring for this one */
		} else {
			keyring = seahorse_gkr_keyring_new (keyring_name);
			g_hash_table_insert (closure->backend->keyrings,
			                     g_strdup (keyring_name),
			                     g_object_ref (keyring));
			gcr_collection_emit_added (GCR_COLLECTION (closure->backend),
			                           G_OBJECT (keyring));
		}

		/* Refresh the keyring as well, and track the load */
		seahorse_gkr_keyring_load_async (keyring, closure->cancellable,
		                                 on_backend_load_keyring_complete,
		                                 g_object_ref (res));
		seahorse_progress_prep_and_begin (closure->cancellable, keyring, NULL);
		closure->num_loads++;
		g_object_unref (keyring);
	}

	g_hash_table_iter_init (&iter, checks);
	while (g_hash_table_iter_next (&iter, (gpointer *)&keyring_name, (gpointer *)&keyring))
		seahorse_gkr_backend_remove_keyring (closure->backend, keyring);
	g_hash_table_destroy (checks);

	if (list == NULL)
		g_simple_async_result_complete_in_idle (res);

	/* Get the default keyring in the background */
	gnome_keyring_get_default_keyring (on_backend_load_default_keyring,
	                                   g_object_ref (closure->backend),
	                                   g_object_unref);
}

static void
on_backend_load_cancelled (GCancellable *cancellable,
                           gpointer user_data)
{
	backend_load_closure *closure = user_data;

	if (closure->request)
		gnome_keyring_cancel_request (closure->request);
}
void
seahorse_gkr_backend_load_async (SeahorseGkrBackend *self,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
	backend_load_closure *closure;
	GSimpleAsyncResult *res;

	self = self ? self : seahorse_gkr_backend_get ();

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_gkr_backend_load_async);
	closure = g_new0 (backend_load_closure, 1);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	closure->backend = g_object_ref (self);
	g_simple_async_result_set_op_res_gpointer (res, closure, backend_load_free);

	closure->request = gnome_keyring_list_keyring_names (on_backend_load_list_keyring_names_complete,
	                                                     g_object_ref (res), g_object_unref);

	if (cancellable)
		closure->cancelled_sig = g_cancellable_connect (cancellable,
		                                                G_CALLBACK (on_backend_load_cancelled),
		                                                closure, NULL);

	g_object_unref (res);
}

gboolean
seahorse_gkr_backend_load_finish (SeahorseGkrBackend *self,
                                  GAsyncResult *result,
                                  GError **error)
{
	self = self ? self : seahorse_gkr_backend_get ();

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
	                      seahorse_gkr_backend_load_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}
