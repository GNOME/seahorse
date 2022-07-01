/*
 * Seahorse
 *
 * Copyright (C) 2004 Stefan Walter
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "seahorse-server-source.h"

#include "seahorse-hkp-source.h"
#include "seahorse-ldap-source.h"
#include "seahorse-pgp-key.h"

#include "seahorse-common.h"

#include "libseahorse/seahorse-util.h"

#include <glib/gi18n.h>

#include <stdlib.h>
#include <string.h>

/**
 * SECTION:seahorse-server-source
 * @short_description: Objects for handling of internet sources of keys (hkp/ldap)
 * @include:seahorse-server-source.h
 *
 **/

enum {
    PROP_0,
    PROP_LABEL,
    PROP_DESCRIPTION,
    PROP_CATEGORY,
    PROP_URI,
    PROP_ACTIONS,
    PROP_ACTION_PREFIX,
    PROP_MENU_MODEL,
    N_PROPS
};

/* -----------------------------------------------------------------------------
 *  SERVER SOURCE
 */

typedef struct _SeahorseServerSourcePrivate {
    gchar *server;
    gchar *uri;
} SeahorseServerSourcePrivate;

static void      seahorse_server_source_list_model_init    (GListModelInterface *iface);

static void      seahorse_server_source_place_iface        (SeahorsePlaceIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (SeahorseServerSource, seahorse_server_source, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (SeahorseServerSource)
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, seahorse_server_source_list_model_init);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_PLACE, seahorse_server_source_place_iface);
);

/* GObject handlers */
static void seahorse_server_source_finalize   (GObject *gobject);
static void seahorse_server_get_property      (GObject *object, guint prop_id,
                                               GValue *value, GParamSpec *pspec);
static void seahorse_server_set_property      (GObject *object, guint prop_id,
                                               const GValue *value, GParamSpec *pspec);

