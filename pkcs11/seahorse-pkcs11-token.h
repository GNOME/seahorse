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


#ifndef __SEAHORSE_PKCS11_TOKEN_H__
#define __SEAHORSE_PKCS11_TOKEN_H__

#include "seahorse-pkcs11-object.h"

#include <gck/gck.h>

#define SEAHORSE_TYPE_PKCS11_TOKEN            (seahorse_pkcs11_token_get_type ())
#define SEAHORSE_PKCS11_TOKEN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PKCS11_TOKEN, SeahorsePkcs11Token))
#define SEAHORSE_PKCS11_TOKEN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PKCS11_TOKEN, SeahorsePkcs11TokenClass))
#define SEAHORSE_IS_PKCS11_TOKEN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PKCS11_TOKEN))
#define SEAHORSE_IS_PKCS11_TOKEN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PKCS11_TOKEN))
#define SEAHORSE_PKCS11_TOKEN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PKCS11_TOKEN, SeahorsePkcs11TokenClass))

typedef struct _SeahorsePkcs11Token SeahorsePkcs11Token;
typedef struct _SeahorsePkcs11TokenClass SeahorsePkcs11TokenClass;
typedef struct _SeahorsePkcs11TokenPrivate SeahorsePkcs11TokenPrivate;

struct _SeahorsePkcs11Token {
	GObject parent;
	SeahorsePkcs11TokenPrivate *pv;
};

struct _SeahorsePkcs11TokenClass {
	GObjectClass parent_class;
};

GType                  seahorse_pkcs11_token_get_type          (void);

SeahorsePkcs11Token *  seahorse_pkcs11_token_new               (GckSlot *slot);

GckSlot *              seahorse_pkcs11_token_get_slot          (SeahorsePkcs11Token *self);

void                   seahorse_pkcs11_token_receive_object    (SeahorsePkcs11Token *self, GckObject *obj);

void                   seahorse_pkcs11_token_remove_object     (SeahorsePkcs11Token *self,
                                                                SeahorsePkcs11Object *object);

#endif /* __SEAHORSE_PKCS11_TOKEN_H__ */
