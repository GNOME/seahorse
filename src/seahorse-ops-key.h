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

/* Key types corresponding to gnupg numbering */
typedef enum {
	/* DSA key with ElGamal subkey. The DSA key will have length of 1024,
	 * while the ElGamal length is variable within #ELGAMAL_MIN and
	 * #LENGTH_MAX. Only used in seahorse_ops_key_generate().
	 */
	DSA_ELGAMAL = 1,
	/* DSA key, sign only. Can be a subkey or a primary key.
	 * See #DSA_MIN and #DSA_MAX.
	 */
	DSA = 2,
	/* ElGamal subkey, encrypt only. See #ELGAMAL_MIN and #LENGTH_MAX.
	 * Only used in seahorse_ops_key_add_subkey().
	 */
	ELGAMAL = 3,
	/* RSA key, sign only. Can be a subkey or a primary key.
	 * See #RSA_MIN and #RSA_MAX.
	 */
	RSA_SIGN = 5,
	/* RSA subkey, encrypt only. See #RSA_MIN and #RSA_MAX.
	 * Only used in seahorse_ops_key_add_subkey().
	 */
	RSA_ENCRYPT = 6
} SeahorseKeyType;

/* Length ranges for key types */
typedef enum {
	/* Length range for #DSA. */
	DSA_MIN = 512,
	DSA_MAX = 1024,
	/* Minimum length for #ELGAMAL. Maximum length is #LENGTH_MAX. */
	ELGAMAL_MIN = 768,
	/* Minimum length of #RSA_SIGN and #RSA_ENCRYPT. Maximum length is
	 * #LENGTH_MAX.
	 */
	RSA_MIN = 1024,
	/* Maximum length for #ELGAMAL, #RSA_SIGN, and #RSA_ENCRYPT. */
	LENGTH_MAX = 4096
} SeahorseKeyLength;

gboolean	seahorse_ops_key_delete		(SeahorseContext	*sctx,
						 SeahorseKey		*skey);

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

gboolean	seahorse_ops_key_recips_add	(GpgmeRecipients	recips,
						 const SeahorseKey	*skey);

gboolean	seahorse_ops_key_set_trust	(SeahorseContext	*sctx,
						 SeahorseKey		*skey,
						 GpgmeValidity		trust);

gboolean	seahorse_ops_key_set_expires	(SeahorseContext	*sctx,
						 SeahorseKey		*skey,
						 time_t			expires);

gboolean	seahorse_ops_key_set_disabled	(SeahorseContext	*sctx,
						 SeahorseKey		*skey,
						 gboolean		disabled);

gboolean	seahorse_ops_key_change_pass	(SeahorseContext	*sctx,
						 SeahorseKey		*skey);

gboolean	seahorse_ops_key_check_email	(const gchar		*email);

gboolean	seahorse_ops_key_add_uid	(SeahorseContext	*sctx,
						 SeahorseKey		*skey,
						 const gchar		*name,
						 const gchar		*email,
						 const gchar		*comment);

#endif /* __SEAHORSE_OPS_KEY_H__ */
