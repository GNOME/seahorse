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

#ifndef __SEAHORSE_PGP_SUBKEY_H__
#define __SEAHORSE_PGP_SUBKEY_H__

#include <glib-object.h>

#include "seahorse-object.h"

#include "seahorse-validity.h"

#define SEAHORSE_TYPE_PGP_SUBKEY            (seahorse_pgp_subkey_get_type ())

#define SEAHORSE_PGP_SUBKEY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PGP_SUBKEY, SeahorsePgpSubkey))
#define SEAHORSE_PGP_SUBKEY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PGP_SUBKEY, SeahorsePgpSubkeyClass))
#define SEAHORSE_IS_PGP_SUBKEY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PGP_SUBKEY))
#define SEAHORSE_IS_PGP_SUBKEY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PGP_SUBKEY))
#define SEAHORSE_PGP_SUBKEY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PGP_SUBKEY, SeahorsePgpSubkeyClass))

typedef struct _SeahorsePgpSubkey SeahorsePgpSubkey;
typedef struct _SeahorsePgpSubkeyClass SeahorsePgpSubkeyClass;
typedef struct _SeahorsePgpSubkeyPrivate SeahorsePgpSubkeyPrivate;

struct _SeahorsePgpSubkey {
	GObject parent;
	SeahorsePgpSubkeyPrivate *pv;
};

struct _SeahorsePgpSubkeyClass {
	GObjectClass parent_class;
};

GType               seahorse_pgp_subkey_get_type          (void);

SeahorsePgpSubkey*  seahorse_pgp_subkey_new               (void);

guint               seahorse_pgp_subkey_get_index         (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_index         (SeahorsePgpSubkey *self,
                                                           guint index);

const gchar*        seahorse_pgp_subkey_get_keyid         (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_keyid         (SeahorsePgpSubkey *self,
                                                           const gchar *keyid);

SeahorseValidity    seahorse_pgp_subkey_get_validity      (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_validity      (SeahorsePgpSubkey *self,
                                                           SeahorseValidity validity);

const gchar*        seahorse_pgp_subkey_get_algorithm     (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_algorithm     (SeahorsePgpSubkey *self,
                                                           const gchar *algorithm);

guint               seahorse_pgp_subkey_get_length        (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_length        (SeahorsePgpSubkey *self,
                                                           guint index);

gulong              seahorse_pgp_subkey_get_created       (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_created       (SeahorsePgpSubkey *self,
                                                           gulong created);

gulong              seahorse_pgp_subkey_get_expires       (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_expires       (SeahorsePgpSubkey *self,
                                                           gulong expires);

const gchar*        seahorse_pgp_subkey_get_description   (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_description   (SeahorsePgpSubkey *self,
                                                           const gchar *description);

gchar*              seahorse_pgp_subkey_calc_description  (const gchar *name, 
                                                           guint index); 

const gchar*        seahorse_pgp_subkey_get_fingerprint   (SeahorsePgpSubkey *self);

void                seahorse_pgp_subkey_set_fingerprint   (SeahorsePgpSubkey *self,
                                                           const gchar *description);

gchar*              seahorse_pgp_subkey_calc_fingerprint  (const gchar *raw_fingerprint); 


#endif /* __SEAHORSE_PGP_SUBKEY_H__ */
