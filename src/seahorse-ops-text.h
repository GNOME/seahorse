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

#ifndef __SEAHORSE_OPS_TEXT_H__
#define __SEAHORSE_OPS_TEXT_H__

#include <glib.h>
#include <gpgme.h>

#include "seahorse-context.h"
#include "seahorse-key.h"

gboolean	seahorse_ops_text_import		(SeahorseContext	*sctx,
							 const gchar		*text);

gboolean	seahorse_ops_text_export		(SeahorseContext	*sctx,
							 GString		*string,
							 const SeahorseKey	*skey);

gboolean	seahorse_ops_text_export_multiple	(SeahorseContext	*sctx,
							 GString		*string,
							 GpgmeRecipients	recips);

gboolean	seahorse_ops_text_sign			(SeahorseContext	*sctx,
							 GString		*string);

gboolean	seahorse_ops_text_clear_sign		(SeahorseContext	*sctx,
							 GString		*string);

gboolean	seahorse_ops_text_encrypt		(SeahorseContext	*sctx,
							 GString		*string,
							 GpgmeRecipients	recips);

gboolean	seahorse_ops_text_encrypt_sign		(SeahorseContext	*sctx,
							 GString		*string,
							 GpgmeRecipients	recips);

gboolean	seahorse_ops_text_decrypt		(SeahorseContext	*sctx,
							 GString		*string);

gboolean	seahorse_ops_text_verify		(SeahorseContext	*sctx,
							 GString		*string,
							 GpgmeSigStat		*status);

gboolean	seahorse_ops_text_decrypt_verify	(SeahorseContext	*sctx,
							 GString		*string,
							 GpgmeSigStat		*status);

#endif /* __SEAHORSE_OPS_TEXT_H__ */
