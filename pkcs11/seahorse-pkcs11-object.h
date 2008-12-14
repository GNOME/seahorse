/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#ifndef __SEAHORSE_PKCS11_OBJECT_H__
#define __SEAHORSE_PKCS11_OBJECT_H__

#include <gp11.h>

#include <glib-object.h>

#include "seahorse-object.h"

#define SEAHORSE_PKCS11_TYPE_OBJECT               (seahorse_pkcs11_object_get_type ())
#define SEAHORSE_PKCS11_OBJECT(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_PKCS11_TYPE_OBJECT, SeahorsePkcs11Object))
#define SEAHORSE_PKCS11_OBJECT_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_PKCS11_TYPE_OBJECT, SeahorsePkcs11ObjectClass))
#define SEAHORSE_PKCS11_IS_OBJECT(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_PKCS11_TYPE_OBJECT))
#define SEAHORSE_PKCS11_IS_OBJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_PKCS11_TYPE_OBJECT))
#define SEAHORSE_PKCS11_OBJECT_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_PKCS11_TYPE_OBJECT, SeahorsePkcs11ObjectClass))

typedef struct _SeahorsePkcs11Object SeahorsePkcs11Object;
typedef struct _SeahorsePkcs11ObjectClass SeahorsePkcs11ObjectClass;
typedef struct _SeahorsePkcs11ObjectPrivate SeahorsePkcs11ObjectPrivate;
    
struct _SeahorsePkcs11Object {
	SeahorseObject parent;
	SeahorsePkcs11ObjectPrivate *pv;
};

struct _SeahorsePkcs11ObjectClass {
	SeahorseObjectClass parent_class;
};

GType                       seahorse_pkcs11_object_get_type               (void);

SeahorsePkcs11Object*       seahorse_pkcs11_object_new                    (GP11Object* object);

gulong                      seahorse_pkcs11_object_get_pkcs11_handle      (SeahorsePkcs11Object* self);

GP11Object*                 seahorse_pkcs11_object_get_pkcs11_object      (SeahorsePkcs11Object* self);

GP11Attributes*             seahorse_pkcs11_object_get_pkcs11_attributes  (SeahorsePkcs11Object* self);

void                        seahorse_pkcs11_object_set_pkcs11_attributes  (SeahorsePkcs11Object* self, 
                                                                           GP11Attributes* value);

GP11Attribute*              seahorse_pkcs11_object_require_attribute      (SeahorsePkcs11Object *self,
                                                                           gulong attr_type);

gboolean                    seahorse_pkcs11_object_require_attributes     (SeahorsePkcs11Object *self,
                                                                           const gulong *attr_types,
                                                                           gsize n_attr_types);

GQuark                      seahorse_pkcs11_object_cannonical_id          (GP11Object *object);

#endif /* __SEAHORSE_PKCS11_OBJECT_H__ */
