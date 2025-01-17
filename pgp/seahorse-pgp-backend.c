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
#include <locale.h>

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

static void         seahorse_pgp_backend_list_model_init  (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorsePgpBackend, seahorse_pgp_backend, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, seahorse_pgp_backend_list_model_init);
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
                                void       *user_data)
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
on_place_loaded (GObject      *object,
                 GAsyncResult *result,
                 void         *user_data)
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

static const char *
seahorse_pgp_backend_get_name (SeahorseBackend *backend)
{
    return SEAHORSE_PGP_NAME;
}

static const char *
seahorse_pgp_backend_get_label (SeahorseBackend *backend)
{
    return _("PGP Keys");
}

static const char *
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

static GType
seahorse_pgp_backend_get_item_type (GListModel *model)
{
    return SEAHORSE_TYPE_GPGME_KEYRING;
}

static unsigned int
seahorse_pgp_backend_get_n_items (GListModel *model)
{
    return 1;
}

static void *
seahorse_pgp_backend_get_item (GListModel   *model,
                               unsigned int  position)
{
    SeahorsePgpBackend *self = SEAHORSE_PGP_BACKEND (model);
    return (position == 0)? g_object_ref (self->keyring) : NULL;
}

static void
seahorse_pgp_backend_list_model_init (GListModelInterface *iface)
{
    iface->get_item_type = seahorse_pgp_backend_get_item_type;
    iface->get_n_items = seahorse_pgp_backend_get_n_items;
    iface->get_item = seahorse_pgp_backend_get_item;
}

static SeahorsePlace *
seahorse_pgp_backend_lookup_place (SeahorseBackend *backend,
                                   const char *uri)
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
search_remote_closure_free (void *user_data)
{
    search_remote_closure *closure = user_data;
    g_free (closure);
}

static void
on_source_search_ready (GObject *source,
                        GAsyncResult *result,
                        void *user_data)
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
                                          const char *search,
                                          GListStore *results,
                                          GCancellable *cancellable,
                                          GAsyncReadyCallback callback,
                                          void *user_data)
{
    search_remote_closure *closure;
    g_autoptr(GTask) task = NULL;
    g_autoptr(GHashTable) servers = NULL;
    g_auto(GStrv) names = NULL;

    g_return_if_fail (SEAHORSE_PGP_IS_BACKEND (self));
    g_return_if_fail (search);
    g_return_if_fail (G_IS_LIST_STORE (results));

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
transfer_closure_free (void *user_data)
{
    transfer_closure *closure = user_data;
    g_free (closure);
}

static void
on_source_transfer_ready (GObject *source,
                          GAsyncResult *result,
                          void *user_data)
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
                                     GListModel *keys,
                                     SeahorsePlace *to,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     void *user_data)
{
    g_autoptr(GTask) task = NULL;
    transfer_closure *closure;
    g_autoptr(GtkSortListModel) sorted_keys = NULL;
    g_autoptr(GtkSorter) sorter = NULL;
    unsigned int current_pos;

    g_return_if_fail (SEAHORSE_PGP_IS_BACKEND (self));
    g_return_if_fail (G_IS_LIST_MODEL (keys));
    g_return_if_fail (g_list_model_get_n_items (keys) > 0);
    g_return_if_fail (SEAHORSE_IS_PLACE (to));

    task = g_task_new (self, cancellable, callback, user_data);
    closure = g_new0 (transfer_closure, 1);
    g_task_set_task_data (task, closure, transfer_closure_free);

    /* Sort the keys by place and put them into sections as well */
    sorter = GTK_SORTER (seahorse_place_sorter_new ());
    sorted_keys = gtk_sort_list_model_new (g_object_ref (keys),
                                           g_object_ref (sorter));
    gtk_sort_list_model_set_section_sorter (sorted_keys, sorter);

    current_pos = 0;
    while (current_pos < g_list_model_get_n_items (keys)) {
        g_autoptr(SeahorsePgpKey) first = NULL;
        SeahorsePlace *from;
        g_autolist(SeahorsePgpKey) to_transfer = NULL;
        unsigned int section_start, section_end;

        /* Get the range for this section */
        gtk_section_model_get_section (GTK_SECTION_MODEL (sorted_keys),
                                       current_pos,
                                       &section_start, &section_end);

        /* Build the list of keys to transfer */
        for (unsigned int i = section_start; i < section_end; i++) {
            g_autoptr(SeahorsePgpKey) key = NULL;

            key = g_list_model_get_item (G_LIST_MODEL (sorted_keys), i);
            to_transfer = g_list_prepend (to_transfer, key);
        }

        /* Get the place were transferring frmo */
        first = g_list_model_get_item (G_LIST_MODEL (sorted_keys), current_pos);
        from = seahorse_item_get_place (SEAHORSE_ITEM (first));
        g_return_if_fail (SEAHORSE_IS_PLACE (from));

        if (from != to) {
            /* Start a new transfer operation between the two places */
            seahorse_progress_prep_and_begin (cancellable, GINT_TO_POINTER (closure->num_transfers), NULL);
            seahorse_transfer_keys_async (from, to,
                                          to_transfer,
                                          cancellable,
                                          on_source_transfer_ready, g_object_ref (task));
            closure->num_transfers++;
        }

        /* Go to the next section */
        current_pos = section_end;
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
                                     const char **keyids,
                                     SeahorsePlace *to,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     void *user_data)
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
                                    const char **keyids,
                                    GCancellable *cancellable)
{
    GList *robjects = NULL;
    SeahorseGpgmeKey *key;
    g_autoptr(GPtrArray) todiscover = NULL;
    int i;

    self = self ? self : seahorse_pgp_backend_get ();
    g_return_val_if_fail (SEAHORSE_PGP_IS_BACKEND (self), NULL);

    todiscover = g_ptr_array_new ();

    /* Check all the ids */
    for (i = 0; keyids[i] != NULL; i++) {
        const char *keyid = keyids[i];

        /* Do we know about this object? */
        key = seahorse_gpgme_keyring_lookup (self->keyring, keyid);

        /* No such object anywhere, discover it */
        if (key == NULL) {
            g_ptr_array_add (todiscover, (char *) keyid);
            continue;
        }

        /* Our return value */
        robjects = g_list_prepend (robjects, key);
    }

    if (todiscover->len > 0) {
        g_ptr_array_add (todiscover, NULL);
        keyids = (const char **) todiscover->pdata;

#ifdef WITH_KEYSERVER
        /* Start a discover process on all todiscover */
        if (seahorse_app_settings_get_server_auto_retrieve (seahorse_app_settings_instance ()))
            seahorse_pgp_backend_retrieve_async (self, keyids, SEAHORSE_PLACE (self->keyring),
                                                 cancellable, NULL, NULL);
#endif

        /* Add unknown objects for all these */
        for (i = 0; keyids[i] != NULL; i++) {
            SeahorseUnknown *unknown;

            unknown = seahorse_unknown_source_add_object (self->unknown, keyids[i], cancellable);
            robjects = g_list_prepend (robjects, unknown);
        }
    }

    return robjects;
}

