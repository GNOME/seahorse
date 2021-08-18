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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "seahorse-gpgme-dialogs.h"
#include "seahorse-pgp-actions.h"
#include "seahorse-pgp-backend.h"
#include "seahorse-server-source.h"
#include "seahorse-transfer.h"
#include "seahorse-unknown-source.h"

#include "seahorse-common.h"

#include "libseahorse/seahorse-progress.h"
#include "libseahorse/seahorse-util.h"

#include <glib/gi18n.h>

#include <string.h>

static SeahorsePgpBackend *pgp_backend = NULL;

struct _SeahorsePgpBackend {
    GObject parent;

    char *gpg_homedir;

    SeahorseGpgmeKeyring *keyring;
    SeahorsePgpSettings *pgp_settings;
    SeahorseDiscovery *discovery;
    SeahorseUnknownSource *unknown;
    GListModel *remotes;
    SeahorseActionGroup *actions;
    gboolean loaded;
};

enum {
    PROP_0,
    PROP_GPG_HOMEDIR,
    N_PROPS,

    /* overridden properties */
    PROP_NAME,
    PROP_LABEL,
    PROP_DESCRIPTION,
    PROP_ACTIONS,
    PROP_LOADED
};
static GParamSpec *obj_props[N_PROPS] = { NULL, };

static void         seahorse_pgp_backend_iface            (SeahorseBackendIface *iface);

static void         seahorse_pgp_backend_collection_init  (GcrCollectionIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorsePgpBackend, seahorse_pgp_backend, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_COLLECTION, seahorse_pgp_backend_collection_init);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_BACKEND, seahorse_pgp_backend_iface);
);

static void
seahorse_pgp_backend_init (SeahorsePgpBackend *self)
{
    g_return_if_fail (pgp_backend == NULL);
    pgp_backend = self;

    self->pgp_settings = seahorse_pgp_settings_instance ();
    self->remotes = G_LIST_MODEL (g_list_store_new (SEAHORSE_TYPE_SERVER_SOURCE));
    self->actions = seahorse_pgp_backend_actions_instance ();
}

#ifdef WITH_KEYSERVER

static void
on_settings_keyservers_changed (GSettings  *settings,
                                const char *key,
                                gpointer    user_data)
{
    SeahorsePgpBackend *self = SEAHORSE_PGP_BACKEND (user_data);
    SeahorsePgpSettings *pgp_settings = SEAHORSE_PGP_SETTINGS (settings);
    g_auto(GStrv) keyservers = NULL;
    g_autoptr(GPtrArray) check = NULL;

    check = g_ptr_array_new_with_free_func (g_free);
    for (guint i = 0; i < g_list_model_get_n_items (self->remotes); i++) {
        g_autoptr(SeahorseServerSource) remote = NULL;
        g_autofree char *uri = NULL;

        remote = g_list_model_get_item (self->remotes, i);
        uri = seahorse_place_get_uri (SEAHORSE_PLACE (remote));
        g_ptr_array_add (check, g_steal_pointer (&uri));
    }

    /* Load and strip names from keyserver list */
    keyservers = seahorse_pgp_settings_get_uris (pgp_settings);

    for (guint i = 0; keyservers[i] != NULL; i++) {
        const char *uri = keyservers[i];
        gboolean found;
        guint index;

        found = g_ptr_array_find_with_equal_func (check, uri, g_str_equal, &index);
        if (found) {
            /* Mark this one as present */
            g_ptr_array_remove_index (check, index);
        } else {
            /* If we don't have a keysource then add it */
            seahorse_pgp_backend_add_remote (self, uri, FALSE);
        }
    }

    /* Now remove any extras */
    for (guint i = 0; i < check->len; i++) {
        const char *uri = g_ptr_array_index (check, i);
        seahorse_pgp_backend_remove_remote (self, uri);
    }
}

#endif /* WITH_KEYSERVER */

static void
on_place_loaded (GObject       *object,
                 GAsyncResult  *result,
                 gpointer       user_data)
{
	SeahorsePgpBackend *backend = user_data;
	gboolean ok;
	GError *error;

	error = NULL;
	ok = seahorse_place_load_finish (SEAHORSE_PLACE (object), result, &error);

	if (!ok) {
		g_warning ("Failed to initialize PGP backend: %s", error->message);
		g_error_free (error);
	}

	backend->loaded = TRUE;
	g_object_notify (G_OBJECT (backend), "loaded");

	g_object_unref (backend);
}