static void
seahorse_server_source_class_init (SeahorseServerSourceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = seahorse_server_source_finalize;
    gobject_class->set_property = seahorse_server_set_property;
    gobject_class->get_property = seahorse_server_get_property;

    g_object_class_override_property (gobject_class, PROP_LABEL, "label");
    g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");
    g_object_class_override_property (gobject_class, PROP_CATEGORY, "category");
    g_object_class_override_property (gobject_class, PROP_ACTIONS, "actions");
    g_object_class_override_property (gobject_class, PROP_ACTION_PREFIX, "action-prefix");
    g_object_class_override_property (gobject_class, PROP_MENU_MODEL, "menu-model");

    g_object_class_install_property (gobject_class, PROP_URI,
            g_param_spec_string ("uri", "Key Server URI",
                                 "Key Server full URI", "",
                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
seahorse_server_source_init (SeahorseServerSource *ssrc)
{
}

static void
seahorse_server_source_finalize (GObject *gobject)
{
    SeahorseServerSource *ssrc = SEAHORSE_SERVER_SOURCE (gobject);
    SeahorseServerSourcePrivate *priv =
        seahorse_server_source_get_instance_private (ssrc);

    g_free (priv->server);
    g_free (priv->uri);

    G_OBJECT_CLASS (seahorse_server_source_parent_class)->finalize (gobject);
}

static void
seahorse_server_source_load (SeahorsePlace *self,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
    g_return_if_reached ();
}

static gboolean
seahorse_server_source_load_finish (SeahorsePlace *self,
                                     GAsyncResult *res,
                                     GError **error)
{
    g_return_val_if_reached (FALSE);
}

static gchar *
seahorse_server_source_get_label (SeahorsePlace* self)
{
    SeahorseServerSource *ssrc = SEAHORSE_SERVER_SOURCE (self);
    SeahorseServerSourcePrivate *priv =
        seahorse_server_source_get_instance_private (ssrc);

    return g_strdup (priv->server);
}

static void
seahorse_server_source_set_label (SeahorsePlace *self, const char *label)
{
}

static gchar *
seahorse_server_source_get_description (SeahorsePlace* self)
{
    SeahorseServerSource *ssrc = SEAHORSE_SERVER_SOURCE (self);
    SeahorseServerSourcePrivate *priv =
        seahorse_server_source_get_instance_private (ssrc);

    return g_strdup (priv->uri);
}

static gchar *
seahorse_server_source_get_uri (SeahorsePlace* self)
{
    SeahorseServerSource *ssrc = SEAHORSE_SERVER_SOURCE (self);
    SeahorseServerSourcePrivate *priv =
        seahorse_server_source_get_instance_private (ssrc);

    return g_strdup (priv->uri);
}

static SeahorsePlaceCategory
seahorse_server_source_get_category (SeahorsePlace *place)
{
    return SEAHORSE_PLACE_CATEGORY_KEYS;
}

static GActionGroup *
seahorse_server_source_get_actions (SeahorsePlace* self)
{
    return NULL;
}

static const gchar *
seahorse_server_source_get_action_prefix (SeahorsePlace* self)
{
    return NULL;
}

static GMenuModel *
seahorse_server_source_get_menu_model (SeahorsePlace* self)
{
    return NULL;
}

static void
seahorse_server_source_place_iface (SeahorsePlaceIface *iface)
{
    iface->load = seahorse_server_source_load;
    iface->load_finish = seahorse_server_source_load_finish;
    iface->get_actions = seahorse_server_source_get_actions;
    iface->get_action_prefix = seahorse_server_source_get_action_prefix;
    iface->get_menu_model = seahorse_server_source_get_menu_model;
    iface->get_description = seahorse_server_source_get_description;
    iface->get_category = seahorse_server_source_get_category;
    iface->get_label = seahorse_server_source_get_label;
    iface->set_label = seahorse_server_source_set_label;
    iface->get_uri = seahorse_server_source_get_uri;
}

static void
seahorse_server_set_property (GObject *object, guint prop_id,
                              const GValue *value, GParamSpec *pspec)
{
    SeahorseServerSource *ssrc = SEAHORSE_SERVER_SOURCE (object);
    SeahorseServerSourcePrivate *priv =
        seahorse_server_source_get_instance_private (ssrc);

    switch (prop_id) {
    case PROP_LABEL:
        seahorse_server_source_set_label (SEAHORSE_PLACE (ssrc),
                                          g_value_get_boxed (value));
        break;
    case PROP_URI:
        g_free (priv->uri);
        priv->uri = g_strdup (g_value_get_string (value));
        g_return_if_fail (priv->uri && priv->uri[0]);
        break;
    default:
        break;
    }
}

static void
seahorse_server_get_property (GObject *obj,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    SeahorseServerSource *self = SEAHORSE_SERVER_SOURCE (obj);
    SeahorsePlace *place = SEAHORSE_PLACE (self);

    switch (prop_id) {
    case PROP_LABEL:
        g_value_take_string (value, seahorse_server_source_get_label (place));
        break;
    case PROP_DESCRIPTION:
        g_value_take_string (value, seahorse_server_source_get_description (place));
        break;
    case PROP_URI:
        g_value_take_string (value, seahorse_server_source_get_uri (place));
        break;
    case PROP_CATEGORY:
        g_value_set_enum (value, seahorse_server_source_get_category (place));
        break;
    case PROP_ACTIONS:
        g_value_set_object (value, seahorse_server_source_get_actions (place));
        break;
    case PROP_ACTION_PREFIX:
        g_value_set_string (value, seahorse_server_source_get_action_prefix (place));
        break;
    case PROP_MENU_MODEL:
        g_value_set_object (value, seahorse_server_source_get_menu_model (place));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
        break;
    }
}

static unsigned int
seahorse_server_source_get_n_items (GListModel *list)
{
    return 0;
}

static void *
seahorse_server_source_get_item (GListModel *list,
                                 unsigned int pos)
{
    return NULL;
}

static GType
seahorse_server_source_get_item_type (GListModel *list)
{
    return G_TYPE_OBJECT;
}

static void
seahorse_server_source_list_model_init (GListModelInterface *iface)
{
    /* This is implemented because SeahorseSource requires it */
    iface->get_n_items = seahorse_server_source_get_n_items;
    iface->get_item = seahorse_server_source_get_item;
    iface->get_item_type = seahorse_server_source_get_item_type;
}

void
seahorse_server_source_search_async (SeahorseServerSource *self,
                                     const char           *match,
                                     GListStore           *results,
                                     GCancellable         *cancellable,
                                     GAsyncReadyCallback   callback,
                                     gpointer              user_data)
{
    g_return_if_fail (SEAHORSE_IS_SERVER_SOURCE (self));
    g_return_if_fail (match != NULL);
    g_return_if_fail (G_IS_LIST_STORE (results));
    g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
    g_return_if_fail (SEAHORSE_SERVER_SOURCE_GET_CLASS (self)->search_async);

    SEAHORSE_SERVER_SOURCE_GET_CLASS (self)->search_async (self, match, results,
                                                           cancellable, callback, user_data);
}

gboolean
seahorse_server_source_search_finish (SeahorseServerSource *self,
                                      GAsyncResult *result,
                                      GError **error)
{
    g_return_val_if_fail (SEAHORSE_IS_SERVER_SOURCE (self), FALSE);
    g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
    g_return_val_if_fail (SEAHORSE_SERVER_SOURCE_GET_CLASS (self)->search_finish, FALSE);

    return SEAHORSE_SERVER_SOURCE_GET_CLASS (self)->search_finish (self, result, error);
}

void
seahorse_server_source_export_async (SeahorseServerSource *self,
                                     const char **keyids,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
    SeahorseServerSourceClass *klass;

    g_return_if_fail (SEAHORSE_IS_SERVER_SOURCE (self));
    g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

    klass = SEAHORSE_SERVER_SOURCE_GET_CLASS (self);
    g_return_if_fail (klass->export_async);
    (klass->export_async) (self, keyids, cancellable, callback, user_data);
}

GBytes *
seahorse_server_source_export_finish (SeahorseServerSource *self,
                                      GAsyncResult *result,
                                      GError **error)
{
    SeahorseServerSourceClass *klass;

    g_return_val_if_fail (SEAHORSE_IS_SERVER_SOURCE (self), NULL);
    g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    klass = SEAHORSE_SERVER_SOURCE_GET_CLASS (self);
    g_return_val_if_fail (klass->export_finish != NULL, NULL);
    return (klass->export_finish) (self, result, error);
}

void
seahorse_server_source_import_async (SeahorseServerSource *source,
                                     GInputStream *input,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
    g_return_if_fail (SEAHORSE_IS_SERVER_SOURCE (source));
    g_return_if_fail (G_IS_INPUT_STREAM (input));
    g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
    g_return_if_fail (SEAHORSE_SERVER_SOURCE_GET_CLASS (source)->import_async);
    SEAHORSE_SERVER_SOURCE_GET_CLASS (source)->import_async (source, input, cancellable,
                                                             callback, user_data);
}

GList *
seahorse_server_source_import_finish (SeahorseServerSource *source,
                                      GAsyncResult *result,
                                      GError **error)
{
    g_return_val_if_fail (SEAHORSE_IS_SERVER_SOURCE (source), NULL);
    g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);
    g_return_val_if_fail (SEAHORSE_SERVER_SOURCE_GET_CLASS (source)->import_finish, NULL);
    return SEAHORSE_SERVER_SOURCE_GET_CLASS (source)->import_finish (source, result, error);
}
