/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
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
 */

#include "config.h"

#include "seahorse-gkr.h"
#include "seahorse-gkr-backend.h"
#include "seahorse-gkr-dialogs.h"
#include "seahorse-gkr-keyring-deleter.h"
#include "seahorse-gkr-keyring.h"
#include "seahorse-gkr-actions.h"

#include "seahorse-action.h"
#include "seahorse-common.h"
#include "seahorse-progress.h"
#include "seahorse-util.h"

#include <gcr/gcr.h>

#include <glib/gi18n.h>

enum {
	PROP_0,
	PROP_DESCRIPTION,
	PROP_KEYRING_INFO,
	PROP_IS_DEFAULT,
	PROP_URI,
	PROP_ACTIONS,
	PROP_LOCKABLE,
	PROP_UNLOCKABLE,

	PROP_ICON,
	PROP_DELETABLE
};

struct _SeahorseGkrKeyringPrivate {
	GHashTable *items;
};

static void     seahorse_gkr_keyring_place_iface    (SeahorsePlaceIface *iface);

static void     seahorse_keyring_collection_iface   (GcrCollectionIface *iface);

static void     seahorse_keyring_deletable_iface    (SeahorseDeletableIface *iface);

static void     seahorse_keyring_lockable_iface     (SeahorseLockableIface *iface);

static void     seahorse_keyring_viewable_iface     (SeahorseViewableIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseGkrKeyring, seahorse_gkr_keyring, SECRET_TYPE_COLLECTION,
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_COLLECTION, seahorse_keyring_collection_iface);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_PLACE, seahorse_gkr_keyring_place_iface);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_DELETABLE, seahorse_keyring_deletable_iface);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_LOCKABLE, seahorse_keyring_lockable_iface);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_VIEWABLE, seahorse_keyring_viewable_iface);
);

static void
refresh_collection (SeahorseGkrKeyring *self)
{
	SecretCollection *collection = SECRET_COLLECTION (self);
	SeahorseGkrItem *item;
	const gchar *object_path;
	GHashTableIter iter;
	GHashTable *seen;
	GList *items = NULL;
	GList *l;

	seen = g_hash_table_new (g_str_hash, g_str_equal);

	if (!secret_collection_get_locked (collection))
		items = secret_collection_get_items (collection);

	for (l = items; l != NULL; l = g_list_next (l)) {
		object_path = g_dbus_proxy_get_object_path (l->data);
		item = l->data;

		g_hash_table_add (seen, (gpointer)object_path);
		if (!g_hash_table_lookup (self->pv->items, object_path)) {
			g_object_set (item, "place", self, NULL);
			g_hash_table_insert (self->pv->items,
			                     g_strdup (object_path),
			                     g_object_ref (item));
			gcr_collection_emit_added (GCR_COLLECTION (self), G_OBJECT (item));
		}
	}

	/* Remove any that we didn't find */
	g_hash_table_iter_init (&iter, self->pv->items);
	while (g_hash_table_iter_next (&iter, (gpointer *)&object_path, NULL)) {
		if (!g_hash_table_lookup (seen, object_path)) {
			item = g_hash_table_lookup (self->pv->items, object_path);
			g_object_ref (item);
			g_object_set (item, "place", NULL, NULL);
			g_hash_table_iter_remove (&iter);
			gcr_collection_emit_removed (GCR_COLLECTION (self), G_OBJECT (item));
			g_object_unref (item);
		}
	}

	g_hash_table_destroy (seen);
	g_list_free_full (items, g_object_unref);
}

static void
seahorse_gkr_keyring_load_async (SeahorsePlace *place,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (place);
	GSimpleAsyncResult *async;

	async = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                   seahorse_gkr_keyring_load_async);

	refresh_collection (self);

	g_simple_async_result_complete_in_idle (async);
	g_object_unref (async);
}

static gboolean
seahorse_gkr_keyring_load_finish (SeahorsePlace *place,
                                  GAsyncResult *result,
                                  GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (place),
	                      seahorse_gkr_keyring_load_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

static void
on_aliases_changed (GObject *obj,
                    GParamSpec *pspec,
                    gpointer user_data)
{
	g_object_notify (user_data, "is-default");
	g_object_notify (user_data, "description");
}

static void
on_notify_items (GObject *obj,
                 GParamSpec *pspec,
                 gpointer user_data)
{
	refresh_collection (SEAHORSE_GKR_KEYRING (obj));
}

static void
on_notify_locked (GObject *obj,
                  GParamSpec *pspec,
                  gpointer user_data)
{
	refresh_collection (SEAHORSE_GKR_KEYRING (obj));
}

static void
seahorse_gkr_keyring_init (SeahorseGkrKeyring *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_GKR_KEYRING, SeahorseGkrKeyringPrivate);
	self->pv->items = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	g_signal_connect (self, "notify::items", G_CALLBACK (on_notify_items), NULL);
	g_signal_connect (self, "notify::locked", G_CALLBACK (on_notify_locked), NULL);
	g_signal_connect (seahorse_gkr_backend_get (), "notify::aliases", G_CALLBACK (on_aliases_changed), self);
}

