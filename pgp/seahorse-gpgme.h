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

#include <glib.h>
#include <glib-object.h>

#include <gpgme.h>

#include "seahorse-common.h"

typedef struct _SeahorseKeyTypeTable *SeahorseKeyTypeTable;

struct _SeahorseKeyTypeTable {
    int rsa_sign, rsa_enc, dsa_sign, elgamal_enc;
};

/* TODO: I think these are extraneous and can be removed. In actuality OK == 0 */
#define GPG_IS_OK(e)        (gpgme_err_code (e) == GPG_ERR_NO_ERROR)
#define GPG_OK              (gpgme_error (GPG_ERR_NO_ERROR))
#define GPG_E(e)            (gpgme_error (e))

#define            SEAHORSE_GPGME_ERROR             (seahorse_gpgme_error_domain ())

GQuark             seahorse_gpgme_error_domain      (void);

gboolean           seahorse_gpgme_propagate_error   (gpgme_error_t gerr,
                                                     GError** error);

void               seahorse_gpgme_handle_error      (gpgme_error_t  err,
                                                     const char    *desc,
                                                     ...);

#define            SEAHORSE_GPGME_BOXED_KEY         (seahorse_gpgme_boxed_key_type ())

GType              seahorse_gpgme_boxed_key_type    (void);

SeahorseValidity   seahorse_gpgme_convert_validity  (gpgme_validity_t validity);

GSource *          seahorse_gpgme_gsource_new       (gpgme_ctx_t gctx,
                                                     GCancellable *cancellable);
