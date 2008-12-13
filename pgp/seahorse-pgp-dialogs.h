/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Stefan Walter
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

/*
 * Various UI elements and dialogs used in pgp component.
 */
 
#ifndef __SEAHORSE_PGP_DIALOGS_H__
#define __SEAHORSE_PGP_DIALOGS_H__

#include <glib.h>

#include "pgp/seahorse-pgp-key.h"
#include "pgp/seahorse-pgp-photo.h"
#include "pgp/seahorse-pgp-subkey.h"
#include "pgp/seahorse-pgp-uid.h"

void            seahorse_pgp_sign_prompt           (SeahorsePgpKey *key,
                                                    GtkWindow *parent);

void            seahorse_pgp_sign_prompt_uid       (SeahorsePgpUid *uid,
                                                    GtkWindow *parent);

SeahorsePgpKey* seahorse_signer_get                 (GtkWindow *parent);

void            seahorse_pgp_handle_gpgme_error     (gpgme_error_t err, 
                                                     const gchar* desc, ...);

void            seahorse_pgp_key_properties_show    (SeahorsePgpKey *pkey,
                                                     GtkWindow *parent);

void            seahorse_pgp_generate_register      (void);

void            seahorse_pgp_generate_show          (SeahorsePGPSource *sksrc,
                                                     GtkWindow *parent);

void            seahorse_pgp_add_revoker_new        (SeahorsePgpKey *pkey,
                                                     GtkWindow *parent);

void            seahorse_pgp_expires_new            (SeahorsePgpSubkey *subkey,
                                                     GtkWindow *parent);

void            seahorse_pgp_add_subkey_new         (SeahorsePgpKey *pkey,
                                                     GtkWindow *parent);

void            seahorse_pgp_add_uid_new            (SeahorsePgpKey *pkey,
                                                     GtkWindow *parent);

void            seahorse_pgp_revoke_new             (SeahorsePgpSubkey *subkey,
                                                     GtkWindow *parent);

gboolean        seahorse_pgp_photo_add              (SeahorsePgpKey *pkey, 
                                                     GtkWindow *parent,
                                                     const gchar *path);
                                         
gboolean        seahorse_pgp_photo_delete           (SeahorsePgpPhoto *photo,
                                                     GtkWindow *parent);

#endif /* __SEAHORSE_PGP_DIALOGS_H__ */
