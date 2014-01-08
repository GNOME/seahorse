/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#ifndef __SEAHORSE_PRIVATE_KEY_H__
#define __SEAHORSE_PRIVATE_KEY_H__

#include <gck/gck.h>

#include <glib-object.h>

#include "seahorse-certificate.h"

#define SEAHORSE_TYPE_PRIVATE_KEY               (seahorse_private_key_get_type ())
#define SEAHORSE_PRIVATE_KEY(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PRIVATE_KEY, SeahorsePrivateKey))
#define SEAHORSE_PRIVATE_KEY_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PRIVATE_KEY, SeahorsePrivateKeyClass))
#define SEAHORSE_IS_PRIVATE_KEY(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PRIVATE_KEY))
#define SEAHORSE_IS_PRIVATE_KEY_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PRIVATE_KEY))
#define SEAHORSE_PRIVATE_KEY_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PRIVATE_KEY, SeahorsePrivateKeyClass))

typedef struct _SeahorsePrivateKeyClass SeahorsePrivateKeyClass;
typedef struct _SeahorsePrivateKeyPrivate SeahorsePrivateKeyPrivate;

struct _SeahorsePrivateKey {
	GckObject parent;
	SeahorsePrivateKeyPrivate *pv;
};

struct _SeahorsePrivateKeyClass {
	GckObjectClass parent_class;
};

GType                 seahorse_private_key_get_type           (void) G_GNUC_CONST;

SeahorseCertificate * seahorse_private_key_get_partner        (SeahorsePrivateKey *self);

void                  seahorse_private_key_set_partner        (SeahorsePrivateKey *self,
                                                               SeahorseCertificate *certificate);

GIcon *               seahorse_private_key_get_icon           (SeahorsePrivateKey *self);

#endif /* __SEAHORSE_PRIVATE_KEY_H__ */
