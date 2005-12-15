/* 
 * Seahorse
 * 
 * Copyright (C) 2005 Nate Nielsen 
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
   
#ifndef __CRYPTUI_H__
#define __CRYPTUI_H__

#include <glib.h>

typedef enum _CryptUIEncType {
    CRYPTUI_ENCTYPE_NONE,
    CRYPTUI_ENCTYPE_SYMMETRIC,
    CRYPTUI_ENCTYPE_PUBLIC,
    CRYPTUI_ENCTYPE_PRIVATE,
    
    /* Used internally */
    _CRYPTUI_ENCTYPE_MAXVALUE
} CryptUIEncType;

/* 
 * Key Properties:
 * 
 * display-name: G_TYPE_STRING
 * display-id:   G_TYPE_STRING 
 * enc-type:     G_TYPE_UINT (CryptUIEncType)
 * 
 * TODO: Flesh this list out 
 */

gchar*              cryptui_key_get_base (const gchar *key);

CryptUIEncType      cryptui_key_get_enctype (const gchar *key);



#endif /* __CRYPT_UI_H__ */
