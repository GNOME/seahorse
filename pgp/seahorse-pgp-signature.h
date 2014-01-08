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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef __SEAHORSE_PGP_SIGNATURE_H__
#define __SEAHORSE_PGP_SIGNATURE_H__

#include <glib-object.h>

#include "seahorse-validity.h"

#define SEAHORSE_TYPE_PGP_SIGNATURE            (seahorse_pgp_signature_get_type ())

#define SEAHORSE_PGP_SIGNATURE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PGP_SIGNATURE, SeahorsePgpSignature))
#define SEAHORSE_PGP_SIGNATURE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PGP_SIGNATURE, SeahorsePgpSignatureClass))
#define SEAHORSE_IS_PGP_SIGNATURE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PGP_SIGNATURE))
#define SEAHORSE_IS_PGP_SIGNATURE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PGP_SIGNATURE))
#define SEAHORSE_PGP_SIGNATURE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PGP_SIGNATURE, SeahorsePgpSignatureClass))

typedef struct _SeahorsePgpSignature SeahorsePgpSignature;
typedef struct _SeahorsePgpSignatureClass SeahorsePgpSignatureClass;
typedef struct _SeahorsePgpSignaturePrivate SeahorsePgpSignaturePrivate;

struct _SeahorsePgpSignature {
	GObject parent;
	SeahorsePgpSignaturePrivate *pv;
};

struct _SeahorsePgpSignatureClass {
	GObjectClass parent_class;
};

GType                  seahorse_pgp_signature_get_type       (void);

SeahorsePgpSignature*  seahorse_pgp_signature_new            (const gchar *keyid);

const gchar*           seahorse_pgp_signature_get_keyid      (SeahorsePgpSignature *self);

void                   seahorse_pgp_signature_set_keyid      (SeahorsePgpSignature *self,
                                                              const gchar *keyid);

guint                  seahorse_pgp_signature_get_flags      (SeahorsePgpSignature *self);

void                   seahorse_pgp_signature_set_flags      (SeahorsePgpSignature *self,
                                                              guint flags);

guint                  seahorse_pgp_signature_get_sigtype    (SeahorsePgpSignature *self);

#endif /* __SEAHORSE_PGP_SIGNATURE_H__ */
