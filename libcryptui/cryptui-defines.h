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
 
#ifndef __CRYPTUI_DEFINES_H__
#define __CRYPTUI_DEFINES_H__

/* 
 * Declarations that are shared between libcryptui and libseahorse, and used
 * internally by both. 
 */

#define SEAHORSE_DESKTOP_KEYS           "/desktop/pgp"
#define SEAHORSE_DEFAULT_KEY            SEAHORSE_DESKTOP_KEYS "/default_key"
#define SEAHORSE_LASTSIGNER_KEY         SEAHORSE_DESKTOP_KEYS "/last_signer"
#define SEAHORSE_ENCRYPTSELF_KEY        SEAHORSE_DESKTOP_KEYS "/encrypt_to_self"
#define SEAHORSE_RECIPIENTS_SORT_KEY    SEAHORSE_DESKTOP_KEYS "/recipients/sort_by"

#endif /* __CRYPTUI_DEFINES_H__ */
