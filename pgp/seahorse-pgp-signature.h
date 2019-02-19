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

#pragma once

#include <glib-object.h>

#include "seahorse-common.h"

#define SEAHORSE_PGP_TYPE_SIGNATURE (seahorse_pgp_signature_get_type ())
G_DECLARE_DERIVABLE_TYPE (SeahorsePgpSignature, seahorse_pgp_signature,
                          SEAHORSE_PGP, SIGNATURE,
                          GObject)


struct _SeahorsePgpSignatureClass {
    GObjectClass parent_class;
};

SeahorsePgpSignature*  seahorse_pgp_signature_new            (const gchar *keyid);

const gchar*           seahorse_pgp_signature_get_keyid      (SeahorsePgpSignature *self);

void                   seahorse_pgp_signature_set_keyid      (SeahorsePgpSignature *self,
                                                              const gchar *keyid);

guint                  seahorse_pgp_signature_get_flags      (SeahorsePgpSignature *self);

void                   seahorse_pgp_signature_set_flags      (SeahorsePgpSignature *self,
                                                              guint flags);

guint                  seahorse_pgp_signature_get_sigtype    (SeahorsePgpSignature *self);
