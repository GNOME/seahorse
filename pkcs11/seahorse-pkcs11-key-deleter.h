/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef __SEAHORSE_PKCS11_KEY_DELETER_H__
#define __SEAHORSE_PKCS11_KEY_DELETER_H__

#include <glib-object.h>

#include "seahorse-common.h"
#include "seahorse-certificate.h"

#define SEAHORSE_TYPE_PKCS11_KEY_DELETER       (seahorse_pkcs11_key_deleter_get_type ())
#define SEAHORSE_PKCS11_KEY_DELETER(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PKCS11_KEY_DELETER, SeahorsePkcs11KeyDeleter))
#define SEAHORSE_IS_PKCS11_KEY_DELETER(obj)    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PKCS11_KEY_DELETER))

typedef struct _SeahorsePkcs11KeyDeleter SeahorsePkcs11KeyDeleter;

GType              seahorse_pkcs11_key_deleter_get_type   (void) G_GNUC_CONST;

SeahorseDeleter *  seahorse_pkcs11_key_deleter_new        (GObject *cert_or_key);

#endif /* __SEAHORSE_PKCS11_KEY_DELETER_H__ */