static void
seahorse_gkr_keyring_finalize (GObject *obj)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (obj);

	g_hash_table_destroy (self->pv->items);
	g_signal_handlers_disconnect_by_func (self, on_notify_items, NULL);
	g_signal_handlers_disconnect_by_func (self, on_notify_locked, NULL);
	g_signal_handlers_disconnect_by_func (seahorse_gkr_backend_get (), on_aliases_changed, self);

	G_OBJECT_CLASS (seahorse_gkr_keyring_parent_class)->finalize (obj);
}

static gchar *
seahorse_gkr_keyring_get_description (SeahorsePlace *place)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_KEYRING (place), NULL);
	if (seahorse_gkr_backend_has_alias (NULL, "login", SEAHORSE_GKR_KEYRING (place)))
		return g_strdup (_("A keyring that is automatically unlocked on login"));
	return g_strdup (_("A keyring used to store passwords"));
}

static GtkActionGroup *
seahorse_gkr_keyring_get_actions (SeahorsePlace *place)
{
	return seahorse_gkr_keyring_actions_new (SEAHORSE_GKR_KEYRING (place));
}

static gchar *
seahorse_gkr_keyring_get_uri (SeahorsePlace *place)
{
	const gchar *object_path;
	object_path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (place));
	return g_strdup_printf ("secret-service://%s", object_path);
}

static GIcon *
seahorse_gkr_keyring_get_icon (SeahorsePlace *place)
{
	return g_themed_icon_new ("folder");
}

static gchar *
seahorse_gkr_keyring_get_label (SeahorsePlace *place)
{
	return secret_collection_get_label (SECRET_COLLECTION (place));
}

static gboolean
seahorse_gkr_keyring_get_lockable (SeahorseLockable *lockable)
{
	return !secret_collection_get_locked (SECRET_COLLECTION (lockable));
}

static gboolean
seahorse_gkr_keyring_get_unlockable (SeahorseLockable *lockable)
{
	return secret_collection_get_locked (SECRET_COLLECTION (lockable));
}

static void
seahorse_gkr_keyring_get_property (GObject *obj, guint prop_id, GValue *value, 
                           GParamSpec *pspec)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (obj);
	SeahorsePlace *place = SEAHORSE_PLACE (obj);
	SeahorseLockable *lockable = SEAHORSE_LOCKABLE (obj);

	switch (prop_id) {
	case PROP_DESCRIPTION:
		g_value_take_string (value, seahorse_gkr_keyring_get_description (place));
		break;
	case PROP_IS_DEFAULT:
		g_value_set_boolean (value, seahorse_gkr_keyring_get_is_default (self));
		break;
	case PROP_URI:
		g_value_take_string (value, seahorse_gkr_keyring_get_uri (place));
		break;
	case PROP_ACTIONS:
		g_value_take_object (value, seahorse_gkr_keyring_get_actions (place));
		break;
	case PROP_LOCKABLE:
		g_value_set_boolean (value, seahorse_gkr_keyring_get_lockable (lockable));
		break;
	case PROP_UNLOCKABLE:
		g_value_set_boolean (value, seahorse_gkr_keyring_get_unlockable (lockable));
		break;
	case PROP_DELETABLE:
		g_value_set_boolean (value, TRUE);
		break;
	case PROP_ICON:
		g_value_take_object (value, seahorse_gkr_keyring_get_icon (place));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_gkr_keyring_class_init (SeahorseGkrKeyringClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (SeahorseGkrKeyringPrivate));

	gobject_class->finalize = seahorse_gkr_keyring_finalize;
	gobject_class->get_property = seahorse_gkr_keyring_get_property;

	g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");
	g_object_class_override_property (gobject_class, PROP_URI, "uri");
	g_object_class_override_property (gobject_class, PROP_ACTIONS, "actions");
	g_object_class_override_property (gobject_class, PROP_ICON, "icon");

	g_object_class_install_property (gobject_class, PROP_IS_DEFAULT,
	           g_param_spec_boolean ("is-default", "Is default", "Is the default keyring.",
	                                 FALSE, G_PARAM_READABLE));

	g_object_class_override_property (gobject_class, PROP_LOCKABLE, "lockable");
	g_object_class_override_property (gobject_class, PROP_UNLOCKABLE, "unlockable");
	g_object_class_override_property (gobject_class, PROP_DELETABLE, "deletable");
}

static void
seahorse_gkr_keyring_place_iface (SeahorsePlaceIface *iface)
{
	iface->load = seahorse_gkr_keyring_load_async;
	iface->load_finish = seahorse_gkr_keyring_load_finish;
	iface->get_actions = seahorse_gkr_keyring_get_actions;
	iface->get_description = seahorse_gkr_keyring_get_description;
	iface->get_icon = seahorse_gkr_keyring_get_icon;
	iface->get_label = seahorse_gkr_keyring_get_label;
	iface->get_uri = seahorse_gkr_keyring_get_uri;
}

static guint
seahorse_gkr_keyring_get_length (GcrCollection *collection)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (collection);
	return g_hash_table_size (self->pv->items);
}