static void
seahorse_pgp_backend_constructed (GObject *obj)
{
    SeahorsePgpBackend *self = SEAHORSE_PGP_BACKEND (obj);

    G_OBJECT_CLASS (seahorse_pgp_backend_parent_class)->constructed (obj);

    /* First of all: configure the GPG home directory */
    gpgme_set_engine_info (GPGME_PROTOCOL_OpenPGP, NULL, self->gpg_homedir);

    self->keyring = seahorse_gpgme_keyring_new ();
    seahorse_place_load (SEAHORSE_PLACE (self->keyring), NULL, on_place_loaded,
                         g_object_ref (self));

    self->discovery = seahorse_discovery_new ();
    self->unknown = seahorse_unknown_source_new ();

#ifdef WITH_KEYSERVER
    g_signal_connect (self->pgp_settings, "changed::keyservers",
                      G_CALLBACK (on_settings_keyservers_changed), self);

    /* Initial loading */
    on_settings_keyservers_changed (G_SETTINGS (self->pgp_settings),
                                    "keyservers",
                                    self);
#endif
}

static const gchar *
seahorse_pgp_backend_get_name (SeahorseBackend *backend)
{
	return SEAHORSE_PGP_NAME;
}

static const gchar *
seahorse_pgp_backend_get_label (SeahorseBackend *backend)
{
	return _("PGP Keys");
}

static const gchar *
seahorse_pgp_backend_get_description (SeahorseBackend *backend)
{
	return _("PGP keys are for encrypting email or files");
}

static SeahorseActionGroup *
seahorse_pgp_backend_get_actions (SeahorseBackend *backend)
{
	SeahorsePgpBackend *self = SEAHORSE_PGP_BACKEND (backend);
	return g_object_ref (self->actions);
}

static gboolean
seahorse_pgp_backend_get_loaded (SeahorseBackend *backend)
{
	g_return_val_if_fail (SEAHORSE_PGP_IS_BACKEND (backend), FALSE);

	return SEAHORSE_PGP_BACKEND (backend)->loaded;
}

const char *
seahorse_pgp_backend_get_gpg_homedir (SeahorsePgpBackend *self)
{
    g_return_val_if_fail (SEAHORSE_PGP_IS_BACKEND (self), FALSE);

    return self->gpg_homedir;
}

static void
seahorse_pgp_backend_get_property (GObject *obj,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
    SeahorsePgpBackend *self = SEAHORSE_PGP_BACKEND (obj);
    SeahorseBackend *backend = SEAHORSE_BACKEND (obj);

    switch (prop_id) {
    case PROP_GPG_HOMEDIR:
        g_value_set_string (value,  seahorse_pgp_backend_get_gpg_homedir (self));
        break;
    case PROP_NAME:
        g_value_set_string (value,  seahorse_pgp_backend_get_name (backend));
        break;
    case PROP_LABEL:
        g_value_set_string (value,  seahorse_pgp_backend_get_label (backend));
        break;
    case PROP_DESCRIPTION:
        g_value_set_string (value,  seahorse_pgp_backend_get_description (backend));
        break;
    case PROP_ACTIONS:
        g_value_take_object (value, seahorse_pgp_backend_get_actions (backend));
        break;
    case PROP_LOADED:
        g_value_set_boolean (value, seahorse_pgp_backend_get_loaded (backend));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
        break;
    }
}

static void
seahorse_pgp_backend_set_property (GObject      *obj,
                                   unsigned int  prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
    SeahorsePgpBackend *self = SEAHORSE_PGP_BACKEND (obj);

    switch (prop_id) {
    case PROP_GPG_HOMEDIR:
        g_free (self->gpg_homedir);
        self->gpg_homedir = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
        break;
    }
}

static void
seahorse_pgp_backend_finalize (GObject *obj)
{
    SeahorsePgpBackend *self = SEAHORSE_PGP_BACKEND (obj);

#ifdef WITH_KEYSERVER
    g_signal_handlers_disconnect_by_func (self->pgp_settings,
                                          on_settings_keyservers_changed, self);
#endif

    g_clear_pointer (&self->gpg_homedir, g_free);
    g_clear_object (&self->keyring);
    g_clear_object (&self->discovery);
    g_clear_object (&self->unknown);
    g_clear_object (&self->remotes);
    g_clear_object (&self->actions);
    pgp_backend = NULL;

    G_OBJECT_CLASS (seahorse_pgp_backend_parent_class)->finalize (obj);
}

