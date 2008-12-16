/*
 * Seahorse
 *
 * Copyright (C) 2004 Stefan Walter
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

#ifndef __SEAHORSE_GPG_OP_H__
#define __SEAHORSE_GPG_OP_H__

#include "config.h"

#include <gpgme.h>

gpgme_error_t seahorse_gpg_op_export_secret  (gpgme_ctx_t ctx, 
                                              const char *pattern,
                                              gpgme_data_t keydata);

gpgme_error_t seahorse_gpg_op_num_uids       (gpgme_ctx_t ctx, 
                                              const char *pattern,
                                              guint *number);

#endif /* __SEAHORSE_GPG_OP_H__ */
