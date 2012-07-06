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

#include "seahorse-gkr-actions.h"
#include "seahorse-gkr-backend.h"
#include "seahorse-gkr-dialogs.h"

#include "seahorse-backend.h"
#include "seahorse-progress.h"

#include <glib/gi18n.h>


typedef SecretService MySecretService;
typedef SecretServiceClass MySecretServiceClass;

GType   my_secret_service_get_type (void);

G_DEFINE_TYPE (MySecretService, my_secret_service, SECRET_TYPE_SERVICE);

static void
my_secret_service_init (MySecretService *self)
{

}

static void
my_secret_service_class_init (MySecretServiceClass *klass)
{
	klass->collection_gtype = SEAHORSE_TYPE_GKR_KEYRING;
	klass->item_gtype = SEAHORSE_TYPE_GKR_ITEM;
}

enum {
	PROP_0,
	PROP_NAME,
	PROP_LABEL,
	PROP_DESCRIPTION,
	PROP_ACTIONS,
	PROP_ALIASES,
	PROP_SERVICE
};

static SeahorseGkrBackend *gkr_backend = NULL;

struct _SeahorseGkrBackend {
	GObject parent;
	SecretService *service;
	GHashTable *keyrings;
	GHashTable *aliases;
	GtkActionGroup *actions;
};

struct _SeahorseGkrBackendClass {
	GObjectClass parent_class;
};

static void         seahorse_gkr_backend_iface            (SeahorseBackendIface *iface);

static void         seahorse_gkr_backend_collection_init  (GcrCollectionIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseGkrBackend, seahorse_gkr_backend, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_COLLECTION, seahorse_gkr_backend_collection_init);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_BACKEND, seahorse_gkr_backend_iface);
);

static void
seahorse_gkr_backend_init (SeahorseGkrBackend *self)
{
	g_return_if_fail (gkr_backend == NULL);
	gkr_backend = self;

	self->actions = seahorse_gkr_backend_actions_instance (self);
	self->keyrings = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                        g_free, g_object_unref);
	self->aliases = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                       g_free, g_free);
}

static void
refresh_collection (SeahorseGkrBackend *self)
{
	GHashTable *seen = NULL;
	GHashTableIter iter;
	const gchar *object_path;
	SeahorseGkrKeyring *keyring;
	GList *keyrings;
	GList *l;

	seen = g_hash_table_new (g_str_hash, g_str_equal);

	keyrings = secret_service_get_collections (self->service);

	/* Add any keyrings we don't have */
	for (l = keyrings; l != NULL; l = g_list_next (l)) {
		object_path = g_dbus_proxy_get_object_path (l->data);

		/* Don't list the session keyring */
		if (g_strcmp0 (g_hash_table_lookup (self->aliases, "session"), object_path) == 0)
			continue;

		keyring = l->data;
		g_hash_table_add (seen, (gpointer)object_path);

		if (!g_hash_table_lookup (self->keyrings, object_path)) {
			g_hash_table_insert (self->keyrings,
			                     g_strdup (object_path),
			                     g_object_ref (keyring));
			gcr_collection_emit_added (GCR_COLLECTION (self), G_OBJECT (keyring));
		}
	}

	/* Remove any that we didn't find */
	g_hash_table_iter_init (&iter, self->keyrings);
	while (g_hash_table_iter_next (&iter, (gpointer *)&object_path, NULL)) {
		if (!g_hash_table_lookup (seen, object_path)) {
			keyring = g_hash_table_lookup (self->keyrings, object_path);
			g_object_ref (keyring);
			g_hash_table_iter_remove (&iter);
			gcr_collection_emit_removed (GCR_COLLECTION (self), G_OBJECT (keyring));
			g_object_unref (keyring);
		}
	}

	g_hash_table_destroy (seen);
	g_list_free_full (keyrings, g_object_unref);
}

static void
on_notify_collections (GObject *obj,
                       GParamSpec *pspec,
                       gpointer user_data)
{
	refresh_collection (SEAHORSE_GKR_BACKEND (user_data));
}