static void
seahorse_pgp_backend_class_init (SeahorsePgpBackendClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->constructed = seahorse_pgp_backend_constructed;
    gobject_class->finalize = seahorse_pgp_backend_finalize;
    gobject_class->get_property = seahorse_pgp_backend_get_property;
    gobject_class->set_property = seahorse_pgp_backend_set_property;

    obj_props[PROP_GPG_HOMEDIR] =
        g_param_spec_string ("gpg-homedir", "gpg-homedir", "GPG home directory",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class, N_PROPS, obj_props);

    /* Overridden properties */
    g_object_class_override_property (gobject_class, PROP_NAME, "name");
    g_object_class_override_property (gobject_class, PROP_LABEL, "label");
    g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");
    g_object_class_override_property (gobject_class, PROP_ACTIONS, "actions");
    g_object_class_override_property (gobject_class, PROP_LOADED, "loaded");
}

static guint
seahorse_pgp_backend_get_length (GcrCollection *collection)
{
	return 1;
}

static GList *
seahorse_pgp_backend_get_objects (GcrCollection *collection)
{
	SeahorsePgpBackend *self = SEAHORSE_PGP_BACKEND (collection);
	return g_list_append (NULL, self->keyring);
}

static gboolean
seahorse_pgp_backend_contains (GcrCollection *collection,
                               GObject *object)
{
	SeahorsePgpBackend *self = SEAHORSE_PGP_BACKEND (collection);
	return G_OBJECT (self->keyring) == object;
}

static void
seahorse_pgp_backend_collection_init (GcrCollectionIface *iface)
{
	iface->contains = seahorse_pgp_backend_contains;
	iface->get_length = seahorse_pgp_backend_get_length;
	iface->get_objects = seahorse_pgp_backend_get_objects;
}

static SeahorsePlace *
seahorse_pgp_backend_lookup_place (SeahorseBackend *backend,
                                   const gchar *uri)
{
	SeahorsePgpBackend *self = SEAHORSE_PGP_BACKEND (backend);
	if (g_str_equal (uri, "gnupg://"))
		return SEAHORSE_PLACE (seahorse_pgp_backend_get_default_keyring (self));
	return NULL;
}

static void
seahorse_pgp_backend_iface (SeahorseBackendIface *iface)
{
	iface->lookup_place = seahorse_pgp_backend_lookup_place;
	iface->get_actions = seahorse_pgp_backend_get_actions;
	iface->get_description = seahorse_pgp_backend_get_description;
	iface->get_label = seahorse_pgp_backend_get_label;
	iface->get_name = seahorse_pgp_backend_get_name;
	iface->get_loaded = seahorse_pgp_backend_get_loaded;
}

SeahorsePgpBackend *
seahorse_pgp_backend_get (void)
{
	g_return_val_if_fail (pgp_backend, NULL);
	return pgp_backend;
}

/**
 * seahorse_pgp_backend_initialize:
 * @gpg_homedir: (nullable):
 *
 * Initializes the PGP backend using the given argument as GNUPGHOME directory.
 * If @gpg_homedir is %NULL, the default will be used.
 *
 * (usually it only makes sense to set this to a nonnull value for tests).
 */
void
seahorse_pgp_backend_initialize (const char *gpg_homedir)
{
    SeahorsePgpBackend *self;

    g_return_if_fail (pgp_backend == NULL);
    self = g_object_new (SEAHORSE_PGP_TYPE_BACKEND,
                         "gpg-homedir", gpg_homedir,
                         NULL);

    seahorse_backend_register (SEAHORSE_BACKEND (self));
    g_object_unref (self);

    g_return_if_fail (pgp_backend != NULL);
}

SeahorseGpgmeKeyring *
seahorse_pgp_backend_get_default_keyring (SeahorsePgpBackend *self)
{
	self = self ? self : seahorse_pgp_backend_get ();
	g_return_val_if_fail (SEAHORSE_PGP_IS_BACKEND (self), NULL);
	g_return_val_if_fail (self->keyring, NULL);
	return self->keyring;
}

