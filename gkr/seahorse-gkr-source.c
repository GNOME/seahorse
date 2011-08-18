/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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

#include <stdlib.h>
#include <string.h>
#include <libintl.h>

#include <glib/gi18n.h>

#include "seahorse-util.h"
#include "seahorse-secure-memory.h"
#include "seahorse-passphrase.h"
#include "seahorse-progress.h"

#include "seahorse-gkr-item.h"
#include "seahorse-gkr-keyring.h"
#include "seahorse-gkr-operation.h"
#include "seahorse-gkr-source.h"

#include "common/seahorse-registry.h"

#include <gnome-keyring.h>

enum {
    PROP_0,
    PROP_SOURCE_TAG,
    PROP_SOURCE_LOCATION,
    PROP_FLAGS
};

static void seahorse_source_iface (SeahorseSourceIface *iface);

G_DEFINE_TYPE_EXTENDED (SeahorseGkrSource, seahorse_gkr_source, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_SOURCE, seahorse_source_iface));

static SeahorseGkrSource *default_source = NULL;

/* -----------------------------------------------------------------------------
 * OBJECT
 */


static GObject* 
seahorse_gkr_source_constructor (GType type, guint n_props, GObjectConstructParam *props)
{
	SeahorseGkrSource *self = SEAHORSE_GKR_SOURCE (G_OBJECT_CLASS (seahorse_gkr_source_parent_class)->constructor (type, n_props, props));
	
	g_return_val_if_fail (SEAHORSE_IS_GKR_SOURCE (self), NULL);
	
	if (default_source == NULL)
		default_source = self;

	return G_OBJECT (self);

}

static void
seahorse_gkr_source_init (SeahorseGkrSource *self)
{
	self->pv = NULL;
}

static void 
seahorse_gkr_source_get_property (GObject *object, guint prop_id, GValue *value, 
                                       GParamSpec *pspec)
{
	switch (prop_id) {
	case PROP_SOURCE_TAG:
		g_value_set_uint (value, SEAHORSE_GKR);
		break;
	case PROP_SOURCE_LOCATION:
		g_value_set_enum (value, SEAHORSE_LOCATION_LOCAL);
		break;
	case PROP_FLAGS:
		g_value_set_uint (value, 0);
		break;
	}
}

static void 
seahorse_gkr_source_set_property (GObject *object, guint prop_id, const GValue *value, 
                                  GParamSpec *pspec)
{

}

static void
seahorse_gkr_source_finalize (GObject *obj)
{
	SeahorseGkrSource *self = SEAHORSE_GKR_SOURCE (obj);

	if (default_source == self)
		default_source = NULL;
	
	G_OBJECT_CLASS (seahorse_gkr_source_parent_class)->finalize (obj);
}

static void
seahorse_gkr_source_class_init (SeahorseGkrSourceClass *klass)
{
	GObjectClass *gobject_class;
    
	seahorse_gkr_source_parent_class = g_type_class_peek_parent (klass);

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->constructor = seahorse_gkr_source_constructor;
	gobject_class->set_property = seahorse_gkr_source_set_property;
	gobject_class->get_property = seahorse_gkr_source_get_property;
	gobject_class->finalize = seahorse_gkr_source_finalize;
    
	g_object_class_install_property (gobject_class, PROP_FLAGS,
	         g_param_spec_uint ("flags", "Flags", "Object Source flags.", 
	                            0, G_MAXUINT, 0, G_PARAM_READABLE));

	g_object_class_override_property (gobject_class, PROP_SOURCE_TAG, "source-tag");
	g_object_class_override_property (gobject_class, PROP_SOURCE_LOCATION, "source-location");
    
	seahorse_registry_register_type (NULL, SEAHORSE_TYPE_GKR_SOURCE, "source", "local", SEAHORSE_GKR_STR, NULL);
}

typedef struct {
	SeahorseGkrSource *source;
	GCancellable *cancellable;
	gulong cancelled_sig;
	gpointer request;
	gint num_loads;
} source_load_closure;

static void
source_load_free (gpointer data)
{
	source_load_closure *closure = data;
	g_assert (closure->request == NULL);
	g_assert (closure->num_loads == 0);
	if (closure->cancellable && closure->cancelled_sig)
		g_signal_handler_disconnect (closure->cancellable,
		                             closure->cancelled_sig);
	g_clear_object (&closure->cancellable);
	g_clear_object (&closure->source);
	g_free (closure);
}

static void
update_each_default_keyring (SeahorseObject *object, gpointer user_data)
{
	const gchar *default_name = user_data;
	const gchar *keyring_name;
	gboolean is_default;

	keyring_name = seahorse_gkr_keyring_get_name (SEAHORSE_GKR_KEYRING (object));
	g_return_if_fail (keyring_name);

	/* Remember default keyring could be null in strange circumstances */
	is_default = default_name && g_str_equal (keyring_name, default_name);
	g_object_set (object, "is-default", is_default, NULL);
}

