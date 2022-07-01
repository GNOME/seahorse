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

#include "seahorse-pkcs11-backend.h"

#include "pkcs11/seahorse-pkcs11.h"

#include "seahorse-common.h"

#include "libseahorse/seahorse-util.h"

#include <gck/gck.h>

#include <glib/gi18n.h>

enum {
    PROP_0,
    PROP_NAME,
    PROP_LABEL,
    PROP_DESCRIPTION,
    PROP_ACTIONS,
    PROP_LOADED,
};

static SeahorsePkcs11Backend *pkcs11_backend = NULL;

struct _SeahorsePkcs11Backend {
    GObject parent;
    SeahorseActionGroup *actions;
    GList *blacklist;
    gboolean loaded;

    GPtrArray *tokens;
    GPtrArray *tokens_visible;
};

struct _SeahorsePkcs11BackendClass {
    GObjectClass parent_class;
};

static const char *token_blacklist[] = {
    "pkcs11:manufacturer=Gnome%20Keyring;serial=1:SSH:HOME",
    "pkcs11:manufacturer=Gnome%20Keyring;serial=1:SECRET:MAIN",
    "pkcs11:manufacturer=Mozilla%20Foundation;token=NSS%20Generic%20Crypto%20Services",
    NULL
};

static void
on_generate_activate (GSimpleAction *action, GVariant *param, gpointer user_data);

static const GActionEntry ACTION_ENTRIES[] = {
    { "pkcs11-generate-key", on_generate_activate  }
};

static void         seahorse_pkcs11_backend_iface            (SeahorseBackendIface *iface);

static void         seahorse_pkcs11_backend_list_model_init  (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorsePkcs11Backend, seahorse_pkcs11_backend, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, seahorse_pkcs11_backend_list_model_init)
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_BACKEND, seahorse_pkcs11_backend_iface);
);

static void
init_actions (SeahorsePkcs11Backend *self)
{
    self->actions = g_object_new (SEAHORSE_TYPE_ACTION_GROUP,
                                  "prefix", "pkcs11",
                                  NULL);
    g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                     ACTION_ENTRIES,
                                     G_N_ELEMENTS (ACTION_ENTRIES),
                                     self->actions);
}

static void
seahorse_pkcs11_backend_init (SeahorsePkcs11Backend *self)
{

    g_return_if_fail (pkcs11_backend == NULL);
    pkcs11_backend = self;

    self->tokens = g_ptr_array_new_with_free_func (g_object_unref);
    self->tokens_visible = g_ptr_array_new ();

    for (unsigned int i = 0; token_blacklist[i] != NULL; i++) {
        g_autoptr(GError) error = NULL;
        GckUriData *uri;

        uri = gck_uri_data_parse (token_blacklist[i], GCK_URI_FOR_TOKEN | GCK_URI_FOR_MODULE, &error);
        if (uri == NULL) {
            g_warning ("couldn't parse pkcs11 blacklist uri: %s", error->message);
        }
        self->blacklist = g_list_prepend (self->blacklist, uri);
    }

    init_actions (self);
}

static gboolean
is_token_usable (SeahorsePkcs11Backend *self,
                 GckSlot *slot,
                 GckTokenInfo *token)
{
    if (!(token->flags & CKF_TOKEN_INITIALIZED)) {
        /* _gcr_debug ("token is not importable: %s: not initialized", token->label); */
        return FALSE;
    }
    if ((token->flags & CKF_LOGIN_REQUIRED) &&
        !(token->flags & CKF_USER_PIN_INITIALIZED)) {
        /* _gcr_debug ("token is not importable: %s: user pin not initialized", token->label); */
        return FALSE;
    }

    for (GList *l = self->blacklist; l != NULL; l = g_list_next (l)) {
        if (gck_slot_match (slot, l->data))
            return FALSE;
    }

    return TRUE;
}

/* We want to avoid showing tokens that will be always empty */
static gboolean
token_visible (unsigned int n_items,
               GckTokenInfo *info)
{
    gboolean needs_login;

    g_return_val_if_fail (info, FALSE);

    needs_login = (info->flags & CKF_LOGIN_REQUIRED) == CKF_LOGIN_REQUIRED;
    return (n_items >= 0) || needs_login;
}

