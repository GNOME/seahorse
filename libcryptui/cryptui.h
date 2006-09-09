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

#ifndef LIBCRYPTUI_API_SUBJECT_TO_CHANGE
#error "Please define LIBCRYPTUI_API_SUBJECT_TO_CHANGE to acknowledge your understanding that this API is not yet stable."
#endif

#include "cryptui-keyset.h"

typedef enum {
    CRYPTUI_ENCTYPE_NONE =       0,
    CRYPTUI_ENCTYPE_SYMMETRIC =  1,
    CRYPTUI_ENCTYPE_PUBLIC =     2,
    CRYPTUI_ENCTYPE_PRIVATE =    3,
    
    /* Used internally */
    _CRYPTUI_ENCTYPE_MAXVALUE
} CryptUIEncType;

typedef enum {
    CRYPTUI_FLAG_IS_VALID =    0x0001,
    CRYPTUI_FLAG_CAN_ENCRYPT = 0x0002,
    CRYPTUI_FLAG_CAN_SIGN =    0x0004,
    CRYPTUI_FLAG_EXPIRED =     0x0100,
    CRYPTUI_FLAG_REVOKED =     0x0200,
    CRYPTUI_FLAG_DISABLED =    0x0400,
    CRYPTUI_FLAG_TRUSTED =     0x1000
} CryptUIKeyFlags;

typedef enum {
    CRYPTUI_LOC_INVALID =        0,    /* An invalid key */
    CRYPTUI_LOC_MISSING =       10,    /* A key we don't know anything about */
    CRYPTUI_LOC_SEARCHING =     20,    /* A key we're searching for but haven't found yet */
    CRYPTUI_LOC_REMOTE =        50,    /* A key that we've found is present remotely */
    CRYPTUI_LOC_LOCAL =        100,    /* A key on the local machine */
} CryptUILocation;

typedef enum {
    CRYPTUI_VALIDITY_REVOKED =   -3,
    CRYPTUI_VALIDITY_DISABLED =  -2,
    CRYPTUI_VALIDITY_NEVER =     -1,
    CRYPTUI_VALIDITY_UNKNOWN =    0,
    CRYPTUI_VALIDITY_MARGINAL =   1,
    CRYPTUI_VALIDITY_FULL =       5,
    CRYPTUI_VALIDITY_ULTIMATE =  10
} CryptUIValidity;

/* 
 * Key Properties:
 * 
 * display-name: G_TYPE_STRING
 * display-id:   G_TYPE_STRING 
 * enc-type:     G_TYPE_UINT (CryptUIEncType)
 * 
 * 
 * TODO: Flesh this list out 
 */

gchar*              cryptui_key_get_base (const gchar *key);

CryptUIEncType      cryptui_key_get_enctype (const gchar *key);

void                cryptui_display_notification (const gchar *title, const gchar *body,
                                                  const gchar *icon, gboolean urgent);

gchar**             cryptui_prompt_recipients (CryptUIKeyset *keyset, 
                                               const gchar *title, gchar **signer);

gchar*              cryptui_prompt_signer (CryptUIKeyset *keyset, const gchar *title);

#endif /* __CRYPT_UI_H__ */
