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

#ifndef __SEAHORSE_OPS_DATA_H__
#define __SEAHORSE_OPS_DATA_H__

#include <glib.h>
#include <gpgme.h>

#include "seahorse-context.h"

gboolean	seahorse_ops_data_import		(SeahorseContext	*sctx,
							 const GpgmeData	data);

gboolean	seahorse_ops_data_export		(const SeahorseContext	*sctx,
							 GpgmeData		*data,
							 const SeahorseKey	*skey);

gboolean	seahorse_ops_data_export_multiple	(const SeahorseContext	*sctx,
							 GpgmeData		*data,
							 GpgmeRecipients	recips);

gboolean	seahorse_ops_data_sign			(const SeahorseContext	*sctx,
							 GpgmeData		plain,
							 GpgmeData		*sig,
							 GpgmeSigMode		mode);

gboolean	seahorse_ops_data_encrypt		(const SeahorseContext	*sctx,
							 GpgmeRecipients	recips,
							 GpgmeData		plain,
							 GpgmeData		*cipher);

gboolean	seahorse_ops_data_encrypt_sign		(const SeahorseContext	*sctx,
							 GpgmeRecipients	recips,
							 GpgmeData		plain,
							 GpgmeData		*cipher);

gboolean	seahorse_ops_data_decrypt		(const SeahorseContext	*sctx,
							 GpgmeData		cipher,
							 GpgmeData		*plain);

gboolean	seahorse_ops_data_verify		(const SeahorseContext	*sctx,
							 GpgmeData		sig,
							 GpgmeData		plain,
							 GpgmeSigStat		*status);

gboolean	seahorse_ops_data_decrypt_verify	(const SeahorseContext	*sctx,
							 GpgmeData		cipher,
							 GpgmeData		*plain,
							 GpgmeSigStat		*status);

#endif /* __SEAHORSE_OPS_DATA_H__ */
