/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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

#include "config.h"

#include "seahorse-unknown-source.h"

#include "seahorse-pgp-key.h"

#include <gcr/gcr.h>

#include <glib/gi18n.h>

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

struct _SeahorseUnknownSource {
    GObject parent;

    GPtrArray *keys;
};

static void      seahorse_unknown_source_list_model_iface      (GListModelInterface *iface);

static void      seahorse_unknown_source_place_iface           (SeahorsePlaceIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseUnknownSource, seahorse_unknown_source, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, seahorse_unknown_source_list_model_iface);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_PLACE, seahorse_unknown_source_place_iface);
);

static void
seahorse_unknown_source_init (SeahorseUnknownSource *self)
{
    self->keys = g_ptr_array_new_with_free_func (g_object_unref);
}

static SeahorseUnknown *
seahorse_unknown_lookup_by_keyid (SeahorseUnknownSource *self,
                                  const char *keyid)
{
    for (unsigned int i = 0; i < self->keys->len; i++) {
        SeahorseUnknown *unknown = g_ptr_array_index (self->keys, i);
        const char *unknown_keyid;

        unknown_keyid = seahorse_unknown_get_keyid (unknown);
        if (seahorse_pgp_keyid_equal (keyid, unknown_keyid))
            return unknown;
    }

    return NULL;
}

static void
seahorse_unknown_source_load (SeahorsePlace *self,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
    g_return_if_reached ();
}

static gboolean
seahorse_unknown_source_load_finish (SeahorsePlace *self,
                                     GAsyncResult *res,
                                     GError **error)
{
    g_return_val_if_reached (FALSE);
}

static gchar *
seahorse_unknown_source_get_label (SeahorsePlace* self)
{
    return g_strdup ("");
}

static void
seahorse_unknown_source_set_label (SeahorsePlace *self, const char *label)
{
}

static gchar *
seahorse_unknown_source_get_description (SeahorsePlace* self)
{
    return NULL;
}

static gchar *
seahorse_unknown_source_get_uri (SeahorsePlace* self)
{
    return NULL;
}

static SeahorsePlaceCategory
seahorse_unknown_source_get_category (SeahorsePlace *place)
{
    return SEAHORSE_PLACE_CATEGORY_KEYS;
}

static GActionGroup *
seahorse_unknown_source_get_actions (SeahorsePlace* self)
{
    return NULL;
}

static const gchar *
seahorse_unknown_source_get_action_prefix (SeahorsePlace* self)
{
    return NULL;
}

static GMenuModel *
seahorse_unknown_source_get_menu_model (SeahorsePlace* self)
{
    return NULL;
}

static void
seahorse_unknown_source_get_property (GObject *obj,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
    SeahorsePlace *place = SEAHORSE_PLACE (obj);

    switch (prop_id) {
    case PROP_LABEL:
        g_value_take_string (value, seahorse_unknown_source_get_label (place));
        break;
    case PROP_DESCRIPTION:
        g_value_take_string (value, seahorse_unknown_source_get_description (place));
        break;
    case PROP_URI:
        g_value_take_string (value, seahorse_unknown_source_get_uri (place));
        break;
    case PROP_CATEGORY:
        g_value_set_enum (value, seahorse_unknown_source_get_category (place));
        break;
    case PROP_ACTIONS:
        g_value_take_object (value, seahorse_unknown_source_get_actions (place));
        break;
    case PROP_ACTION_PREFIX:
        g_value_set_string (value, seahorse_unknown_source_get_action_prefix (place));
        break;
    case PROP_MENU_MODEL:
        g_value_take_object (value, seahorse_unknown_source_get_menu_model (place));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
        break;
    }
}

