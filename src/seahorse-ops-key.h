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

#ifndef __SEAHORSE_OPS_KEY_H__
#define __SEAHORSE_OPS_KEY_H__

/* Key related operations.  SeahorseKey cannot depend on a SeahorseContext,
 * so separate ops are necessary.  Functions return TRUE iff no errors occur.
 * When completed, functions send the operation status to context. */

#include <glib.h>
#include <gpgme.h>
#include <time.h>

#include "seahorse-context.h"
#include "seahorse-key.h"

/* Available key types for key generation */
typedef enum {
	DSA_ELGAMAL,
	DSA,
	RSA
} SeahorseKeyType;

/* Possible lengths of keys */
typedef enum {
	DSA_MIN = 512,
	DSA_MAX = 1024,
	ELGAMAL_MIN = 768,
	RSA_MIN = 1024,
	LENGTH_MAX = 4096
} SeahorseKeyLength;

/* Removes key from key ring, then destroys it. */
gboolean	seahorse_ops_key_delete		(SeahorseContext	*sctx,
						 SeahorseKey		*skey);

/* Generates a key with parameters, then adds it to key ring. */
gboolean	seahorse_ops_key_generate	(SeahorseContext	*sctx,
						 const gchar		*name,
						 const gchar		*email,
						 const gchar		*comment,
						 const gchar		*passphrase,
						 const SeahorseKeyType	type,
						 const guint		length,
						 const time_t		expires);

gboolean	seahorse_ops_key_export_server	(SeahorseContext	*sctx,
						 SeahorseKey		*skey,
						 const gchar		*server);

gboolean	seahorse_ops_key_import_server	(SeahorseContext	*sctx,
						 const gchar		*keyid,
						 const gchar		*server);

/* Adds key to recipient list, giving full validity if necessary. */
gboolean	seahorse_ops_key_recips_add	(GpgmeRecipients	recips,
						 const SeahorseKey	*skey);

/* Sets key's owner trust to trust */
gboolean	seahorse_ops_key_set_trust	(SeahorseContext	*sctx,
						 SeahorseKey		*skey,
						 GpgmeValidity		trust);

/* Changes key's primary expiration date */
gboolean	seahorse_ops_key_set_expires	(SeahorseContext	*sctx,
						 SeahorseKey		*skey,
						 time_t			expires);

/* Enables/disables key */
gboolean	seahorse_ops_key_set_disabled	(SeahorseContext	*sctx,
						 SeahorseKey		*skey,
						 gboolean		disabled);

/* Begins passphrase changing dialogs for key */
gboolean	seahorse_ops_key_change_pass	(SeahorseContext	*sctx,
						 SeahorseKey		*skey);

#endif /* __SEAHORSE_OPS_KEY_H__ */