/* We only expose non-empty tokens */
static void
on_token_items_changed (GListModel *list,
                        unsigned int position,
                        unsigned int removed,
                        unsigned int added,
                        void *user_data)
{
    SeahorsePkcs11Token *token = SEAHORSE_PKCS11_TOKEN (list);
    SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (user_data);
    unsigned int n_items, old_n_items;
    gboolean was_visible, is_visible;
    GckTokenInfo *info = NULL;

    info = seahorse_pkcs11_token_get_info (token);

    n_items = g_list_model_get_n_items (list);
    is_visible = token_visible (n_items, info);

    old_n_items = (n_items + removed - added);
    was_visible = token_visible (old_n_items, info);

    if (is_visible == was_visible)
        return;

    /* Token was empty, but no longer is: add to exposed items */
    if (is_visible) {
        g_return_if_fail (!g_ptr_array_find (self->tokens_visible, token, NULL));
        g_ptr_array_add (self->tokens_visible, token);
        g_list_model_items_changed (G_LIST_MODEL (self), self->tokens_visible->len - 1, 0, 1);
    }

    /* Token was not empty, but now is: remove from exposed items */
    if (was_visible) {
        gboolean found;
        unsigned int pos;

        found = g_ptr_array_find (self->tokens_visible, token, &pos);
        g_return_if_fail (found);
        g_ptr_array_remove_index (self->tokens_visible, pos);
        g_list_model_items_changed (G_LIST_MODEL (self), pos, 1, 0);
    }
}

static void
on_initialized_registered (GObject *unused,
                           GAsyncResult *result,
                           gpointer user_data)
{
    g_autoptr(SeahorsePkcs11Backend) self = SEAHORSE_PKCS11_BACKEND (user_data);
    g_autolist(GckModule) modules = NULL;
    g_autoptr(GError) error = NULL;

    modules = gck_modules_initialize_registered_finish (result, &error);
    if (error != NULL) {
        g_warning ("%s", error->message);
    }

    g_debug ("Found %u PKCS#11 modules", g_list_length (modules));

    for (GList *m = modules; m != NULL; m = g_list_next (m)) {
        g_autolist(GckSlot) slots = NULL;

        slots = gck_module_get_slots (m->data, TRUE);

        g_debug ("Found %u slots for PKCS#11 module %p", g_list_length (modules), slots);

        for (GList *s = slots; s; s = g_list_next (s)) {
            g_autoptr(GckTokenInfo) info = NULL;
            SeahorsePkcs11Token *token;
            unsigned int n_items;

            info = gck_slot_get_token_info (s->data);
            if (info == NULL || !is_token_usable (self, s->data, info)) {
                g_debug ("Skipping unusable token slot %p", slots);
                continue;
            }

            token = seahorse_pkcs11_token_new (s->data);
            g_ptr_array_add (self->tokens, token);

            n_items = g_list_model_get_n_items (G_LIST_MODEL (token));
            if (token_visible (n_items, info)) {
                g_ptr_array_add (self->tokens_visible, token);
                g_list_model_items_changed (G_LIST_MODEL (self),
                                            self->tokens_visible->len - 1,
                                            0, 1);
            } else {
                g_debug ("Not showing token %p", token);
            }
            g_signal_connect (token, "items-changed", G_CALLBACK (on_token_items_changed), self);
        }
    }

    self->loaded = TRUE;
    g_object_notify (G_OBJECT (self), "loaded");
}

static void
seahorse_pkcs11_backend_constructed (GObject *obj)
{
    SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (obj);

    G_OBJECT_CLASS (seahorse_pkcs11_backend_parent_class)->constructed (obj);

    gck_modules_initialize_registered_async (NULL, on_initialized_registered,
                                             g_object_ref (self));
}

static const gchar *
seahorse_pkcs11_backend_get_name (SeahorseBackend *backend)
{
    return SEAHORSE_PKCS11_NAME;
}

static const gchar *
seahorse_pkcs11_backend_get_label (SeahorseBackend *backend)
{
    return _("Certificates");
}

static const gchar *
seahorse_pkcs11_backend_get_description (SeahorseBackend *backend)
{
    return _("X.509 certificates and related keys");
}

static gboolean
seahorse_pkcs11_backend_get_loaded (SeahorseBackend *backend)
{
    g_return_val_if_fail (SEAHORSE_IS_PKCS11_BACKEND (backend), FALSE);

    return SEAHORSE_PKCS11_BACKEND (backend)->loaded;
}

static SeahorseActionGroup *
seahorse_pkcs11_backend_get_actions (SeahorseBackend *backend)
{
    return g_object_ref (SEAHORSE_PKCS11_BACKEND (backend)->actions);
}

static void
seahorse_pkcs11_backend_get_property (GObject *obj,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
    SeahorseBackend *backend = SEAHORSE_BACKEND (obj);

    switch (prop_id) {
    case PROP_NAME:
        g_value_set_string (value, seahorse_pkcs11_backend_get_name (backend));
        break;
    case PROP_LABEL:
        g_value_set_string (value, seahorse_pkcs11_backend_get_label (backend));
        break;
    case PROP_DESCRIPTION:
        g_value_set_string (value, seahorse_pkcs11_backend_get_description (backend));
        break;
    case PROP_ACTIONS:
        g_value_take_object (value, seahorse_pkcs11_backend_get_actions (backend));
        break;
    case PROP_LOADED:
        g_value_set_boolean (value, seahorse_pkcs11_backend_get_loaded (backend));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
        break;
    }
}

