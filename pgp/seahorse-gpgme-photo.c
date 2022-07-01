/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

#include "seahorse-gpgme.h"
#include "seahorse-gpgme-photo.h"

#include <string.h>

#include <glib/gi18n.h>

enum {
    PROP_0,
    PROP_PUBKEY,
    PROP_INDEX
};

struct _SeahorseGpgmePhoto {
    SeahorsePgpPhoto parent_instance;

    gpgme_key_t pubkey;        /* Key that this photo is on */
    guint index;               /* The GPGME index of the photo */
};

G_DEFINE_TYPE (SeahorseGpgmePhoto, seahorse_gpgme_photo, SEAHORSE_PGP_TYPE_PHOTO);

static void
seahorse_gpgme_photo_init (SeahorseGpgmePhoto *self)
{
}

static GObject*
seahorse_gpgme_photo_constructor (GType type, guint n_props, GObjectConstructParam *props)
{
    GObject *obj = G_OBJECT_CLASS (seahorse_gpgme_photo_parent_class)->constructor (type, n_props, props);
    SeahorseGpgmePhoto *self = NULL;

    if (obj) {
        self = SEAHORSE_GPGME_PHOTO (obj);
        g_return_val_if_fail (self->pubkey, NULL);
    }

    return obj;
}

static void
seahorse_gpgme_photo_get_property (GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec)
{
    SeahorseGpgmePhoto *self = SEAHORSE_GPGME_PHOTO (object);

    switch (prop_id) {
    case PROP_PUBKEY:
        g_value_set_boxed (value, seahorse_gpgme_photo_get_pubkey (self));
        break;
    case PROP_INDEX:
        g_value_set_uint (value, seahorse_gpgme_photo_get_index (self));
        break;
    }
}

static void
seahorse_gpgme_photo_set_property (GObject      *object,
                                   unsigned int  prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
    SeahorseGpgmePhoto *self = SEAHORSE_GPGME_PHOTO (object);

    switch (prop_id) {
    case PROP_PUBKEY:
        g_return_if_fail (!self->pubkey);
        self->pubkey = g_value_get_boxed (value);
        if (self->pubkey)
            gpgme_key_ref (self->pubkey);
        break;
    case PROP_INDEX:
        seahorse_gpgme_photo_set_index (self, g_value_get_uint (value));
        break;
    }
}

static void
seahorse_gpgme_photo_finalize (GObject *gobject)
{
    SeahorseGpgmePhoto *self = SEAHORSE_GPGME_PHOTO (gobject);

    g_clear_pointer (&self->pubkey, gpgme_key_unref);

    G_OBJECT_CLASS (seahorse_gpgme_photo_parent_class)->finalize (gobject);
}

static void
seahorse_gpgme_photo_class_init (SeahorseGpgmePhotoClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->constructor = seahorse_gpgme_photo_constructor;
    gobject_class->finalize = seahorse_gpgme_photo_finalize;
    gobject_class->set_property = seahorse_gpgme_photo_set_property;
    gobject_class->get_property = seahorse_gpgme_photo_get_property;

    g_object_class_install_property (gobject_class, PROP_PUBKEY,
            g_param_spec_boxed ("pubkey", "Public Key", "GPGME Public Key that this subkey is on",
                                SEAHORSE_GPGME_BOXED_KEY, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class, PROP_INDEX,
            g_param_spec_uint ("index", "Index", "Index of photo UID",
                               0, G_MAXUINT, 0, G_PARAM_READWRITE));
}

/* -----------------------------------------------------------------------------
 * PUBLIC
 */

SeahorseGpgmePhoto*
seahorse_gpgme_photo_new (gpgme_key_t   pubkey,
                          GdkPixbuf    *pixbuf,
                          unsigned int  index)
{
    return g_object_new (SEAHORSE_TYPE_GPGME_PHOTO,
                         "pubkey", pubkey,
                         "pixbuf", pixbuf,
                         "index", index, NULL);
}

gpgme_key_t
seahorse_gpgme_photo_get_pubkey (SeahorseGpgmePhoto *self)
{
    g_return_val_if_fail (SEAHORSE_IS_GPGME_PHOTO (self), NULL);
    g_return_val_if_fail (self->pubkey, NULL);
    return self->pubkey;
}

guint
seahorse_gpgme_photo_get_index (SeahorseGpgmePhoto *self)
{
    g_return_val_if_fail (SEAHORSE_IS_GPGME_PHOTO (self), 0);
    return self->index;
}

void
seahorse_gpgme_photo_set_index (SeahorseGpgmePhoto *self, guint index)
{
    g_return_if_fail (SEAHORSE_IS_GPGME_PHOTO (self));

    self->index = index;
    g_object_notify (G_OBJECT (self), "index");
}
