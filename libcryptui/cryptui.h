/* 
 * Seahorse
 * 
 * Copyright (C) 2005 Stefan Walter
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

G_BEGIN_DECLS

#ifndef LIBCRYPTUI_API_SUBJECT_TO_CHANGE
#error "Please define LIBCRYPTUI_API_SUBJECT_TO_CHANGE to acknowledge your understanding that this API is not yet stable."
#endif

#include "cryptui-keyset.h"

/**
 * CryptUIEncType:
 * @CRYPTUI_ENCTYPE_NONE: the key is none of the below types
 * @CRYPTUI_ENCTYPE_SYMMETRIC: the key's type is symmetric
 * @CRYPTUI_ENCTYPE_PUBLIC: the key's type is public
 * @CRYPTUI_ENCTYPE_PRIVATE: the key's type is private
 * @_CRYPTUI_ENCTYPE_MAXVALUE: for internal use only
 *
 * Used in key properties to indicate what kind of key it is
 *
 * Never ever change a values already present. These values are used
 * across applications, DBus etc... 
 */
typedef enum {
    CRYPTUI_ENCTYPE_NONE =       0,
    CRYPTUI_ENCTYPE_SYMMETRIC =  1,
    CRYPTUI_ENCTYPE_PUBLIC =     2,
    CRYPTUI_ENCTYPE_PRIVATE =    3,
    
    /* Used internally */
    CRYPTUI_ENCTYPE_MAXVALUE
} CryptUIEncType;

/**
 * CryptUIKeyFlags:
 * @CRYPTUI_FLAG_IS_VALID: whether the key is valid
 * @CRYPTUI_FLAG_CAN_ENCRYPT: whether the key can encrypt
 * @CRYPTUI_FLAG_CAN_SIGN: whether the key can sign
 * @CRYPTUI_FLAG_EXPIRED: whether the key is expired
 * @CRYPTUI_FLAG_REVOKED: whether the key is revoked
 * @CRYPTUI_FLAG_DISABLED: whether the key is disabled
 * @CRYPTUI_FLAG_TRUSTED: whether the key is trusted
 * @CRYPTUI_FLAG_EXPORTABLE: whether the key is exportable
 *
 * Used in key properties to indicate a key's status.  These values are
 * logically OR'd together.
 *
 * Never ever change a values already present. These values are used
 * across applications, DBus etc...
 */
typedef enum {
    CRYPTUI_FLAG_IS_VALID =    0x00000001,
    CRYPTUI_FLAG_CAN_ENCRYPT = 0x00000002,
    CRYPTUI_FLAG_CAN_SIGN =    0x00000004,
    CRYPTUI_FLAG_EXPIRED =     0x00000100,
    CRYPTUI_FLAG_REVOKED =     0x00000200,
    CRYPTUI_FLAG_DISABLED =    0x00000400,
    CRYPTUI_FLAG_TRUSTED =     0x00001000,
    CRYPTUI_FLAG_EXPORTABLE =  0x00100000
} CryptUIKeyFlags;

/**
 * CryptUILocation:
 * @CRYPTUI_LOC_INVALID: An invalid key
 * @CRYPTUI_LOC_MISSING: A key we don't know anything about
 * @CRYPTUI_LOC_SEARCHING: A key we're searching for but haven't found yet
 * @CRYPTUI_LOC_REMOTE: A key that we've found is present remotely
 * @CRYPTUI_LOC_LOCAL: A key on the local machine
 *
 * Indicates where a key is located.
 *
 * Never ever change a values already present. These values are used
 * across applications, DBus etc...
 */
typedef enum {
    CRYPTUI_LOC_INVALID =        0,
    CRYPTUI_LOC_MISSING =       10,
    CRYPTUI_LOC_SEARCHING =     20,
    CRYPTUI_LOC_REMOTE =        50,
    CRYPTUI_LOC_LOCAL =        100,
} CryptUILocation;

/**
 * CryptUIValidity:
 * @CRYPTUI_VALIDITY_REVOKED: the key is revoked
 * @CRYPTUI_VALIDITY_DISABLED: the key is disabled
 * @CRYPTUI_VALIDITY_NEVER: the key has never been valid
 * @CRYPTUI_VALIDITY_UNKNOWN: the key's validity is unknown
 * @CRYPTUI_VALIDITY_MARGINAL: the key's validity is marginal
 * @CRYPTUI_VALIDITY_FULL: the key's validity is full
 * @CRYPTUI_VALIDITY_ULTIMATE: the key's validity is ultimate
 *
 * What the validity of the key is.
 *
 * Never ever change a values already present. These values are used
 * across applications, DBus etc... 
 */
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

/*
 * This function creates a dialog when a consumer of the library needs the user to 
 * create or import at least one key for the application to perform the necessary
 * action.  It will inform the user of the need to create or import a key and launch
 * the seahorse (Passwords and Encryption Keys) application so that they may do so.
 * 
 * quit: whether to run gtk_main_quit () after launching seahorse
 */
void                cryptui_need_to_get_keys ();

G_END_DECLS

#endif /* __CRYPT_UI_H__ */
