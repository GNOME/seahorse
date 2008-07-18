/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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
 * SeahorsePGPUid: Represents a PGP UID loaded via GPGME.
 * 
 * - Derived from SeahorseKey
 * 
 * Properties:
 *   label: (gchar*) The display name for the UID.
 *   markup: 
 *   simple-name: 
 *   validity: (SeahorseValidity) The key validity.
 *   stock-id: 
 */
 
#ifndef __SEAHORSE_PGP_UID_H__
#define __SEAHORSE_PGP_UID_H__

#include <gtk/gtk.h>
#include <gpgme.h>

#include "seahorse-key.h"

#include "pgp/seahorse-pgp-module.h"
#include "pgp/seahorse-gpgmex.h"

#define SEAHORSE_TYPE_PGP_UID            (seahorse_pgp_uid_get_type ())

/* For vala's sake */
#define SEAHORSE_PGP_TYPE_UID 		SEAHORSE_TYPE_PGP_UID

#define SEAHORSE_PGP_UID(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PGP_UID, SeahorsePGPUid))
#define SEAHORSE_PGP_UID_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PGP_UID, SeahorsePGPUidClass))
#define SEAHORSE_IS_PGP_UID(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PGP_UID))
#define SEAHORSE_IS_PGP_UID_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PGP_UID))
#define SEAHORSE_PGP_UID_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PGP_UID, SeahorsePGPUidClass))


typedef struct _SeahorsePGPUid SeahorsePGPUid;
typedef struct _SeahorsePGPUidClass SeahorsePGPUidClass;

struct _SeahorsePGPUid {
	SeahorseObject parent;

	/*< public >*/
	gpgme_key_t pubkey;         /* The public key that this uid is part of */
	gpgme_user_id_t userid;     /* The userid referred to */
	guint index;                /* The index of the UID */
};

struct _SeahorsePGPUidClass {
	SeahorseObjectClass parent_class;
};

SeahorsePGPUid*   seahorse_pgp_uid_new                  (gpgme_key_t pubkey,
                                                         gpgme_user_id_t userid);

GType             seahorse_pgp_uid_get_type             (void);

gboolean          seahorse_pgp_uid_equal                (SeahorsePGPUid *uid,
                                                         gpgme_user_id_t userid);

gchar*            seahorse_pgp_uid_get_display_name     (SeahorsePGPUid *uid);

gchar*            seahorse_pgp_uid_get_markup           (SeahorsePGPUid *uid, 
                                                         guint flags);

gchar*            seahorse_pgp_uid_get_name             (SeahorsePGPUid *uid);

gchar*            seahorse_pgp_uid_get_email            (SeahorsePGPUid *uid);

gchar*            seahorse_pgp_uid_get_comment          (SeahorsePGPUid *uid);
                                  
SeahorseValidity  seahorse_pgp_uid_get_validity         (SeahorsePGPUid *uid);

guint             seahorse_pgp_uid_get_index            (SeahorsePGPUid *uid);

#endif /* __SEAHORSE_PGP_UID_H__ */
