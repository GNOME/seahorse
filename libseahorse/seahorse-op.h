/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#ifndef __SEAHORSE_OP_H__
#define __SEAHORSE_OP_H__

#include <glib.h>
#include <gpgme.h>

#include "seahorse-context.h"

typedef gchar*	(*SeahorseEncryptFunc)	(SeahorseContext	*sctx,
					 const gchar		*data,
					 gpgme_key_t *	recips,
					 gpgme_error_t		*err);

gint		seahorse_op_import_file		(SeahorseContext	*sctx,
						 const gchar		*path,
						 gpgme_error_t		*err);

gint		seahorse_op_import_text		(SeahorseContext	*sctx,
						 const gchar		*text,
						 gpgme_error_t		*err);

void		seahorse_op_export_file		(SeahorseContext	*sctx,
						 const gchar		*path,
						 gpgme_key_t *	recips,
						 gpgme_error_t		*err);

gchar*		seahorse_op_export_text		(SeahorseContext	*sctx,
						 gpgme_key_t *	recips,
						 gpgme_error_t		*err);

gchar*		seahorse_op_encrypt_file	(SeahorseContext	*sctx,
						 const gchar		*path,
						 gpgme_key_t *	recips,
						 gpgme_error_t		*err);

gchar*		seahorse_op_encrypt_text	(SeahorseContext	*sctx,
						 const gchar		*text,
						 gpgme_key_t *	recips,
						 gpgme_error_t		*err);

gchar*		seahorse_op_sign_file		(SeahorseContext	*sctx,
						 const gchar		*path,
						 gpgme_error_t		*err);

gchar*		seahorse_op_sign_text		(SeahorseContext	*sctx,
						 const gchar		*text,
						 gpgme_error_t		*err);

gchar*		seahorse_op_encrypt_sign_file	(SeahorseContext	*sctx,
						 const gchar		*path,
						 gpgme_key_t *	recips,
						 gpgme_error_t		*err);

gchar*		seahorse_op_encrypt_sign_text	(SeahorseContext	*sctx,
						 const gchar		*text,
						 gpgme_key_t *	recips,
						 gpgme_error_t		*err);

void		seahorse_op_verify_file		(SeahorseContext	*sctx,
						 const gchar		*path,
						 gpgme_verify_result_t	*status,
						 gpgme_error_t		*err);

gchar*		seahorse_op_verify_text		(SeahorseContext	*sctx,
						 const gchar		*text,
						 gpgme_verify_result_t	*status,
						 gpgme_error_t		*err);

gchar*		seahorse_op_decrypt_verify_file	(SeahorseContext	*sctx,
						 const gchar		*path,
						 gpgme_verify_result_t	*status,
						 gpgme_error_t		*err);

gchar*		seahorse_op_decrypt_verify_text	(SeahorseContext	*sctx,
						 const gchar		*text,
						 gpgme_verify_result_t	*status,
						 gpgme_error_t		*err);

#endif /* __SEAHORSE_OP_H__ */
