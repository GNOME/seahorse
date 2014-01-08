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

#ifndef __SEAHORSE_GPGME_SUBKEY_H__
#define __SEAHORSE_GPGME_SUBKEY_H__

#include <glib-object.h>

#include <gpgme.h>

#include "pgp/seahorse-pgp-subkey.h"

#define SEAHORSE_TYPE_GPGME_SUBKEY            (seahorse_gpgme_subkey_get_type ())
#define SEAHORSE_GPGME_SUBKEY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GPGME_SUBKEY, SeahorseGpgmeSubkey))
#define SEAHORSE_GPGME_SUBKEY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GPGME_SUBKEY, SeahorseGpgmeSubkeyClass))
#define SEAHORSE_IS_GPGME_SUBKEY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GPGME_SUBKEY))
#define SEAHORSE_IS_GPGME_SUBKEY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GPGME_SUBKEY))
#define SEAHORSE_GPGME_SUBKEY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GPGME_SUBKEY, SeahorseGpgmeSubkeyClass))

typedef struct _SeahorseGpgmeSubkey SeahorseGpgmeSubkey;
typedef struct _SeahorseGpgmeSubkeyClass SeahorseGpgmeSubkeyClass;
typedef struct _SeahorseGpgmeSubkeyPrivate SeahorseGpgmeSubkeyPrivate;

struct _SeahorseGpgmeSubkey {
	SeahorsePgpSubkey parent;
	SeahorseGpgmeSubkeyPrivate *pv;
};

struct _SeahorseGpgmeSubkeyClass {
	SeahorsePgpSubkeyClass parent_class;
};

GType                 seahorse_gpgme_subkey_get_type          (void);

SeahorseGpgmeSubkey*  seahorse_gpgme_subkey_new               (gpgme_key_t pubkey,
                                                               gpgme_subkey_t subkey);

gpgme_key_t           seahorse_gpgme_subkey_get_pubkey        (SeahorseGpgmeSubkey *self);

gpgme_subkey_t        seahorse_gpgme_subkey_get_subkey        (SeahorseGpgmeSubkey *self);

void                  seahorse_gpgme_subkey_set_subkey        (SeahorseGpgmeSubkey *self,
                                                               gpgme_subkey_t subkey);

#endif /* __SEAHORSE_GPGME_SUBKEY_H__ */
