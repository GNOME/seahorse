/*
 * Seahorse
 *
 * Copyright (C) 2025 Niels De Graef
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

#include "seahorse-pgp-photos-widget.h"
#include "seahorse-gpgme.h"
#include "seahorse-gpgme-dialogs.h"
#include "seahorse-gpgme-key.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-gpgme-photo.h"

#include "seahorse-common.h"

#include <glib.h>
#include <glib/gi18n.h>

enum {
    PROP_KEY = 1,
    N_PROPS
};
static GParamSpec *properties[N_PROPS] = { NULL, };

struct _SeahorsePgpPhotosWidget {
    SeahorsePanel parent_instance;

    SeahorsePgpKey *key;

    GtkWidget *carousel;
};

G_DEFINE_TYPE (SeahorsePgpPhotosWidget,
               seahorse_pgp_photos_widget,
               GTK_TYPE_WIDGET)

static SeahorsePgpPhoto *
get_current_photo (SeahorsePgpPhotosWidget *self)
{
    GListModel *photos;
    unsigned int pos;

    photos = seahorse_pgp_key_get_photos (self->key);
    pos = (unsigned int) adw_carousel_get_position (ADW_CAROUSEL (self->carousel));

    /* this will return a strong ref or NULL */
    return g_list_model_get_item (photos, pos);
}

static GtkWindow *
get_toplevel (SeahorsePgpPhotosWidget *self)
{
    GtkRoot *root;

    root = gtk_widget_get_root (GTK_WIDGET (self));
    return GTK_IS_WINDOW (root)? GTK_WINDOW (root) : NULL;
}

static void
on_photo_added (GObject      *source_object,
                GAsyncResult *result,
                void         *user_data)
{
    SeahorsePgpPhotosWidget *self = SEAHORSE_PGP_PHOTOS_WIDGET (user_data);
    SeahorseGpgmeKey *key = SEAHORSE_GPGME_KEY (self->key);
    g_autoptr(GError) error = NULL;

    if (!seahorse_gpgme_photo_add_finish (key, result, &error)) {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            return;
        seahorse_util_show_error (GTK_WIDGET (self), _("Couldn't add photo"), NULL);
    }
}

static void
add_photo_action (GtkWidget  *widget,
                  const char *action_name,
                  GVariant   *param)
{
    SeahorsePgpPhotosWidget *self = SEAHORSE_PGP_PHOTOS_WIDGET (widget);

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self->key));

    seahorse_gpgme_photo_add (SEAHORSE_GPGME_KEY (self->key),
                              get_toplevel (self),
                              NULL,
                              on_photo_added,
                              self);
}

static void
remove_photo_action (GtkWidget  *widget,
                     const char *action_name,
                     GVariant   *param)
{
    SeahorsePgpPhotosWidget *self = SEAHORSE_PGP_PHOTOS_WIDGET (widget);
    g_autoptr(SeahorsePgpPhoto) photo = NULL;

    photo = get_current_photo (self);
    g_return_if_fail (SEAHORSE_IS_GPGME_PHOTO (photo));

    seahorse_gpgme_key_op_photo_delete (SEAHORSE_GPGME_PHOTO (photo));
}

static void
set_primary_photo_action (GtkWidget  *widget,
                          const char *action_name,
                          GVariant   *param)
{
    SeahorsePgpPhotosWidget *self = SEAHORSE_PGP_PHOTOS_WIDGET (widget);
    g_autoptr(SeahorsePgpPhoto) photo = NULL;
    gpgme_error_t gerr;

    photo = get_current_photo (self);
    g_return_if_fail (SEAHORSE_IS_GPGME_PHOTO (photo));

    gerr = seahorse_gpgme_key_op_photo_primary (SEAHORSE_GPGME_PHOTO (photo));
    if (!GPG_IS_OK (gerr))
        seahorse_gpgme_handle_error (gerr, _("Couldn’t change primary photo"));
}

static GtkWidget *
create_widget_for_photo (SeahorsePgpPhoto *photo)
{
    GdkPixbuf *pixbuf;
    g_autoptr(GdkTexture) texture = NULL;
    GtkWidget *picture;

    pixbuf = seahorse_pgp_photo_get_pixbuf (photo);
    g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

    texture = gdk_texture_new_for_pixbuf (pixbuf);
    picture = gtk_picture_new_for_paintable (GDK_PAINTABLE (texture));
    gtk_widget_set_size_request (picture, 84, 84);

    return picture;
}