static GList *
seahorse_gkr_keyring_get_objects (GcrCollection *collection)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (collection);
	return g_hash_table_get_values (self->pv->items);
}

static gboolean
seahorse_gkr_keyring_contains (GcrCollection *collection,
                               GObject *object)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (collection);

	if (!SEAHORSE_IS_GKR_ITEM (object))
		return FALSE;
	return g_hash_table_lookup (self->pv->items, g_dbus_proxy_get_object_path (G_DBUS_PROXY (object))) != NULL;
}

static void
seahorse_keyring_collection_iface (GcrCollectionIface *iface)
{
	iface->get_length = seahorse_gkr_keyring_get_length;
	iface->get_objects = seahorse_gkr_keyring_get_objects;
	iface->contains = seahorse_gkr_keyring_contains;
}

static SeahorseDeleter *
seahorse_gkr_keyring_create_deleter (SeahorseDeletable *deletable)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (deletable);
	return seahorse_gkr_keyring_deleter_new (self);
}

static gboolean
seahorse_gkr_keyring_get_deletable (SeahorseDeletable *deletable)
{
	gboolean can;
	g_object_get (deletable, "deletable", &can, NULL);
	return can;
}


static void
seahorse_keyring_deletable_iface (SeahorseDeletableIface *iface)
{
	iface->create_deleter = seahorse_gkr_keyring_create_deleter;
	iface->get_deletable = seahorse_gkr_keyring_get_deletable;
}

static void
on_keyring_lock_done (GObject *source,
                      GAsyncResult *result,
                      gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (g_async_result_get_source_object (user_data));
	GError *error = NULL;

	secret_service_lock_finish (SECRET_SERVICE (source), result, NULL, &error);
	if (error != NULL)
		g_simple_async_result_take_error (res, error);

	refresh_collection (self);
	g_simple_async_result_complete (res);

	g_object_unref (self);
	g_object_unref (res);
}

static void
seahorse_gkr_keyring_lock_async (SeahorseLockable *lockable,
                                 GTlsInteraction *interaction,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
	SecretCollection *self = SECRET_COLLECTION (lockable);
	GSimpleAsyncResult *res;
	GList *objects;

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_gkr_keyring_lock_async);

	objects = g_list_prepend (NULL, self);
	secret_service_lock (secret_collection_get_service (self),
	                     objects, cancellable, on_keyring_lock_done,
	                     g_object_ref (res));
	g_list_free (objects);

	g_object_unref (res);
}

static gboolean
seahorse_gkr_keyring_lock_finish (SeahorseLockable *lockable,
                                  GAsyncResult *result,
                                  GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (lockable),
	                      seahorse_gkr_keyring_lock_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

static void
on_keyring_unlock_done (GObject *source,
                        GAsyncResult *result,
                        gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (g_async_result_get_source_object (user_data));
	GError *error = NULL;

	secret_service_unlock_finish (SECRET_SERVICE (source), result, NULL, &error);
	if (error != NULL)
		g_simple_async_result_take_error (res, error);

	refresh_collection (self);
	g_simple_async_result_complete (res);

	g_object_unref (self);
	g_object_unref (res);
}

static void
seahorse_gkr_keyring_unlock_async (SeahorseLockable *lockable,
                                   GTlsInteraction *interaction,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
	SecretCollection *self = SECRET_COLLECTION (lockable);
	GSimpleAsyncResult *res;
	GList *objects;

	res = g_simple_async_result_new (G_OBJECT (lockable), callback, user_data,
	                                 seahorse_gkr_keyring_unlock_async);

	objects = g_list_prepend (NULL, self);
	secret_service_unlock (secret_collection_get_service (self),
	                       objects, cancellable, on_keyring_unlock_done,
	                       g_object_ref (res));
	g_list_free (objects);

	g_object_unref (res);
}

static gboolean
seahorse_gkr_keyring_unlock_finish (SeahorseLockable *lockable,
                                    GAsyncResult *result,
                                    GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (lockable),
	                      seahorse_gkr_keyring_unlock_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

static void
seahorse_keyring_lockable_iface (SeahorseLockableIface *iface)
{
	iface->get_lockable = seahorse_gkr_keyring_get_lockable;
	iface->get_unlockable = seahorse_gkr_keyring_get_unlockable;
	iface->lock = seahorse_gkr_keyring_lock_async;
	iface->lock_finish = seahorse_gkr_keyring_lock_finish;
	iface->unlock = seahorse_gkr_keyring_unlock_async;
	iface->unlock_finish = seahorse_gkr_keyring_unlock_finish;
}

static void
seahorse_gkr_keyring_show_viewer (SeahorseViewable *viewable,
                                  GtkWindow *parent)
{
	seahorse_gkr_keyring_properties_show (SEAHORSE_GKR_KEYRING (viewable),
	                                      parent);
}

static void
seahorse_keyring_viewable_iface (SeahorseViewableIface *iface)
{
	iface->show_viewer = seahorse_gkr_keyring_show_viewer;
}

gboolean
seahorse_gkr_keyring_get_is_default (SeahorseGkrKeyring *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_KEYRING (self), FALSE);
	return seahorse_gkr_backend_has_alias (NULL, "default", self);
}
