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

#ifndef __SEAHORSE_OPS_TEXT_H__
#define __SEAHORSE_OPS_TEXT_H__

/* Text related operations, mainly conveniance wrappers around gpgme operations.
 * Functions return TRUE iff no errors occur, FALSE otherwise.
 * When completed, functions send the operation status to context.
 * Also see corresponding functions in seahorse-ops-data. */

#include <glib.h>
#include <gpgme.h>

#include "seahorse-context.h"
#include "seahorse-key.h"

/* Import keys contained in text */
gboolean	seahorse_ops_text_import		(SeahorseContext	*sctx,
							 const gchar		*text);

/* Export key to string */
gboolean	seahorse_ops_text_export		(SeahorseContext	*sctx,
							 GString		*string,
							 const SeahorseKey	*skey);

/* Export recipients' keys to string */
gboolean	seahorse_ops_text_export_multiple	(SeahorseContext	*sctx,
							 GString		*string,
							 GpgmeRecipients	recips);

/* The following ops will operate on the text given in string.
 * When the function finishes, string will contain the text output of the operation. */

/* Signs string with current signers */
gboolean	seahorse_ops_text_sign			(SeahorseContext	*sctx,
							 GString		*string);

/* Clear signs string with current signers */
gboolean	seahorse_ops_text_clear_sign		(SeahorseContext	*sctx,
							 GString		*string);

/* Encrypts string to recipients */
gboolean	seahorse_ops_text_encrypt		(SeahorseContext	*sctx,
							 GString		*string,
							 GpgmeRecipients	recips);

/* Decrypts string */
gboolean	seahorse_ops_text_decrypt		(SeahorseContext	*sctx,
							 GString		*string);

/* Verifies any signatures in string,
 * status will point to the overall signature status. */
gboolean	seahorse_ops_text_verify		(SeahorseContext	*sctx,
							 GString		*string,
							 GpgmeSigStat		*status);

/* Decrypts string, then verifies any signatures.
 * Status will point to the overall signature status. */
gboolean	seahorse_ops_text_decrypt_verify	(SeahorseContext	*sctx,
							 GString		*string,
							 GpgmeSigStat		*status);

#endif /* __SEAHORSE_OPS_TEXT_H__ */
