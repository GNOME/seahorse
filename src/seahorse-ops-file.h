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

#ifndef __SEAHORSE_OPS_FILE_H__
#define __SEAHORSE_OPS_FILE_H__

#include <glib.h>
#include <gpgme.h>

#include "seahorse-context.h"
#include "seahorse-key.h"

typedef enum {
	SEAHORSE_SIG_FILE, /* Has a '.sig' suffix */
	SEAHORSE_CRYPT_FILE /* Has a '.gpg' suffix */
} SeahorseFileType;

gchar*		seahorse_ops_file_add_suffix		(const gchar		*file,
							 SeahorseFileType	type,
							 SeahorseContext	*sctx);

gchar*		seahorse_ops_file_del_suffix		(const gchar		*file,
							 SeahorseFileType	type);

gboolean	seahorse_ops_file_check_suffix		(const gchar		*file,
							 SeahorseFileType	type);

gboolean	seahorse_ops_file_import		(SeahorseContext	*sctx,
						 	const gchar		*file);

gboolean	seahorse_ops_file_export		(SeahorseContext	*sctx,
						 	const gchar		*file,
						 	const SeahorseKey	*skey);

gboolean	seahorse_ops_file_export_multiple	(SeahorseContext	*sctx,
							 const gchar		*file,
							 GpgmeRecipients	recips);

gboolean	seahorse_ops_file_sign			(SeahorseContext	*sctx,
							 const gchar		*file);

gboolean	seahorse_ops_file_encrypt		(SeahorseContext	*sctx,
							 const gchar		*file,
							 GpgmeRecipients	recips);

gboolean	seahorse_ops_file_decrypt		(SeahorseContext	*sctx,
							 const gchar		*file);

gboolean	seahorse_ops_file_verify		(SeahorseContext	*sctx,
							 const gchar		*file,
							 GpgmeSigStat		*status);

gboolean	seahorse_ops_file_decrypt_verify	(SeahorseContext	*sctx,
							 const gchar		*file,
							 GpgmeSigStat		*status);

#endif /* __SEAHORSE_OPS_FILE_H__ */
