/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#ifndef __SEAHORSE_GPGME_PHOTO_H__
#define __SEAHORSE_GPGME_PHOTO_H__

#include <glib-object.h>

#include <gpgme.h>

#include "pgp/seahorse-gpgme.h"
#include "pgp/seahorse-pgp-photo.h"

#define SEAHORSE_TYPE_GPGME_PHOTO            (seahorse_gpgme_photo_get_type ())
#define SEAHORSE_GPGME_PHOTO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GPGME_PHOTO, SeahorseGpgmePhoto))
#define SEAHORSE_GPGME_PHOTO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GPGME_PHOTO, SeahorseGpgmePhotoClass))
#define SEAHORSE_IS_GPGME_PHOTO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GPGME_PHOTO))
#define SEAHORSE_IS_GPGME_PHOTO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GPGME_PHOTO))
#define SEAHORSE_GPGME_PHOTO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GPGME_PHOTO, SeahorseGpgmePhotoClass))

typedef struct _SeahorseGpgmePhoto SeahorseGpgmePhoto;
typedef struct _SeahorseGpgmePhotoClass SeahorseGpgmePhotoClass;
typedef struct _SeahorseGpgmePhotoPrivate SeahorseGpgmePhotoPrivate;

struct _SeahorseGpgmePhoto {
	SeahorsePgpPhoto parent;
	SeahorseGpgmePhotoPrivate *pv;
};

struct _SeahorseGpgmePhotoClass {
	SeahorsePgpPhotoClass parent_class;
};

GType               seahorse_gpgme_photo_get_type        (void);

SeahorseGpgmePhoto* seahorse_gpgme_photo_new             (gpgme_key_t key,
                                                          GdkPixbuf *pixbuf,
                                                          guint index);

gpgme_key_t         seahorse_gpgme_photo_get_pubkey      (SeahorseGpgmePhoto *self);

guint               seahorse_gpgme_photo_get_index       (SeahorseGpgmePhoto *self);

void                seahorse_gpgme_photo_set_index       (SeahorseGpgmePhoto *self,
                                                          guint index);

#endif /* __SEAHORSE_GPGME_PHOTO_H__ */