SeahorsePgpKey *
seahorse_pgp_backend_get_default_key (SeahorsePgpBackend *self)
{
    SeahorsePgpKey *key = NULL;
    g_autofree char *value = NULL;

    self = self ? self : seahorse_pgp_backend_get ();
    g_return_val_if_fail (SEAHORSE_PGP_IS_BACKEND (self), NULL);

    value = seahorse_pgp_settings_get_default_key (self->pgp_settings);
    if (value && *value) {
        const char *keyid;

        if (g_str_has_prefix (value, "openpgp:"))
            keyid = value + strlen ("openpgp:");
        else
            keyid = value;

        key = SEAHORSE_PGP_KEY (seahorse_gpgme_keyring_lookup (self->keyring, keyid));
    }

    return key;
}

#ifdef WITH_KEYSERVER

SeahorseDiscovery *
seahorse_pgp_backend_get_discovery (SeahorsePgpBackend *self)
{
	self = self ? self : seahorse_pgp_backend_get ();
	g_return_val_if_fail (SEAHORSE_PGP_IS_BACKEND (self), NULL);
	g_return_val_if_fail (self->discovery, NULL);
	return self->discovery;
}

/**
 * seahorse_pgp_backend_get_remotes:
 * @self: A #SeahorsePgpBackend
 *
 * Returns a list of remotes
 *
 * Returns: (transfer none) (element-type SeahorseServerSource):
 */
GListModel *
seahorse_pgp_backend_get_remotes (SeahorsePgpBackend *self)
{
    g_return_val_if_fail (SEAHORSE_PGP_IS_BACKEND (self), NULL);

    return self->remotes;
}

SeahorseServerSource *
seahorse_pgp_backend_lookup_remote (SeahorsePgpBackend *self,
                                    const char         *uri)
{
    self = self ? self : seahorse_pgp_backend_get ();
    g_return_val_if_fail (SEAHORSE_PGP_IS_BACKEND (self), NULL);

    for (guint i = 0; i < g_list_model_get_n_items (self->remotes); i++) {
        g_autoptr(SeahorseServerSource) ssrc = NULL;
        g_autofree char *src_uri = NULL;

        ssrc = g_list_model_get_item (self->remotes, i);
        src_uri = seahorse_place_get_uri (SEAHORSE_PLACE (ssrc));

        if (g_ascii_strcasecmp (uri, src_uri) == 0)
            return ssrc;
    }

    return NULL;
}

void
seahorse_pgp_backend_add_remote (SeahorsePgpBackend   *self,
                                 const char           *uri,
                                 gboolean              persist)
{
    g_return_if_fail (SEAHORSE_PGP_IS_BACKEND (self));
    g_return_if_fail (seahorse_pgp_backend_lookup_remote (self, uri) == NULL);

    if (persist) {
        /* Add to the PGP settings. That's all we need to do, since we're
         * subscribed to the "changed" callback */
        seahorse_pgp_settings_add_keyserver (self->pgp_settings, uri, NULL);
    } else {
        /* Don't persist, so just immediately create a ServerSource */
        g_autoptr(SeahorseServerSource) ssrc = NULL;
        ssrc = seahorse_server_category_create_server (uri);
        /* If the scheme of the uri is ldap, but ldap support is disabled
         * in the build, ssrc will be NULL. */
        if (ssrc)
            g_list_store_append (G_LIST_STORE (self->remotes), ssrc);
    }
}

void
seahorse_pgp_backend_remove_remote (SeahorsePgpBackend *self,
                                    const char         *uri)
{
    self = self ? self : seahorse_pgp_backend_get ();
    g_return_if_fail (SEAHORSE_PGP_IS_BACKEND (self));
    g_return_if_fail (uri && *uri);

    g_debug ("Removing remote %s", uri);

    for (guint i = 0; i < g_list_model_get_n_items (self->remotes); i++) {
        g_autoptr(SeahorseServerSource) ssrc = NULL;
        g_autofree char *src_uri = NULL;

        ssrc = g_list_model_get_item (self->remotes, i);
        src_uri = seahorse_place_get_uri (SEAHORSE_PLACE (ssrc));

        if (g_ascii_strcasecmp (uri, src_uri) == 0)
            g_list_store_remove (G_LIST_STORE (self->remotes), i);
    }

    seahorse_pgp_settings_remove_keyserver (self->pgp_settings, uri);
}

typedef struct {
    int num_searches;
} search_remote_closure;

static void
search_remote_closure_free (gpointer user_data)
{
    search_remote_closure *closure = user_data;
    g_free (closure);
}

