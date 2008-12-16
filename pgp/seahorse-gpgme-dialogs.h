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
 
#ifndef __SEAHORSE_GPGME_DIALOGS_H__
#define __SEAHORSE_GPGME_DIALOGS_H__

#include <gtk/gtk.h>

#include "pgp/seahorse-gpgme-key.h"
#include "pgp/seahorse-gpgme-photo.h"
#include "pgp/seahorse-gpgme-subkey.h"
#include "pgp/seahorse-gpgme-source.h"
#include "pgp/seahorse-gpgme-uid.h"

void            seahorse_gpgme_sign_prompt         (SeahorseGpgmeKey *key,
                                                    GtkWindow *parent);

void            seahorse_gpgme_sign_prompt_uid     (SeahorseGpgmeUid *uid,
                                                    GtkWindow *parent);

void            seahorse_gpgme_generate_register    (void);

void            seahorse_gpgme_generate_show        (SeahorseGpgmeSource *sksrc,
                                                     GtkWindow *parent);

void            seahorse_gpgme_add_revoker_new      (SeahorseGpgmeKey *pkey,
                                                     GtkWindow *parent);

void            seahorse_gpgme_expires_new          (SeahorseGpgmeSubkey *subkey,
                                                     GtkWindow *parent);

void            seahorse_gpgme_add_subkey_new       (SeahorseGpgmeKey *pkey,
                                                     GtkWindow *parent);

void            seahorse_gpgme_add_uid_new          (SeahorseGpgmeKey *pkey,
                                                     GtkWindow *parent);

void            seahorse_gpgme_revoke_new           (SeahorseGpgmeSubkey *subkey,
                                                     GtkWindow *parent);

gboolean        seahorse_gpgme_photo_add            (SeahorseGpgmeKey *pkey, 
                                                     GtkWindow *parent,
                                                     const gchar *path);
                                         
gboolean        seahorse_gpgme_photo_delete         (SeahorseGpgmePhoto *photo,
                                                     GtkWindow *parent);

#endif /* __SEAHORSE_GPGME_DIALOGS_H__ */
