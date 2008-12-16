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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __SEAHORSE_PGP_PHOTO_H__
#define __SEAHORSE_PGP_PHOTO_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#define SEAHORSE_TYPE_PGP_PHOTO            (seahorse_pgp_photo_get_type ())
#define SEAHORSE_PGP_PHOTO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PGP_PHOTO, SeahorsePgpPhoto))
#define SEAHORSE_PGP_PHOTO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PGP_PHOTO, SeahorsePgpPhotoClass))
#define SEAHORSE_IS_PGP_PHOTO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PGP_PHOTO))
#define SEAHORSE_IS_PGP_PHOTO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PGP_PHOTO))
#define SEAHORSE_PGP_PHOTO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PGP_PHOTO, SeahorsePgpPhotoClass))

typedef struct _SeahorsePgpPhoto SeahorsePgpPhoto;
typedef struct _SeahorsePgpPhotoClass SeahorsePgpPhotoClass;
typedef struct _SeahorsePgpPhotoPrivate SeahorsePgpPhotoPrivate;

struct _SeahorsePgpPhoto {
	GObject parent;
	SeahorsePgpPhotoPrivate *pv;
};

struct _SeahorsePgpPhotoClass {
	GObjectClass parent_class;
};

GType               seahorse_pgp_photo_get_type          (void);

SeahorsePgpPhoto*   seahorse_pgp_photo_new               (GdkPixbuf *pixbuf);

GdkPixbuf*          seahorse_pgp_photo_get_pixbuf        (SeahorsePgpPhoto *self);

void                seahorse_pgp_photo_set_pixbuf        (SeahorsePgpPhoto *self,
                                                          GdkPixbuf *pixbuf);

#endif /* __SEAHORSE_PGP_PHOTO_H__ */
