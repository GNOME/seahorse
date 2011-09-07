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

#ifdef WITH_PKCS11

#include <glib.h>

#include <gcr/gcr.h>

#define SEAHORSE_PKCS11_TYPE_STR "pkcs11"
#define SEAHORSE_PKCS11_TYPE g_quark_from_string ("pkcs11")

GcrCollection *       seahorse_pkcs11_backend_initialize    (void);

#endif /* WITH_PKCS11 */

#endif
