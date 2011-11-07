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

#ifndef __SEAHORSE_PKCS11_PROPERTIES_H__
#define __SEAHORSE_PKCS11_PROPERTIES_H__

#include <gcr/gcr.h>

#include <gtk/gtk.h>

#include <glib-object.h>
#include <glib/gi18n.h>

#define SEAHORSE_TYPE_PKCS11_PROPERTIES               (seahorse_pkcs11_properties_get_type ())
#define SEAHORSE_PKCS11_PROPERTIES(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PKCS11_PROPERTIES, SeahorsePkcs11Properties))
#define SEAHORSE_IS_PKCS11_PROPERTIES(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PKCS11_PROPERTIES))

typedef struct _SeahorsePkcs11Properties SeahorsePkcs11Properties;

GType            seahorse_pkcs11_properties_get_type            (void);

GtkWindow *      seahorse_pkcs11_properties_show                (GObject *object,
                                                                 GtkWindow *parent);

GObject *        seahorse_pkcs11_properties_get_object          (SeahorsePkcs11Properties *self);

#endif /* __SEAHORSE_PKCS11_PROPERTIES_H__ */
