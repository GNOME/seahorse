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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "seahorse-pgp-key.h"
#include "seahorse-unknown.h"
#include "seahorse-unknown-source.h"

#include <glib/gi18n.h>

struct _SeahorseUnknown {
    GObject parent;

    char *keyid;
};

enum {
    PROP_0,
    PROP_KEYID,
    N_PROPS
};
static GParamSpec *properties[N_PROPS] = { NULL, };

G_DEFINE_TYPE (SeahorseUnknown, seahorse_unknown, G_TYPE_OBJECT);

static void
seahorse_unknown_get_property (GObject *object,
                               unsigned int prop_id,
                               GValue *value,
                               GParamSpec *pspec)
{
    SeahorseUnknown *self = SEAHORSE_UNKNOWN (object);

    switch (prop_id) {
    case PROP_KEYID:
        g_value_set_string (value, seahorse_unknown_get_keyid (self));
        break;
    }
}

static void
seahorse_unknown_set_property (GObject *object,
                               unsigned int prop_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
    SeahorseUnknown *self = SEAHORSE_UNKNOWN (object);

    switch (prop_id) {
    case PROP_KEYID:
        g_clear_pointer (&self->keyid, g_free);
        self->keyid = g_value_dup_string (value);
        break;
    }
}

static void
seahorse_unknown_object_finalize (GObject *obj)
{
    SeahorseUnknown *self = SEAHORSE_UNKNOWN (obj);

    g_clear_pointer (&self->keyid, g_free);

    G_OBJECT_CLASS (seahorse_unknown_parent_class)->finalize (G_OBJECT (self));
}

static void
seahorse_unknown_init (SeahorseUnknown *self)
{
}

static void
seahorse_unknown_class_init (SeahorseUnknownClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = seahorse_unknown_object_finalize;
    gobject_class->set_property = seahorse_unknown_set_property;
    gobject_class->get_property = seahorse_unknown_get_property;

    properties[PROP_KEYID] =
        g_param_spec_string ("keyid", NULL, NULL, NULL,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

SeahorseUnknown *
seahorse_unknown_new (SeahorseUnknownSource *source,
                      const char *keyid,
                      const char *display)
{
    const char *identifier;

    if (!display)
        display = _("Unavailable");
    identifier = seahorse_pgp_key_calc_identifier (keyid);

    return g_object_new (SEAHORSE_TYPE_UNKNOWN,
                         "place", source,
                         "label", display,
                         "identifier", identifier,
                         "keyid", keyid,
                         NULL);
}

const char *
seahorse_unknown_get_keyid (SeahorseUnknown *self)
{
    g_return_val_if_fail (SEAHORSE_IS_UNKNOWN (self), NULL);

    return self->keyid;
}
