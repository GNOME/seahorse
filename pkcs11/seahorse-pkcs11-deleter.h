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

#ifndef __SEAHORSE_PKCS11_DELETER_H__
#define __SEAHORSE_PKCS11_DELETER_H__

#include <glib-object.h>

#include "seahorse-common.h"
#include "seahorse-certificate.h"

#define SEAHORSE_TYPE_PKCS11_DELETER       (seahorse_pkcs11_deleter_get_type ())
#define SEAHORSE_PKCS11_DELETER(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PKCS11_DELETER, SeahorsePkcs11Deleter))
#define SEAHORSE_IS_PKCS11_DELETER(obj)    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PKCS11_DELETER))
#define SEAHORSE_PKCS11_DELETER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PKCS11_DELETER, SeahorsePkcs11DeleterClass))
#define SEAHORSE_IS_PKCS11_DELETER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PKCS11_DELETER))
#define SEAHORSE_PKCS11_DELETER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PKCS11_DELETER, SeahorsePkcs11DeleterClass))

typedef struct _SeahorsePkcs11Deleter SeahorsePkcs11Deleter;
typedef struct _SeahorsePkcs11DeleterClass SeahorsePkcs11DeleterClass;

struct _SeahorsePkcs11Deleter {
	SeahorseDeleter parent;
	GList *objects;
};

struct _SeahorsePkcs11DeleterClass {
	SeahorseDeleterClass parent_class;
};

GType              seahorse_pkcs11_deleter_get_type   (void) G_GNUC_CONST;

SeahorseDeleter *  seahorse_pkcs11_deleter_new        (SeahorseCertificate *cert);

#endif /* __SEAHORSE_PKCS11_DELETER_H__ */
