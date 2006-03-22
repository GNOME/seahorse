/*
 * Seahorse
 *
 * Copyright (C) 2005 Nate Nielsen
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
 * SeahorsePGPKey: Represents a PGP key loaded via GPGME.
 * 
 * - Derived from SeahorseKey
 * - Stuff from seahorse-key-op.c should eventually be merged in here.
 * 
 * Properties:
 *   display-name: (gchar*) The display name for the key.
 *   display-id: (gchar*) The keyid to display.
 *   simple-name: (gchar*) Shortened display name for the key (for use in files etc...).
 *   fingerprint: (gchar*) Displayable fingerprint for the key.
 *   validity: (SeahorseValidity) The key validity.
 *   trust: (SeahorseValidity) Trust for the key.
 *   expires: (gulong) Date this key expires or 0.
 */
 
#ifndef __SEAHORSE_PGP_KEY_H__
#define __SEAHORSE_PGP_KEY_H__

#include <gtk/gtk.h>
#include <gpgme.h>

#include "seahorse-key.h"
#include "seahorse-pgp-source.h"
#include "seahorse-gpgmex.h"

#define SKEY_PGP                         (g_quark_from_static_string ("openpgp"))

enum {
    SKEY_PGPSIG_TRUSTED = 0x0001,
    SKEY_PGPSIG_PERSONAL = 0x0002
};

#define SEAHORSE_TYPE_PGP_KEY            (seahorse_pgp_key_get_type ())
#define SEAHORSE_PGP_KEY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PGP_KEY, SeahorsePGPKey))
#define SEAHORSE_PGP_KEY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PGP_KEY, SeahorsePGPKeyClass))
#define SEAHORSE_IS_PGP_KEY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PGP_KEY))
#define SEAHORSE_IS_PGP_KEY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PGP_KEY))
#define SEAHORSE_PGP_KEY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PGP_KEY, SeahorsePGPKeyClass))


typedef struct _SeahorsePGPKey SeahorsePGPKey;
typedef struct _SeahorsePGPKeyClass SeahorsePGPKeyClass;

struct _SeahorsePGPKey {
    SeahorseKey	                parent;

    /*< public >*/
    gpgme_key_t                 pubkey;         /* The public key */
    gpgme_key_t                 seckey;         /* The secret key */
    gpgmex_photo_id_t           photoids;       /* List of photos */
};

struct _SeahorsePGPKeyClass {
    SeahorseKeyClass            parent_class;
};

SeahorsePGPKey* seahorse_pgp_key_new                  (SeahorseKeySource *sksrc,
                                                       gpgme_key_t        pubkey,
                                                       gpgme_key_t        seckey);

GType           seahorse_pgp_key_get_type             (void);

guint           seahorse_pgp_key_get_num_subkeys      (SeahorsePGPKey   *pkey);

gpgme_subkey_t  seahorse_pgp_key_get_nth_subkey       (SeahorsePGPKey   *pkey,
                                                       guint            index);

guint           seahorse_pgp_key_get_num_userids      (SeahorsePGPKey   *pkey);

gpgme_user_id_t seahorse_pgp_key_get_nth_userid       (SeahorsePGPKey   *pkey,
                                                       guint            index);

gchar*          seahorse_pgp_key_get_userid	          (SeahorsePGPKey   *pkey,
                                                       guint            index);

gchar*          seahorse_pgp_key_get_userid_name      (SeahorsePGPKey   *pkey,
                                                       guint            index);

gchar*          seahorse_pgp_key_get_userid_email     (SeahorsePGPKey   *pkey,
                                                       guint            index);

gchar*          seahorse_pgp_key_get_userid_comment   (SeahorsePGPKey   *pkey,
                                                       guint            index);
                                                       
const gchar*    seahorse_pgp_key_get_algo             (SeahorsePGPKey   *pkey,
                                                       guint            index);

const gchar*    seahorse_pgp_key_get_id               (gpgme_key_t      key,
                                                       guint            index);

guint           seahorse_pgp_key_get_num_photoids     (SeahorsePGPKey   *pkey);
 
gpgmex_photo_id_t seahorse_pgp_key_get_nth_photoid    (SeahorsePGPKey   *pkey,
                                                       guint            index);                                                    

gpgmex_photo_id_t seahorse_pgp_key_get_photoid_n      (SeahorsePGPKey   *pkey,
                                                       guint            uid);

void            seahorse_pgp_key_get_signature_text   (SeahorsePGPKey   *pkey,
                                                       gpgme_key_sig_t  signature,
                                                       gchar            **name,
                                                       gchar            **email,
                                                       gchar            **comment);

guint           seahorse_pgp_key_get_sigtype          (SeahorsePGPKey   *pkey, 
                                                       gpgme_key_sig_t  signature);

gboolean        seahorse_pgp_key_have_signatures      (SeahorsePGPKey   *pkey,
                                                       guint            types);

/* 
 * This function is necessary because the uid stored in a gpgme_user_id_t
 * struct is only usable with gpgme functions.  Problems will be caused if 
 * that uid is used with functions found in seahorse-pgp-key-op.h.  This 
 * function is only to be called with uids from gpgme_user_id_t structs.
 */
guint           seahorse_pgp_key_get_actual_uid       (SeahorsePGPKey   *pkey,
                                                       guint            uid);
#endif /* __SEAHORSE_KEY_H__ */
