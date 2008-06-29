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

/**
 * Various UI elements and dialogs used in libseahorse.
 */
 
#ifndef __SEAHORSE_PGP_DIALOGS_H__
#define __SEAHORSE_PGP_DIALOGS_H__

#include <gtk/gtk.h>

#include "pgp/seahorse-pgp-key.h"

void            seahorse_pgp_sign_prompt           (SeahorsePGPKey *pkey,
                                                    guint uid,
                                                    GtkWindow *parent);

SeahorsePGPKey* seahorse_signer_get                 (GtkWindow *parent);

void            seahorse_pgp_handle_gpgme_error     (gpgme_error_t err, const gchar* desc, ...);

void            seahorse_pgp_key_properties_show    (SeahorsePGPKey *pkey,
                                                     GtkWindow *parent);

void            seahorse_pgp_generate_show          (SeahorsePGPSource *sksrc,
                                                     GtkWindow *parent);

void            seahorse_pgp_add_revoker_new        (SeahorsePGPKey *pkey,
                                                     GtkWindow *parent);

void            seahorse_pgp_expires_new            (SeahorsePGPKey *pkey,
                                                     GtkWindow *parent,
                                                     guint index);

void            seahorse_pgp_add_subkey_new         (SeahorsePGPKey *pkey,
                                                     GtkWindow *parent);

void            seahorse_pgp_add_uid_new            (SeahorsePGPKey *pkey,
                                                     GtkWindow *parent);

void            seahorse_pgp_revoke_new             (SeahorsePGPKey *pkey,
                                                     GtkWindow *parent,
                                                     guint index);

gboolean        seahorse_pgp_photo_add              (SeahorsePGPKey *pkey, 
                                                     GtkWindow *parent,
                                                     const gchar *path);
                                         
gboolean        seahorse_pgp_photo_delete           (SeahorsePGPKey *pkey,
                                                     GtkWindow *parent,
                                                     gpgmex_photo_id_t photo);

void            seahorse_pgp_delete_subkey_new      (SeahorsePGPKey *pkey,
                                                     guint index);

void            seahorse_pgp_delete_userid_show     (SeahorseKey *skey, 
                                                     guint index);

#endif /* __SEAHORSE_PGP_DIALOGS_H__ */
