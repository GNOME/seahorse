/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "seahorse-common.h"
#include "seahorse-gpgme.h"

#define SEAHORSE_GPGME_TYPE_KEY_PARMS (seahorse_gpgme_key_parms_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseGpgmeKeyParms, seahorse_gpgme_key_parms,
                      SEAHORSE_GPGME, KEY_PARMS,
                      GObject)

SeahorseGpgmeKeyParms *  seahorse_gpgme_key_parms_new                  (const char              *name,
                                                                        const char              *email,
                                                                        const char              *comment,
                                                                        SeahorsePgpKeyAlgorithm  type,
                                                                        unsigned int             length,
                                                                        GDateTime               *expires);

void                     seahorse_gpgme_key_parms_set_passphrase       (SeahorseGpgmeKeyParms *self,
                                                                        const char            *passphrase);

gboolean                 seahorse_gpgme_key_parms_has_subkey           (SeahorseGpgmeKeyParms *self);

gboolean                 seahorse_gpgme_key_parms_has_valid_name       (SeahorseGpgmeKeyParms *self);

gboolean                 seahorse_gpgme_key_parms_has_valid_key_length (SeahorseGpgmeKeyParms *self);

char *                   seahorse_gpgme_key_parms_to_string            (SeahorseGpgmeKeyParms *self);
