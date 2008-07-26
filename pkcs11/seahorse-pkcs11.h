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

#ifndef __SEAHORSE_PKCS11_H__
#define __SEAHORSE_PKCS11_H__

#include <glib.h>
#include <glib-object.h>
#include <gp11.h>

G_BEGIN_DECLS



#define SEAHORSE_PKCS11_TYPE_STR "pkcs11"
#define SEAHORSE_PKCS11_TYPE g_quark_from_string ("pkcs11")
GQuark seahorse_pkcs11_id_from_attributes (GP11Attributes* attrs);
gboolean seahorse_pkcs11_id_to_attributes (GQuark id, GP11Attributes* attrs);


G_END_DECLS

#endif
