/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef __SEAHORSE_GPGME_KEY_DELETER_H__
#define __SEAHORSE_GPGME_KEY_DELETER_H__

#include <glib-object.h>

#include "seahorse-common.h"
#include "seahorse-gpgme-key.h"

#define SEAHORSE_TYPE_GPGME_KEY_DELETER       (seahorse_gpgme_key_deleter_get_type ())
#define SEAHORSE_GPGME_KEY_DELETER(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GPGME_KEY_DELETER, SeahorseGpgmeKeyDeleter))
#define SEAHORSE_IS_GPGME_KEY_DELETER(obj)    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GPGME_KEY_DELETER))

typedef struct _SeahorseGpgmeKeyDeleter SeahorseGpgmeKeyDeleter;

GType              seahorse_gpgme_key_deleter_get_type   (void) G_GNUC_CONST;

SeahorseDeleter *  seahorse_gpgme_key_deleter_new        (SeahorseGpgmeKey *key);

#endif /* __SEAHORSE_GPGME_KEY_DELETER_H__ */
