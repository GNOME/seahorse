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

#ifndef __SEAHORSE_PKCS11_PROPERTIES_H__
#define __SEAHORSE_PKCS11_PROPERTIES_H__

#include <gcr/gcr.h>

#include <gtk/gtk.h>

#include <glib-object.h>

#define SEAHORSE_TYPE_PKCS11_PROPERTIES               (seahorse_pkcs11_properties_get_type ())
#define SEAHORSE_PKCS11_PROPERTIES(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PKCS11_PROPERTIES, SeahorsePkcs11Properties))
#define SEAHORSE_IS_PKCS11_PROPERTIES(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PKCS11_PROPERTIES))

typedef struct _SeahorsePkcs11Properties SeahorsePkcs11Properties;

GType            seahorse_pkcs11_properties_get_type            (void);

GtkWindow *      seahorse_pkcs11_properties_new                 (GObject *object,
                                                                 GtkWindow *parent);

GObject *        seahorse_pkcs11_properties_get_object          (SeahorsePkcs11Properties *self);

#endif /* __SEAHORSE_PKCS11_PROPERTIES_H__ */