static void
seahorse_pgp_photos_widget_get_property (GObject      *object,
                                         unsigned int  prop_id,
                                         GValue       *value,
                                         GParamSpec   *pspec)
{
    SeahorsePgpPhotosWidget *self = SEAHORSE_PGP_PHOTOS_WIDGET (object);

    switch (prop_id) {
    case PROP_KEY:
        g_value_set_object (value, self->key);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_pgp_photos_widget_set_property (GObject      *object,
                                         unsigned int  prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
    SeahorsePgpPhotosWidget *self = SEAHORSE_PGP_PHOTOS_WIDGET (object);

    switch (prop_id) {
    case PROP_KEY:
        g_set_object (&self->key, g_value_get_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_pgp_photos_widget_finalize (GObject *obj)
{
    SeahorsePgpPhotosWidget *self = SEAHORSE_PGP_PHOTOS_WIDGET (obj);

    g_clear_object (&self->key);

    G_OBJECT_CLASS (seahorse_pgp_photos_widget_parent_class)->finalize (obj);
}

static void
seahorse_pgp_photos_widget_dispose (GObject *obj)
{
    SeahorsePgpPhotosWidget *self = SEAHORSE_PGP_PHOTOS_WIDGET (obj);
    GtkWidget *child = NULL;

    while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))) != NULL)
        gtk_widget_unparent (child);

    G_OBJECT_CLASS (seahorse_pgp_photos_widget_parent_class)->dispose (obj);
}

static void
seahorse_pgp_photos_widget_constructed (GObject *obj)
{
    SeahorsePgpPhotosWidget *self = SEAHORSE_PGP_PHOTOS_WIDGET (obj);
    GListModel *photos;
    unsigned int n_photos;

    G_OBJECT_CLASS (seahorse_pgp_photos_widget_parent_class)->constructed (obj);

    photos = seahorse_pgp_key_get_photos (self->key);
    n_photos = g_list_model_get_n_items (photos);

    gtk_widget_set_visible (self->carousel, n_photos > 0);

    if (seahorse_pgp_key_is_private_key (self->key)) {
        GtkWidget *w = GTK_WIDGET (self);
        gboolean is_gpgme = SEAHORSE_GPGME_IS_KEY (self->key);

        gtk_widget_action_set_enabled (w, "add-photo", is_gpgme);
        gtk_widget_action_set_enabled (w, "set-primary-photo",
                                       is_gpgme && n_photos > 1);
        gtk_widget_action_set_enabled (w, "remove-photo",
                                       is_gpgme && n_photos > 0);
    }

    /* Change sensitivity if first/last photo id */
    for (unsigned int i = 0; i < n_photos; i++) {
        g_autoptr(SeahorsePgpPhoto) photo = NULL;
        GtkWidget *page;

        photo = g_list_model_get_item (G_LIST_MODEL (photos), i);
        page = create_widget_for_photo (photo);
        adw_carousel_append (ADW_CAROUSEL (self->carousel), page);
    }
}

static void
seahorse_pgp_photos_widget_init (SeahorsePgpPhotosWidget *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}

static void
seahorse_pgp_photos_widget_class_init (SeahorsePgpPhotosWidgetClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->constructed = seahorse_pgp_photos_widget_constructed;
    gobject_class->get_property = seahorse_pgp_photos_widget_get_property;
    gobject_class->set_property = seahorse_pgp_photos_widget_set_property;
    gobject_class->dispose = seahorse_pgp_photos_widget_dispose;
    gobject_class->finalize = seahorse_pgp_photos_widget_finalize;

    properties[PROP_KEY] =
        g_param_spec_object ("key", NULL, NULL,
                             SEAHORSE_PGP_TYPE_KEY,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (gobject_class, N_PROPS, properties);

    gtk_widget_class_install_action (widget_class, "add-photo", NULL, add_photo_action);
    gtk_widget_class_install_action (widget_class, "remove-photo", NULL, remove_photo_action);
    gtk_widget_class_install_action (widget_class, "set-primary-photo", NULL, set_primary_photo_action);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/Seahorse/seahorse-pgp-photos-widget.ui");
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorsePgpPhotosWidget,
                                          carousel);
}

GtkWidget *
seahorse_pgp_photos_widget_new (SeahorsePgpKey *key)
{
    g_autoptr(SeahorsePgpPhotosWidget) self = NULL;

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (key), NULL);

    self = g_object_new (SEAHORSE_PGP_TYPE_PHOTOS_WIDGET,
                         "key", key,
                         NULL);

    return GTK_WIDGET (g_steal_pointer (&self));
}
