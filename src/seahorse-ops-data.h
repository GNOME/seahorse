/*
 * Seahorse
 *
 * Copyright (C) 2002 Jacob Perkins
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

/* Common data related operations used by seahorse-ops-file
 * and seahorse-ops-text.  Wraps around usage of GpgmeData. */

#include <glib.h>
#include <gpgme.h>

#include "seahorse-context.h"
#include "seahorse-key.h"

/* Imports keys in data to key ring */
gboolean	seahorse_ops_data_import		(SeahorseContext	*sctx,
							 const GpgmeData	data);

/* Exports key to data */
gboolean	seahorse_ops_data_export		(const SeahorseContext	*sctx,
							 GpgmeData		*data,
							 const SeahorseKey	*skey);

/* Exports keys in recips to data, releasing recips when finished. */
gboolean	seahorse_ops_data_export_multiple	(const SeahorseContext	*sctx,
							 GpgmeData		*data,
							 GpgmeRecipients	recips);

/* All operations below will release the first GpgmeData when finished. */

/* Signs data in plain with current signers.  Signed data will be in sig. */
gboolean	seahorse_ops_data_sign			(const SeahorseContext	*sctx,
							 GpgmeData		plain,
							 GpgmeData		*sig,
							 GpgmeSigMode		mode);

/* Encrypts data in plain to recips.  Encrypted data will be in cipher.
 * Release recipients when finished. */
gboolean	seahorse_ops_data_encrypt		(const SeahorseContext	*sctx,
							 GpgmeRecipients	recips,
							 GpgmeData		plain,
							 GpgmeData		*cipher);

/* Decrypts data in cipher to plain */
gboolean	seahorse_ops_data_decrypt		(const SeahorseContext	*sctx,
							 GpgmeData		cipher,
							 GpgmeData		*plain);

/* Verifies signatures in sig.  For files, plain is the data file.
 * For text, plain will contain the verified data.
 * Status will contain the overall status. */
gboolean	seahorse_ops_data_verify		(const SeahorseContext	*sctx,
							 GpgmeData		sig,
							 GpgmeData		plain,
							 GpgmeSigStat		*status);

/* Decrypts and verifies cipher.  Status will have the overall verification status.
 * Plain will contain the decrypted (and verified) data. */
gboolean	seahorse_ops_data_decrypt_verify	(const SeahorseContext	*sctx,
							 GpgmeData		cipher,
							 GpgmeData		*plain,
							 GpgmeSigStat		*status);

#endif /* __SEAHORSE_OPS_DATA_H__ */