static void
on_service_new (GObject *source,
                GAsyncResult *result,
                gpointer user_data)
{
	SeahorseGkrBackend *self = SEAHORSE_GKR_BACKEND (user_data);
	GError *error = NULL;

	self->service = secret_service_new_finish (result, &error);
	if (error == NULL) {
		g_signal_connect (self->service, "notify::collections", G_CALLBACK (on_notify_collections), self);
		refresh_collection (self);

		secret_service_load_collections (self->service, NULL, NULL, NULL);
		seahorse_gkr_backend_load_async (self, NULL, NULL, NULL);
	} else {
		g_warning ("couldn't connect to secret service: %s", error->message);
		g_error_free (error);
	}

	g_object_notify (G_OBJECT (self), "service");
	g_object_unref (self);
}

static void
seahorse_gkr_backend_constructed (GObject *obj)
{
	SeahorseGkrBackend *self = SEAHORSE_GKR_BACKEND (obj);

	G_OBJECT_CLASS (seahorse_gkr_backend_parent_class)->constructed (obj);

	secret_service_new (my_secret_service_get_type (), NULL, SECRET_SERVICE_OPEN_SESSION,
	                    NULL, on_service_new, g_object_ref (self));
}

static void
seahorse_gkr_backend_get_property (GObject *obj,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	SeahorseGkrBackend *self = SEAHORSE_GKR_BACKEND (obj);

	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, SEAHORSE_GKR_NAME);
		break;
	case PROP_LABEL:
		g_value_set_string (value, _("Passwords"));
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, _("Stored personal passwords, credentials and secrets"));
		break;
	case PROP_ACTIONS:
		g_value_set_object (value, self->actions);
		break;
	case PROP_ALIASES:
		g_value_set_boxed (value, self->aliases);
		break;
	case PROP_SERVICE:
		g_value_set_object (value, self->service);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_gkr_backend_dispose (GObject *obj)
{
	SeahorseGkrBackend *self = SEAHORSE_GKR_BACKEND (obj);

	if (self->service) {
		g_signal_handlers_disconnect_by_func (self->service, on_notify_collections, self);
		g_object_unref (self->service);
		self->service = NULL;
	}

	g_hash_table_remove_all (self->keyrings);
	g_hash_table_remove_all (self->aliases);
	gtk_action_group_set_sensitive (self->actions, FALSE);

	G_OBJECT_CLASS (seahorse_gkr_backend_parent_class)->finalize (obj);
}