static void
on_source_search_ready (GObject *source,
                        GAsyncResult *result,
                        gpointer user_data)
{
    g_autoptr(GTask) task = G_TASK (user_data);
    search_remote_closure *closure = g_task_get_task_data (task);
    g_autoptr(GError) error = NULL;

    g_return_if_fail (closure->num_searches > 0);

    if (!seahorse_server_source_search_finish (SEAHORSE_SERVER_SOURCE (source),
                                               result, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    closure->num_searches--;
    seahorse_progress_end (g_task_get_cancellable (task),
                           GINT_TO_POINTER (closure->num_searches));

    if (closure->num_searches == 0)
        g_task_return_boolean (task, TRUE);
}

void
seahorse_pgp_backend_search_remote_async (SeahorsePgpBackend *self,
                                          const gchar *search,
                                          GcrSimpleCollection *results,
                                          GCancellable *cancellable,
                                          GAsyncReadyCallback callback,
                                          gpointer user_data)
{
    search_remote_closure *closure;
    g_autoptr(GTask) task = NULL;
    g_autoptr(GHashTable) servers = NULL;
    g_auto(GStrv) names = NULL;

    self = self ? self : seahorse_pgp_backend_get ();
    g_return_if_fail (SEAHORSE_PGP_IS_BACKEND (self));

    /* Get a list of all selected key servers */
    names = seahorse_app_settings_get_last_search_servers (seahorse_app_settings_instance ());
    if (names != NULL && names[0] != NULL) {
        servers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
        for (guint i = 0; names[i] != NULL; i++)
            g_hash_table_insert (servers, g_strdup (names[i]), GINT_TO_POINTER (TRUE));
    }

    task = g_task_new (self, cancellable, callback, user_data);
    closure = g_new0 (search_remote_closure, 1);
    g_task_set_task_data (task, closure, search_remote_closure_free);

    for (guint i = 0; i < g_list_model_get_n_items (self->remotes); i++) {
        g_autoptr(SeahorseServerSource) ssrc = NULL;

        ssrc = g_list_model_get_item (self->remotes, i);
        if (servers) {
            g_autofree char *src_uri = NULL;
            src_uri = seahorse_place_get_uri (SEAHORSE_PLACE (ssrc));
            if (!g_hash_table_lookup (servers, src_uri))
                continue;
        }

        seahorse_progress_prep_and_begin (cancellable, GINT_TO_POINTER (closure->num_searches), NULL);
        seahorse_server_source_search_async (ssrc, search, results, cancellable,
                                             on_source_search_ready, g_object_ref (task));
        closure->num_searches++;
    }

    if (closure->num_searches == 0)
        g_task_return_boolean (task, FALSE);
}

gboolean
seahorse_pgp_backend_search_remote_finish (SeahorsePgpBackend *self,
                                           GAsyncResult *result,
                                           GError **error)
{
    g_return_val_if_fail (SEAHORSE_PGP_IS_BACKEND (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

typedef struct {
    int num_transfers;
} transfer_closure;

static void
transfer_closure_free (gpointer user_data)
{
    transfer_closure *closure = user_data;
    g_free (closure);
}

static void
on_source_transfer_ready (GObject *source,
                          GAsyncResult *result,
                          gpointer user_data)
{
    g_autoptr(GTask) task = G_TASK (user_data);
    transfer_closure *closure = g_task_get_task_data (task);
    g_autoptr(GError) error = NULL;

    g_return_if_fail (closure->num_transfers > 0);

    seahorse_transfer_finish (result, &error);
    if (error != NULL) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    closure->num_transfers--;
    seahorse_progress_end (g_task_get_cancellable (task),
                           GINT_TO_POINTER (closure->num_transfers));

    if (closure->num_transfers == 0)
        g_task_return_boolean (task, TRUE);
}

void
seahorse_pgp_backend_transfer_async (SeahorsePgpBackend *self,
                                     GList *keys,
                                     SeahorsePlace *to,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
    transfer_closure *closure;
    SeahorseObject *object;
    g_autoptr(GTask) task = NULL;
    SeahorsePlace *from;

    self = self ? self : seahorse_pgp_backend_get ();
    g_return_if_fail (SEAHORSE_PGP_IS_BACKEND (self));
    g_return_if_fail (SEAHORSE_IS_PLACE (to));

    task = g_task_new (self, cancellable, callback, user_data);
    closure = g_new0 (transfer_closure, 1);
    g_task_set_task_data (task, closure, transfer_closure_free);

    keys = g_list_copy (keys);
    /* Sort by key place */
    keys = seahorse_util_objects_sort_by_place (keys);

    while (keys) {
        GList *next;

        /* break off one set (same keysource) */
        next = seahorse_util_objects_splice_by_place (keys);

        g_assert (SEAHORSE_IS_OBJECT (keys->data));
        object = SEAHORSE_OBJECT (keys->data);

        /* Export from this key place */
        from = seahorse_object_get_place (object);
        g_return_if_fail (SEAHORSE_IS_PLACE (from));

        if (from != to) {
            /* Start a new transfer operation between the two places */
            seahorse_progress_prep_and_begin (cancellable, GINT_TO_POINTER (closure->num_transfers), NULL);
            seahorse_transfer_keys_async (from, to, keys, cancellable,
                                          on_source_transfer_ready, g_object_ref (task));
            closure->num_transfers++;
        }

        g_list_free (keys);
        keys = next;
    }

    if (closure->num_transfers == 0)
        g_task_return_boolean (task, TRUE);
}

gboolean
seahorse_pgp_backend_transfer_finish (SeahorsePgpBackend *self,
                                      GAsyncResult *result,
                                      GError **error)
{
    g_return_val_if_fail (SEAHORSE_PGP_IS_BACKEND (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

void
seahorse_pgp_backend_retrieve_async (SeahorsePgpBackend *self,
                                     const gchar **keyids,
                                     SeahorsePlace *to,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
    transfer_closure *closure;
    g_autoptr(GTask) task = NULL;

    g_return_if_fail (SEAHORSE_PGP_IS_BACKEND (self));
    g_return_if_fail (SEAHORSE_IS_PLACE (to));

    task = g_task_new (self, cancellable, callback, user_data);
    closure = g_new0 (transfer_closure, 1);
    g_task_set_task_data (task, closure, transfer_closure_free);

    for (guint i = 0; i < g_list_model_get_n_items (self->remotes); i++) {
        g_autoptr(SeahorseServerSource) ssrc = NULL;

        ssrc = g_list_model_get_item (self->remotes, i);

        /* Start a new transfer operation between the two places */
        seahorse_progress_prep_and_begin (cancellable,
                                          GINT_TO_POINTER (closure->num_transfers), NULL);
        seahorse_transfer_keyids_async (ssrc, to, keyids, cancellable,
                                        on_source_transfer_ready, g_object_ref (task));
        closure->num_transfers++;
    }

    if (closure->num_transfers == 0)
        g_task_return_boolean (task, TRUE);
}

gboolean
seahorse_pgp_backend_retrieve_finish (SeahorsePgpBackend *self,
                                      GAsyncResult *result,
                                      GError **error)
{
    g_return_val_if_fail (SEAHORSE_PGP_IS_BACKEND (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

#endif /* WITH_KEYSERVER */

GList *
seahorse_pgp_backend_discover_keys (SeahorsePgpBackend *self,
                                    const gchar **keyids,
                                    GCancellable *cancellable)
{
    GList *robjects = NULL;
    SeahorseGpgmeKey *key;
    SeahorseObject *object;
    g_autoptr(GPtrArray) todiscover = NULL;
    int i;

    self = self ? self : seahorse_pgp_backend_get ();
    g_return_val_if_fail (SEAHORSE_PGP_IS_BACKEND (self), NULL);

    todiscover = g_ptr_array_new ();

    /* Check all the ids */
    for (i = 0; keyids[i] != NULL; i++) {
        const gchar *keyid = keyids[i];

        /* Do we know about this object? */
        key = seahorse_gpgme_keyring_lookup (self->keyring, keyid);

        /* No such object anywhere, discover it */
        if (key == NULL) {
            g_ptr_array_add (todiscover, (gchar *)keyid);
            continue;
        }

        /* Our return value */
        robjects = g_list_prepend (robjects, key);
    }

    if (todiscover->len > 0) {
        g_ptr_array_add (todiscover, NULL);
        keyids = (const gchar **)todiscover->pdata;

#ifdef WITH_KEYSERVER
        /* Start a discover process on all todiscover */
        if (seahorse_app_settings_get_server_auto_retrieve (seahorse_app_settings_instance ()))
            seahorse_pgp_backend_retrieve_async (self, keyids, SEAHORSE_PLACE (self->keyring),
                                                 cancellable, NULL, NULL);
#endif

        /* Add unknown objects for all these */
        for (i = 0; keyids[i] != NULL; i++) {
            object = seahorse_unknown_source_add_object (self->unknown, keyids[i], cancellable);
            robjects = g_list_prepend (robjects, object);
        }
    }

    return robjects;
}