static void
on_source_load_default_keyring (GnomeKeyringResult result,
                                const gchar *default_name,
                                gpointer user_data)
{
	SeahorseGkrSource *self = SEAHORSE_GKR_SOURCE (user_data);
	SeahorseObjectPredicate pred;

	if (result != GNOME_KEYRING_RESULT_OK) {
		if (result != GNOME_KEYRING_RESULT_CANCELLED)
			g_warning ("couldn't get default keyring name: %s", gnome_keyring_result_to_message (result));
		return;
	}

	seahorse_object_predicate_clear (&pred);
	pred.source = SEAHORSE_SOURCE (self);
	pred.type = SEAHORSE_TYPE_GKR_KEYRING;
	seahorse_context_for_objects_full (NULL, &pred, update_each_default_keyring, (gpointer)default_name);
}

static void
on_source_load_keyring_complete (GObject *source,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_load_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	g_assert (closure->num_loads > 0);
	closure->num_loads--;
	seahorse_progress_end (closure->cancellable, source);

	if (!seahorse_source_load_finish (SEAHORSE_SOURCE (source), result, &error))
		g_simple_async_result_take_error (res, error);

	if (closure->num_loads == 0)
		g_simple_async_result_complete (res);

	g_object_unref (res);
}

static void
remove_each_keyring_from_context (const gchar *keyring_name, SeahorseObject *keyring,
                                  gpointer unused)
{
	seahorse_context_remove_object (NULL, keyring);
	seahorse_context_remove_source (NULL, SEAHORSE_SOURCE (keyring));
}

static void
insert_each_keyring_in_hashtable (SeahorseObject *object, gpointer user_data)
{
	g_hash_table_insert ((GHashTable*)user_data,
	                     g_strdup (seahorse_gkr_keyring_get_name (SEAHORSE_GKR_KEYRING (object))),
	                     g_object_ref (object));
}

static void
on_source_load_list_keyring_names_complete (GnomeKeyringResult result,
                                            GList *list,
                                            gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_load_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	SeahorseGkrKeyring *keyring;
	SeahorseObjectPredicate pred;
	GError *error = NULL;
	gchar *keyring_name;
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
	seahorse_object_predicate_clear (&pred);
	pred.source = SEAHORSE_SOURCE (closure->source);
	pred.type = SEAHORSE_TYPE_GKR_KEYRING;
	seahorse_context_for_objects_full (NULL, &pred, insert_each_keyring_in_hashtable, checks);

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
			g_object_set (keyring, "source", closure->source, NULL);
			seahorse_context_add_source (NULL, SEAHORSE_SOURCE (keyring));
			seahorse_context_add_object (NULL, SEAHORSE_OBJECT (keyring));
		}

		/* Refresh the keyring as well, and track the load */
		seahorse_source_load_async (SEAHORSE_SOURCE (keyring), closure->cancellable,
		                            on_source_load_keyring_complete, g_object_ref (res));
		seahorse_progress_prep_and_begin (closure->cancellable, keyring, NULL);
		closure->num_loads++;
		g_object_unref (keyring);
	}

	g_hash_table_foreach (checks, (GHFunc)remove_each_keyring_from_context, NULL);
	g_hash_table_destroy (checks);

	if (list == NULL)
		g_simple_async_result_complete_in_idle (res);

	/* Get the default keyring in the background */
	gnome_keyring_get_default_keyring (on_source_load_default_keyring,
	                                   g_object_ref (closure->source),
	                                   g_object_unref);
}

static void
on_source_load_cancelled (GCancellable *cancellable,
                          gpointer user_data)
{
	source_load_closure *closure = user_data;

	if (closure->request)
		gnome_keyring_cancel_request (closure->request);
}

static void
seahorse_gkr_source_load_async (SeahorseSource *source,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
	SeahorseGkrSource *self = SEAHORSE_GKR_SOURCE (source);
	source_load_closure *closure;
	GSimpleAsyncResult *res;

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_gkr_source_load_async);
	closure = g_new0 (source_load_closure, 1);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	closure->source = g_object_ref (self);
	g_simple_async_result_set_op_res_gpointer (res, closure, source_load_free);

	closure->request = gnome_keyring_list_keyring_names (on_source_load_list_keyring_names_complete,
	                                                     g_object_ref (res), g_object_unref);

	if (cancellable)
		closure->cancelled_sig = g_cancellable_connect (cancellable,
		                                                G_CALLBACK (on_source_load_cancelled),
		                                                closure, NULL);

	g_object_unref (res);
}

static gboolean
seahorse_gkr_source_load_finish (SeahorseSource *source,
                                 GAsyncResult *result,
                                 GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_gkr_source_load_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

static void 
seahorse_source_iface (SeahorseSourceIface *iface)
{
	iface->load_async = seahorse_gkr_source_load_async;
	iface->load_finish = seahorse_gkr_source_load_finish;
}

/* -------------------------------------------------------------------------- 
 * PUBLIC
 */

SeahorseGkrSource*
seahorse_gkr_source_new (void)
{
   return g_object_new (SEAHORSE_TYPE_GKR_SOURCE, NULL);
}   

SeahorseGkrSource*
seahorse_gkr_source_default (void)
{
	g_return_val_if_fail (default_source, NULL);
	return default_source;
}