SeahorsePgpKey *
seahorse_pgp_backend_create_key_for_parsed (SeahorsePgpBackend *self,
                                            GcrParsed *parsed)
{
    g_autoptr(GInputStream) stream = NULL;
    const void *data;
    size_t data_len;
    gpgme_ctx_t gctx;
    gpgme_error_t gerr;
    gpgme_key_t gkey;
    gpgme_key_t end;
    gpgme_data_t gpgme_data = NULL;
    g_autoptr(SeahorsePgpKey) key = NULL;

    g_return_val_if_fail (gcr_parsed_get_format (parsed) == GCR_FORMAT_OPENPGP_PACKET, NULL);

    data = gcr_parsed_get_data (parsed, &data_len);
    g_return_val_if_fail (data != NULL, NULL);

    /* Create a temporary context */
    setlocale (LC_ALL, "");
    gpgme_check_version (NULL);
    gpgme_set_locale (NULL, LC_CTYPE, setlocale (LC_CTYPE, NULL));

    gerr = gpgme_new (&gctx);
    g_return_val_if_fail (GPG_IS_OK (gerr), NULL);

    gerr = gpgme_set_protocol (gctx, GPGME_PROTOCOL_OPENPGP);
    g_return_val_if_fail (GPG_IS_OK (gerr), NULL);

    gpgme_set_armor (gctx, 0);
    gpgme_set_textmode (gctx, 0);
    gpgme_set_offline (gctx, 1);
    gpgme_set_keylist_mode (gctx, GPGME_KEYLIST_MODE_LOCAL);

    /* Put it in a gpgme data struct which we can put in the special gpgme_op_keylist_from_data_start() */
    gerr = gpgme_data_new (&gpgme_data);
    g_return_val_if_fail (GPG_IS_OK (gerr), NULL);

    gerr = gpgme_data_new_from_mem(&gpgme_data, data, data_len, 0);
    g_return_val_if_fail (GPG_IS_OK (gerr), NULL);

    gerr = gpgme_op_keylist_from_data_start(gctx, gpgme_data, 0);
    g_return_val_if_fail (GPG_IS_OK (gerr), NULL);

    gerr = gpgme_op_keylist_next(gctx, &gkey);
    g_return_val_if_fail (GPG_IS_OK (gerr), NULL);

    gerr = gpgme_op_keylist_next(gctx, &end);
    // XXX supposedly we're only to have one here? What about public vs secret
    g_return_val_if_fail (!GPG_IS_OK (gerr), NULL);

    if (gkey->revoked || gkey->expired || gkey->disabled || gkey->invalid || gkey->secret) {
        //XXX some things can be left here, but what about invalid? disabled?
        return NULL;
    }

    //XXX how do we know it's a public or secret key?
    // XXX something is off here also, as it shows as a private key only
    if (gkey->secret)
        key = SEAHORSE_PGP_KEY (seahorse_gpgme_key_new (NULL, NULL, gkey));
    else
        key = SEAHORSE_PGP_KEY (seahorse_gpgme_key_new (NULL, gkey, NULL));

    //XXX don't we need to release the gpgme-context?

    return g_steal_pointer (&key);
}
