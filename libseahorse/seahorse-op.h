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
					 GpgmeRecipients	recips,
					 GpgmeError		*err);

gint		seahorse_op_import_file		(SeahorseContext	*sctx,
						 const gchar		*path,
						 GpgmeError		*err);

gint		seahorse_op_import_text		(SeahorseContext	*sctx,
						 const gchar		*text,
						 GpgmeError		*err);

void		seahorse_op_export_file		(SeahorseContext	*sctx,
						 const gchar		*path,
						 GpgmeRecipients	recips,
						 GpgmeError		*err);

gchar*		seahorse_op_export_text		(SeahorseContext	*sctx,
						 GpgmeRecipients	recips,
						 GpgmeError		*err);

gchar*		seahorse_op_encrypt_file	(SeahorseContext	*sctx,
						 const gchar		*path,
						 GpgmeRecipients	recips,
						 GpgmeError		*err);

gchar*		seahorse_op_encrypt_text	(SeahorseContext	*sctx,
						 const gchar		*text,
						 GpgmeRecipients	recips,
						 GpgmeError		*err);

gchar*		seahorse_op_sign_file		(SeahorseContext	*sctx,
						 const gchar		*path,
						 GpgmeError		*err);

gchar*		seahorse_op_sign_text		(SeahorseContext	*sctx,
						 const gchar		*text,
						 GpgmeError		*err);

gchar*		seahorse_op_encrypt_sign_file	(SeahorseContext	*sctx,
						 const gchar		*path,
						 GpgmeRecipients	recips,
						 GpgmeError		*err);

gchar*		seahorse_op_encrypt_sign_text	(SeahorseContext	*sctx,
						 const gchar		*text,
						 GpgmeRecipients	recips,
						 GpgmeError		*err);

gchar*		seahorse_op_decrypt_file	(SeahorseContext	*sctx,
						 const gchar		*path,
						 GpgmeError		*err);

gchar*		seahorse_op_decrypt_text	(SeahorseContext	*sctx,
						 const gchar		*text,
						 GpgmeError		*err);

void		seahorse_op_verify_file		(SeahorseContext	*sctx,
						 const gchar		*path,
						 GpgmeSigStat		*status,
						 GpgmeError		*err);

gchar*		seahorse_op_verify_text		(SeahorseContext	*sctx,
						 const gchar		*text,
						 GpgmeSigStat		*status,
						 GpgmeError		*err);

gchar*		seahorse_op_decrypt_verify_file	(SeahorseContext	*sctx,
						 const gchar		*path,
						 GpgmeSigStat		*status,
						 GpgmeError		*err);

gchar*		seahorse_op_decrypt_verify_text	(SeahorseContext	*sctx,
						 const gchar		*text,
						 GpgmeSigStat		*status,
						 GpgmeError		*err);

#endif /* __SEAHORSE_OP_H__ */