static void
seahorse_gkr_backend_finalize (GObject *obj)
{
	SeahorseGkrBackend *self = SEAHORSE_GKR_BACKEND (obj);

	g_assert (self->service == NULL);

	g_hash_table_destroy (self->keyrings);
	g_hash_table_destroy (self->aliases);
	g_clear_object (&self->actions);

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
	gobject_class->get_property = seahorse_gkr_backend_get_property;

	g_object_class_override_property (gobject_class, PROP_NAME, "name");
	g_object_class_override_property (gobject_class, PROP_LABEL, "label");
	g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");
	g_object_class_override_property (gobject_class, PROP_ACTIONS, "actions");

	g_object_class_install_property (gobject_class, PROP_ALIASES,
	             g_param_spec_boxed ("aliases", "aliases", "Aliases",
	                                 G_TYPE_HASH_TABLE, G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_SERVICE,
	            g_param_spec_object ("service", "Service", "Service",
	                                 SECRET_TYPE_SERVICE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
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
	gboolean have;
	gchar *uri;

	if (!SEAHORSE_IS_GKR_KEYRING (object))
		return FALSE;

	g_object_get (object, "uri", &uri, NULL);
	have = (g_hash_table_lookup (self->keyrings, uri) == object);
	g_free (uri);

	return have;
}

static void
seahorse_gkr_backend_collection_init (GcrCollectionIface *iface)
{
	iface->contains = seahorse_gkr_backend_contains;
	iface->get_length = seahorse_gkr_backend_get_length;
	iface->get_objects = seahorse_gkr_backend_get_objects;
}

static SeahorsePlace *
seahorse_gkr_backend_lookup_place (SeahorseBackend *backend,
                                   const gchar *uri)
{
	SeahorseGkrBackend *self = SEAHORSE_GKR_BACKEND (backend);
	return g_hash_table_lookup (self->keyrings, uri);
}

static void
seahorse_gkr_backend_iface (SeahorseBackendIface *iface)
{
	iface->lookup_place = seahorse_gkr_backend_lookup_place;
}

void
seahorse_gkr_backend_initialize (void)
{
	SeahorseGkrBackend *self;

	g_return_if_fail (gkr_backend == NULL);
	self = g_object_new (SEAHORSE_TYPE_GKR_BACKEND, NULL);

	seahorse_backend_register (SEAHORSE_BACKEND (self));
	g_object_unref (self);

	g_return_if_fail (gkr_backend != NULL);
}

SeahorseGkrBackend *
seahorse_gkr_backend_get (void)
{
	g_return_val_if_fail (gkr_backend, NULL);
	return gkr_backend;
}

SecretService *
seahorse_gkr_backend_get_service (SeahorseGkrBackend *self)
{
	self = self ? self : seahorse_gkr_backend_get ();
	g_return_val_if_fail (SEAHORSE_IS_GKR_BACKEND (self), NULL);
	return self->service;
}

GList *
seahorse_gkr_backend_get_keyrings (SeahorseGkrBackend *self)
{
	self = self ? self : seahorse_gkr_backend_get ();
	g_return_val_if_fail (SEAHORSE_IS_GKR_BACKEND (self), NULL);
	return g_hash_table_get_values (self->keyrings);
}

typedef struct {
	gint outstanding;
} LoadClosure;

static void
on_load_read_alias (GObject *source,
                    GAsyncResult *result,
                    const gchar *alias,
                    gpointer user_data)
{
	GSimpleAsyncResult *async = G_SIMPLE_ASYNC_RESULT (user_data);
	LoadClosure *load = g_simple_async_result_get_op_res_gpointer (async);
	SeahorseGkrBackend *self = SEAHORSE_GKR_BACKEND (g_async_result_get_source_object (user_data));
	GError *error = NULL;
	gchar *path;

	path = secret_service_read_alias_dbus_path_finish (SECRET_SERVICE (source), result, &error);
	if (path != NULL) {
		g_hash_table_replace (self->aliases, g_strdup (alias), path);
		g_object_notify (G_OBJECT (self), "aliases");
	}

	if (error != NULL)
		g_simple_async_result_take_error (async, error);

	load->outstanding--;
	if (load->outstanding <= 0) {
		refresh_collection (self);
		g_simple_async_result_complete (async);
	}

	g_object_unref (self);
	g_object_unref (async);

}
static void
on_load_read_default_alias (GObject *source,
                            GAsyncResult *result,
                            gpointer user_data)
{
	on_load_read_alias (source, result, "default", user_data);
}

static void
on_load_read_session_alias (GObject *source,
                            GAsyncResult *result,
                            gpointer user_data)
{
	on_load_read_alias (source, result, "session", user_data);
}

static void
on_load_read_login_alias (GObject *source,
                          GAsyncResult *result,
                          gpointer user_data)
{
	on_load_read_alias (source, result, "login", user_data);
}

void
seahorse_gkr_backend_load_async (SeahorseGkrBackend *self,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
	LoadClosure *load;
	GSimpleAsyncResult *async;

	self = self ? self : seahorse_gkr_backend_get ();

	async = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                   seahorse_gkr_backend_load_async);
	load = g_new0 (LoadClosure, 1);
	load->outstanding = 0;
	g_simple_async_result_set_op_res_gpointer (async, load, g_free);

	if (!self->service) {
		g_simple_async_result_complete_in_idle (async);
		g_object_unref (async);
		return;
	}

	secret_service_read_alias_dbus_path (self->service, "default", cancellable,
	                                     on_load_read_default_alias, g_object_ref (async));
	load->outstanding++;

	secret_service_read_alias_dbus_path (self->service, "session", cancellable,
	                                     on_load_read_session_alias, g_object_ref (async));
	load->outstanding++;

	secret_service_read_alias_dbus_path (self->service, "login", cancellable,
	                                     on_load_read_login_alias, g_object_ref (async));
	load->outstanding++;

	g_object_unref (async);
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

gboolean
seahorse_gkr_backend_has_alias (SeahorseGkrBackend *self,
                                const gchar *alias,
                                SeahorseGkrKeyring *keyring)
{
	const gchar *object_path;

	self = self ? self : seahorse_gkr_backend_get ();
	object_path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (keyring));
	return g_strcmp0 (g_hash_table_lookup (self->aliases, alias), object_path) == 0;
}