static void
seahorse_unknown_source_set_property (GObject *obj,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
    SeahorsePlace *place = SEAHORSE_PLACE (obj);

    switch (prop_id) {
    case PROP_LABEL:
        seahorse_unknown_source_set_label (place, g_value_get_boxed (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
        break;
    }
}

static void
seahorse_unknown_source_finalize (GObject *obj)
{
    SeahorseUnknownSource *self = SEAHORSE_UNKNOWN_SOURCE (obj);

    g_ptr_array_unref (self->keys);

    G_OBJECT_CLASS (seahorse_unknown_source_parent_class)->finalize (obj);
}

static void
seahorse_unknown_source_class_init (SeahorseUnknownSourceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->get_property = seahorse_unknown_source_get_property;
    gobject_class->set_property = seahorse_unknown_source_set_property;
    gobject_class->finalize = seahorse_unknown_source_finalize;

    g_object_class_override_property (gobject_class, PROP_LABEL, "label");
    g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");
    g_object_class_override_property (gobject_class, PROP_CATEGORY, "category");
    g_object_class_override_property (gobject_class, PROP_ACTIONS, "actions");
    g_object_class_override_property (gobject_class, PROP_ACTION_PREFIX, "action-prefix");
    g_object_class_override_property (gobject_class, PROP_MENU_MODEL, "menu-model");
    g_object_class_override_property (gobject_class, PROP_URI, "uri");
}

static unsigned int
seahorse_unknown_source_get_n_items (GListModel *list)
{
    SeahorseUnknownSource *self = SEAHORSE_UNKNOWN_SOURCE (list);

    return self->keys->len;
}

static void *
seahorse_unknown_source_get_item (GListModel *list,
                                  unsigned int pos)
{
    SeahorseUnknownSource *self = SEAHORSE_UNKNOWN_SOURCE (list);

    if (pos >= self->keys->len)
        return NULL;

    return g_object_ref (g_ptr_array_index (self->keys, pos));
}

static GType
seahorse_unknown_source_get_item_type (GListModel *list)
{
    return SEAHORSE_TYPE_UNKNOWN;
}

static void
seahorse_unknown_source_list_model_iface (GListModelInterface *iface)
{
    iface->get_n_items = seahorse_unknown_source_get_n_items;
    iface->get_item = seahorse_unknown_source_get_item;
    iface->get_item_type = seahorse_unknown_source_get_item_type;
}

static void
seahorse_unknown_source_place_iface (SeahorsePlaceIface *iface)
{
    iface->load = seahorse_unknown_source_load;
    iface->load_finish = seahorse_unknown_source_load_finish;
    iface->get_actions = seahorse_unknown_source_get_actions;
    iface->get_menu_model = seahorse_unknown_source_get_menu_model;
    iface->get_description = seahorse_unknown_source_get_description;
    iface->get_category = seahorse_unknown_source_get_category;
    iface->get_label = seahorse_unknown_source_get_label;
    iface->set_label = seahorse_unknown_source_set_label;
    iface->get_uri = seahorse_unknown_source_get_uri;
}

SeahorseUnknownSource*
seahorse_unknown_source_new (void)
{
    return g_object_new (SEAHORSE_TYPE_UNKNOWN_SOURCE, NULL);
}

static void
on_cancellable_gone (gpointer user_data,
                     GObject *where_the_object_was)
{
    /* TODO: Change the icon */
}

SeahorseUnknown *
seahorse_unknown_source_add_object (SeahorseUnknownSource *self,
                                    const char *keyid,
                                    GCancellable *cancellable)
{
    SeahorseUnknown *unknown;

    g_return_val_if_fail (SEAHORSE_IS_UNKNOWN_SOURCE (self), NULL);
    g_return_val_if_fail (keyid != NULL, NULL);
    g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

    unknown = seahorse_unknown_lookup_by_keyid (self, keyid);
    if (unknown == NULL) {
        unknown = seahorse_unknown_new (self, keyid, NULL);
        g_ptr_array_add (self->keys, unknown);
    }

    if (cancellable)
        g_object_weak_ref (G_OBJECT (cancellable), on_cancellable_gone, unknown);

    return unknown;
}