static void
seahorse_pkcs11_backend_finalize (GObject *obj)
{
    SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (obj);

    g_list_free_full (self->blacklist, (GDestroyNotify)gck_uri_data_free);
    g_clear_object (&self->actions);
    g_clear_pointer (&self->tokens_visible, g_ptr_array_unref);
    g_clear_pointer (&self->tokens, g_ptr_array_unref);
    g_return_if_fail (pkcs11_backend == self);
    pkcs11_backend = NULL;

    G_OBJECT_CLASS (seahorse_pkcs11_backend_parent_class)->finalize (obj);
}

static void
seahorse_pkcs11_backend_class_init (SeahorsePkcs11BackendClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->constructed = seahorse_pkcs11_backend_constructed;
    gobject_class->finalize = seahorse_pkcs11_backend_finalize;
    gobject_class->get_property = seahorse_pkcs11_backend_get_property;

    g_object_class_override_property (gobject_class, PROP_NAME, "name");
    g_object_class_override_property (gobject_class, PROP_LABEL, "label");
    g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");
    g_object_class_override_property (gobject_class, PROP_ACTIONS, "actions");
    g_object_class_override_property (gobject_class, PROP_LOADED, "loaded");
}

static GType
seahorse_pkcs11_backend_get_item_type (GListModel *model)
{
    return SEAHORSE_PKCS11_TYPE_TOKEN;
}

static unsigned int
seahorse_pkcs11_backend_get_n_items (GListModel *model)
{
    SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (model);
    return self->tokens_visible->len;
}

static void *
seahorse_pkcs11_backend_get_item (GListModel   *model,
                                  unsigned int  position)
{
    SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (model);

    if (position >= self->tokens_visible->len)
        return NULL;
    return g_object_ref (g_ptr_array_index (self->tokens_visible, position));
}

static void
seahorse_pkcs11_backend_list_model_init (GListModelInterface *iface)
{
    iface->get_item_type = seahorse_pkcs11_backend_get_item_type;
    iface->get_n_items = seahorse_pkcs11_backend_get_n_items;
    iface->get_item = seahorse_pkcs11_backend_get_item;
}

static SeahorsePlace *
seahorse_pkcs11_backend_lookup_place (SeahorseBackend *backend,
                                      const gchar *uri)
{
    SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (backend);
    g_autoptr(GckUriData) uri_data = NULL;

    if (!g_str_has_prefix (uri, "pkcs11:"))
        return NULL;

    uri_data = gck_uri_data_parse (uri, GCK_URI_FOR_TOKEN | GCK_URI_FOR_MODULE, NULL);
    if (uri_data == NULL)
        return NULL;

    for (unsigned int i = 0; i < self->tokens->len; i++) {
        SeahorsePkcs11Token *token = g_ptr_array_index (self->tokens, i);

        if (gck_slot_match (seahorse_pkcs11_token_get_slot (token), uri_data))
            return SEAHORSE_PLACE (token);
    }

    return NULL;
}

static void
seahorse_pkcs11_backend_iface (SeahorseBackendIface *iface)
{
    iface->lookup_place = seahorse_pkcs11_backend_lookup_place;
    iface->get_actions = seahorse_pkcs11_backend_get_actions;
    iface->get_description = seahorse_pkcs11_backend_get_description;
    iface->get_label = seahorse_pkcs11_backend_get_label;
    iface->get_name = seahorse_pkcs11_backend_get_name;
    iface->get_loaded = seahorse_pkcs11_backend_get_loaded;
}

void
seahorse_pkcs11_backend_initialize (void)
{
    SeahorsePkcs11Backend *self;

    g_return_if_fail (pkcs11_backend == NULL);
    self = g_object_new (SEAHORSE_TYPE_PKCS11_BACKEND, NULL);

    seahorse_backend_register (SEAHORSE_BACKEND (self));
    g_object_unref (self);

    g_return_if_fail (pkcs11_backend != NULL);
}

SeahorsePkcs11Backend *
seahorse_pkcs11_backend_get (void)
{
    g_return_val_if_fail (pkcs11_backend, NULL);
    return pkcs11_backend;
}

static void
on_generate_activate (GSimpleAction *action,
                      GVariant *param,
                      gpointer user_data)
{
    SeahorseActionGroup *actions = SEAHORSE_ACTION_GROUP (user_data);
    SeahorseCatalog *catalog;
    SeahorsePkcs11Generate *dialog;

    catalog = seahorse_action_group_get_catalog (actions);
    dialog = seahorse_pkcs11_generate_new (GTK_WINDOW (catalog));
    gtk_window_present (GTK_WINDOW (dialog));
    g_clear_object (&catalog);
}
