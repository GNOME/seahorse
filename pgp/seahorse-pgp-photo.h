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

#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

#define SEAHORSE_PGP_TYPE_PHOTO            (seahorse_pgp_photo_get_type ())
G_DECLARE_DERIVABLE_TYPE (SeahorsePgpPhoto, seahorse_pgp_photo,
                          SEAHORSE_PGP, PHOTO,
                          GObject);

struct _SeahorsePgpPhotoClass {
    GObjectClass parent_class;
};

SeahorsePgpPhoto*   seahorse_pgp_photo_new               (GdkPixbuf *pixbuf);

GdkPixbuf*          seahorse_pgp_photo_get_pixbuf        (SeahorsePgpPhoto *self);

void                seahorse_pgp_photo_set_pixbuf        (SeahorsePgpPhoto *self,
                                                          GdkPixbuf *pixbuf);
