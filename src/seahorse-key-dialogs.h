/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004,2005 Nate Nielsen
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

#ifndef __SEAHORSE_KEY_DIALOGS__
#define __SEAHORSE_KEY_DIALOGS__

#include <gtk/gtk.h>

#include "seahorse-context.h"
#include "seahorse-pgp-key.h"

#ifdef WITH_SSH
#include "seahorse-ssh-key.h"
#endif

#ifdef WITH_GNOME_KEYRING
#include "seahorse-gkeyring-item.h"
#endif

void        seahorse_key_properties_new (SeahorsePGPKey     *pkey,
                                         GtkWindow          *parent);

#ifdef WITH_SSH

void        seahorse_ssh_key_properties_new (SeahorseSSHKey *skey,
                                             GtkWindow      *parent);

void        seahorse_ssh_upload_prompt  (GList      *keys,
                                         GtkWindow  *parent);

void        seahorse_ssh_generate_show  (SeahorseSSHSource  *sksrc,
                                         GtkWindow          *parent);

#endif /* WITH_SSH */

#ifdef WITH_GNOME_KEYRING

void        seahorse_gkeyring_item_properties_new (SeahorseGKeyringItem *git,
                                                   GtkWindow            *parent);

#endif /* WITH_GNOME_KEYRING */


void        seahorse_pgp_generate_show  (SeahorsePGPSource  *sksrc,
                                         GtkWindow          *parent);

void        seahorse_add_revoker_new    (SeahorsePGPKey     *pkey,
                                         GtkWindow          *parent);

void        seahorse_expires_new        (SeahorsePGPKey     *pkey,
                                         GtkWindow          *parent,
                                         guint index);

void        seahorse_add_subkey_new     (SeahorsePGPKey     *pkey,
                                         GtkWindow          *parent);

void        seahorse_add_uid_new        (SeahorsePGPKey     *pkey,
                                         GtkWindow          *parent);

void        seahorse_delete_subkey_new  (SeahorsePGPKey     *pkey,
                                         guint              index);

void        seahorse_delete_userid_show (SeahorseKey        *skey, 
                                         guint              index);

void        seahorse_revoke_new         (SeahorsePGPKey     *pkey,
                                         GtkWindow          *parent,
                                         guint              index);

void        seahorse_sign_show          (SeahorsePGPKey     *pkey,
                                         GtkWindow          *parent, 
                                         guint              uid);
                                         
gboolean    seahorse_photo_add          (SeahorsePGPKey *pkey, 
                                         GtkWindow *parent,
                                         const gchar *path);
                                         
gboolean    seahorse_photo_delete       (SeahorsePGPKey *pkey,
                                         GtkWindow *parent,
                                         gpgmex_photo_id_t photo);

#endif /* __SEAHORSE_KEY_DIALOGS__ */
