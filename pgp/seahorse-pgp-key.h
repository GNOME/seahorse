/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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

#ifndef __SEAHORSE_PGP_KEY_H__
#define __SEAHORSE_PGP_KEY_H__

#include <glib-object.h>

#include "pgp/seahorse-pgp-module.h"

#include "seahorse-object.h"
#include "seahorse-validity.h"

enum {
    SKEY_PGPSIG_TRUSTED = 0x0001,
    SKEY_PGPSIG_PERSONAL = 0x0002
};

#define SEAHORSE_TYPE_PGP_KEY            (seahorse_pgp_key_get_type ())
#define SEAHORSE_PGP_KEY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PGP_KEY, SeahorsePgpKey))
#define SEAHORSE_PGP_KEY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PGP_KEY, SeahorsePgpKeyClass))
#define SEAHORSE_IS_PGP_KEY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PGP_KEY))
#define SEAHORSE_IS_PGP_KEY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PGP_KEY))
#define SEAHORSE_PGP_KEY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PGP_KEY, SeahorsePgpKeyClass))


typedef struct _SeahorsePgpKey SeahorsePgpKey;
typedef struct _SeahorsePgpKeyClass SeahorsePgpKeyClass;
typedef struct _SeahorsePgpKeyPrivate SeahorsePgpKeyPrivate;

struct _SeahorsePgpKey {
	SeahorseObject parent;
	SeahorsePgpKeyPrivate *pv;
};

struct _SeahorsePgpKeyClass {
	SeahorseObjectClass parent_class;
	
	/* virtual methods */
	GList* (*get_uids) (SeahorsePgpKey *self);
	void   (*set_uids) (SeahorsePgpKey *self, GList *uids);
	
	GList* (*get_subkeys) (SeahorsePgpKey *self);
	void   (*set_subkeys) (SeahorsePgpKey *self, GList *uids);
	
	GList* (*get_photos) (SeahorsePgpKey *self);
	void   (*set_photos) (SeahorsePgpKey *self, GList *uids);
};

GType             seahorse_pgp_key_get_type             (void);

SeahorsePgpKey*   seahorse_pgp_key_new                  (void);

void              seahorse_pgp_key_realize              (SeahorsePgpKey *self);

GList*            seahorse_pgp_key_get_subkeys          (SeahorsePgpKey *self);

void              seahorse_pgp_key_set_subkeys          (SeahorsePgpKey *self,
                                                         GList *subkeys);

GList*            seahorse_pgp_key_get_uids             (SeahorsePgpKey *self);

void              seahorse_pgp_key_set_uids             (SeahorsePgpKey *self,
                                                         GList *subkeys);

GList*            seahorse_pgp_key_get_photos           (SeahorsePgpKey *self);

void              seahorse_pgp_key_set_photos           (SeahorsePgpKey *self,
                                                         GList *subkeys);

const gchar*      seahorse_pgp_key_get_fingerprint      (SeahorsePgpKey *self);

SeahorseValidity  seahorse_pgp_key_get_validity         (SeahorsePgpKey *self);

gulong            seahorse_pgp_key_get_expires          (SeahorsePgpKey *self);

SeahorseValidity  seahorse_pgp_key_get_trust            (SeahorsePgpKey *self);

guint             seahorse_pgp_key_get_length           (SeahorsePgpKey *self);

const gchar*      seahorse_pgp_key_get_algo             (SeahorsePgpKey *self);

const gchar*      seahorse_pgp_key_get_keyid            (SeahorsePgpKey *self);

gboolean          seahorse_pgp_key_has_keyid            (SeahorsePgpKey *self, 
                                                         const gchar *keyid);

gchar*            seahorse_pgp_key_calc_identifier      (const gchar *keyid);

gchar*            seahorse_pgp_key_calc_id              (const gchar *keyid,
                                                         guint index);

const gchar*      seahorse_pgp_key_calc_rawid           (GQuark id);

GQuark            seahorse_pgp_key_canonize_id          (const gchar *keyid);

#endif /* __SEAHORSE_KEY_H__ */
