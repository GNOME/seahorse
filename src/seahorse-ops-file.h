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

#ifndef __SEAHORSE_OPS_FILE_H__
#define __SEAHORSE_OPS_FILE_H__

/* File related operations, mainly
 * conveniance wrappers around gpgme.
 * Also see corresponding functions in seahorse-ops-data. */

#include <glib.h>
#include <gpgme.h>

#include "seahorse-context.h"
#include "seahorse-key.h"

/* File type suffixes for non ascii armored files.
 * Ascii armored files have a '.asc' suffix. */
typedef enum {
	SIG_FILE, /* Has a '.sig' suffix */
	CRYPT_FILE /* Has a '.gpg' suffix */
} SeahorseFileType;

/* Returns TRUE if file name has a valid suffix */
gboolean	seahorse_ops_file_check_suffix		(const gchar		*file,
							 SeahorseFileType	type);

/* Imports all keys contained in file */
gboolean	seahorse_ops_file_import		(SeahorseContext	*sctx,
						 	const gchar		*file);

/* Exports key to file */
gboolean	seahorse_ops_file_export		(SeahorseContext	*sctx,
						 	const gchar		*file,
						 	const SeahorseKey	*skey);

/* Exports all keys in recips to file */
gboolean	seahorse_ops_file_export_multiple	(SeahorseContext	*sctx,
							 const gchar		*file,
							 GpgmeRecipients	recips);

/* All of the following operations make use of file suffixes. */

/* Creates a detached signature for file using current signers.
 * Signature file will be 'file.asc' or 'file.sig'. */
gboolean	seahorse_ops_file_sign			(SeahorseContext	*sctx,
							 const gchar		*file);

/* Encrypts file to recipients.  Encrypted file will be 'file.asc' or 'file.gpg'. */
gboolean	seahorse_ops_file_encrypt		(SeahorseContext	*sctx,
							 const gchar		*file,
							 GpgmeRecipients	recips);

/* Decrypts file.  File must have a valid suffix.
 * The decrypted file name will not have the outermost suffix (foo.asc -> foo). */
gboolean	seahorse_ops_file_decrypt		(SeahorseContext	*sctx,
							 const gchar		*file);

/* Verifies a detached signature.  The file must have a valid suffix
 * and the data file must be the file name without the suffix (foo.sig & foo). */
gboolean	seahorse_ops_file_verify		(SeahorseContext	*sctx,
							 const gchar		*file,
							 GpgmeSigStat		*status);

/* Decrypts file, then verifies any signatures.  File must have a valid suffix,
 * decrypted file name will not have the outermost suffix.  Data should be as above in verify. */
gboolean	seahorse_ops_file_decrypt_verify	(SeahorseContext	*sctx,
							 const gchar		*file,
							 GpgmeSigStat		*status);

#endif /* __SEAHORSE_OPS_FILE_H__ */
