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


#ifndef __SEAHORSE_PKCS11_SOURCE_H__
#define __SEAHORSE_PKCS11_SOURCE_H__

#include "seahorse-source.h"

#include <gck/gck.h>

#define SEAHORSE_TYPE_PKCS11_SOURCE            (seahorse_pkcs11_source_get_type ())
#define SEAHORSE_PKCS11_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PKCS11_SOURCE, SeahorsePkcs11Source))
#define SEAHORSE_PKCS11_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PKCS11_SOURCE, SeahorsePkcs11SourceClass))
#define SEAHORSE_IS_PKCS11_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PKCS11_SOURCE))
#define SEAHORSE_IS_PKCS11_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PKCS11_SOURCE))
#define SEAHORSE_PKCS11_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PKCS11_SOURCE, SeahorsePkcs11SourceClass))

typedef struct _SeahorsePkcs11Source SeahorsePkcs11Source;
typedef struct _SeahorsePkcs11SourceClass SeahorsePkcs11SourceClass;
typedef struct _SeahorsePkcs11SourcePrivate SeahorsePkcs11SourcePrivate;

struct _SeahorsePkcs11Source {
	GObject parent;
	SeahorsePkcs11SourcePrivate *pv;
};

struct _SeahorsePkcs11SourceClass {
	GObjectClass parent_class;
};

GType                  seahorse_pkcs11_source_get_type          (void);

SeahorsePkcs11Source*  seahorse_pkcs11_source_new               (GckSlot *slot);

GckSlot*               seahorse_pkcs11_source_get_slot          (SeahorsePkcs11Source *self);

void                   seahorse_pkcs11_source_receive_object    (SeahorsePkcs11Source *self, GckObject *obj);

#endif /* __SEAHORSE_PKCS11_SOURCE_H__ */
