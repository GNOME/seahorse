/*
 * Seahorse
 *
 * Copyright (C) 2004 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#ifndef __SEAHORSE_GPG_OP_H__
#define __SEAHORSE_GPG_OP_H__

#include "config.h"

#include <gpgme.h>

gpgme_error_t seahorse_gpg_op_export_secret  (gpgme_ctx_t ctx, 
                                              const gchar **patterns,
                                              gpgme_data_t keydata);

gpgme_error_t seahorse_gpg_op_num_uids       (gpgme_ctx_t ctx, 
                                              const char *pattern,
                                              guint *number);

#endif /* __SEAHORSE_GPG_OP_H__ */
