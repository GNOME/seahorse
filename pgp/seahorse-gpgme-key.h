/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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

#ifndef __SEAHORSE_GPGME_KEY_H__
#define __SEAHORSE_GPGME_KEY_H__

#include <glib-object.h>

#include <gpgme.h>

#include "seahorse-pgp-key.h"

#define SEAHORSE_TYPE_GPGME_KEY            (seahorse_gpgme_key_get_type ())
#define SEAHORSE_GPGME_KEY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GPGME_KEY, SeahorseGpgmeKey))
#define SEAHORSE_GPGME_KEY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GPGME_KEY, SeahorseGpgmeKeyClass))
#define SEAHORSE_IS_GPGME_KEY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GPGME_KEY))
#define SEAHORSE_IS_GPGME_KEY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GPGME_KEY))
#define SEAHORSE_GPGME_KEY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GPGME_KEY, SeahorseGpgmeKeyClass))


typedef struct _SeahorseGpgmeKey SeahorseGpgmeKey;
typedef struct _SeahorseGpgmeKeyClass SeahorseGpgmeKeyClass;
typedef struct _SeahorseGpgmeKeyPrivate SeahorseGpgmeKeyPrivate;

struct _SeahorseGpgmeKey {
	SeahorsePgpKey parent;
	SeahorseGpgmeKeyPrivate *pv;
};

struct _SeahorseGpgmeKeyClass {
	SeahorsePgpKeyClass         parent_class;
};

SeahorseGpgmeKey* seahorse_gpgme_key_new                 (SeahorsePlace *sksrc,
                                                          gpgme_key_t pubkey,
                                                          gpgme_key_t seckey);

void              seahorse_gpgme_key_refresh              (SeahorseGpgmeKey *self);

void              seahorse_gpgme_key_realize              (SeahorseGpgmeKey *self);

GType             seahorse_gpgme_key_get_type             (void);

gpgme_key_t       seahorse_gpgme_key_get_public           (SeahorseGpgmeKey *self);

void              seahorse_gpgme_key_set_public           (SeahorseGpgmeKey *self,
                                                           gpgme_key_t key);

gpgme_key_t       seahorse_gpgme_key_get_private          (SeahorseGpgmeKey *self);

void              seahorse_gpgme_key_set_private          (SeahorseGpgmeKey *self,
                                                           gpgme_key_t key);

void              seahorse_gpgme_key_refresh_matching     (gpgme_key_t key);

SeahorseValidity  seahorse_gpgme_key_get_validity         (SeahorseGpgmeKey *self);

SeahorseValidity  seahorse_gpgme_key_get_trust            (SeahorseGpgmeKey *self);

#endif /* __SEAHORSE_KEY_H__ */
