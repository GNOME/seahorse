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

#include <gnome.h>

#include "seahorse-ops-data.h"
#include "seahorse-ops-key.h"

/**
 * seahorse_ops_data_import:
 * @sctx: #SeahorseContext
 * @data: Data to import
 *
 * Imports @data, adds any keys to key ring,
 * then calls seahorse_context_show_status() to show number of keys added.
 * @data will be released when finished.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_data_import (SeahorseContext *sctx, const GpgmeData data)
{
	gboolean success;
	gint keys = 0;
	
	success = (gpgme_op_import_ext (sctx->ctx, data, &keys) == GPGME_No_Error);
	gpgme_data_release (data);
	
	if (success)
		seahorse_context_key_added (sctx);
	
	seahorse_context_show_status (sctx, g_strdup_printf (_("Imported %d Keys: "), keys), success);
	return success;
}

/**
 * seahorse_ops_data_export:
 * @sctx: #SeahorseContext
 * @data: Will contain exported key
 * @skey: Key to export
 *
 * Sugar method to export one key using seahorse_ops_data_export_multiple().
 * Exports @skey to @data.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_data_export (const SeahorseContext *sctx, GpgmeData *data, const SeahorseKey *skey)
{
	GpgmeRecipients recips;
	
	g_return_val_if_fail (gpgme_recipients_new (&recips) == GPGME_No_Error, FALSE);
	
	/* need to do so no memleak with recips */
	if (!seahorse_ops_key_recips_add (recips, skey) ||
	!seahorse_ops_data_export_multiple (sctx, data, recips)) {
		gpgme_recipients_release (recips);
		return FALSE;
	}
	
	return TRUE;
}

/**
 * seahorse_ops_data_export_multiple:
 * @sctx: #SeahorseContext
 * @data: Will contain exported keys
 * @recips: Keys to export
 *
 * Exports @recips to @data.
 * @recips will be released upon successful completion.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_data_export_multiple (const SeahorseContext *sctx, GpgmeData *data, GpgmeRecipients recips)
{
	g_return_val_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx), FALSE);
	g_return_val_if_fail (recips != NULL, FALSE);
	g_return_val_if_fail (gpgme_data_new (data) == GPGME_No_Error, FALSE);
	g_return_val_if_fail (gpgme_op_export (sctx->ctx, recips, *data) == GPGME_No_Error, FALSE);
	
	gpgme_recipients_release (recips);
	
	return TRUE;
}

/**
 * seahorse_ops_data_sign:
 * @sctx: #SeahorseContext with signers
 * @plain: Data to sign
 * @sig: Will contain signed data
 * @mode: Sign mode
 *
 * Signs @plain with signers in @sctx, then stores in @sig.
 * @plain will be released upon successful completion.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_data_sign (const SeahorseContext *sctx, GpgmeData plain, GpgmeData *sig, GpgmeSigMode mode)
{
	g_return_val_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx), FALSE);
	g_return_val_if_fail (plain != NULL, FALSE);
	g_return_val_if_fail (gpgme_data_new (sig) == GPGME_No_Error, FALSE);
	g_return_val_if_fail (gpgme_op_sign (sctx->ctx, plain, *sig, mode) == GPGME_No_Error, FALSE);
	
	gpgme_data_release (plain);
	
	return TRUE;
}

/**
 * seahorse_ops_data_encrypt:
 * @sctx: #SeahorseContext
 * @recips: Keys to encrypt to
 * @plain: Data to encrypt
 * @cipher: Will contain encrypted data
 *
 * Encrypts @plain to @recips, then stores in @cipher.
 * @recips and @plain will be released upon successful completion.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_data_encrypt (const SeahorseContext *sctx, GpgmeRecipients recips,
			   GpgmeData plain, GpgmeData *cipher)
{
	g_return_val_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx), FALSE);
	g_return_val_if_fail (recips != NULL && plain != NULL, FALSE);
	g_return_val_if_fail (gpgme_data_new (cipher) == GPGME_No_Error, FALSE);
	g_return_val_if_fail (gpgme_op_encrypt (sctx->ctx, recips, plain, *cipher) == GPGME_No_Error, FALSE);
	
	gpgme_recipients_release (recips);
	gpgme_data_release (plain);
	
	return TRUE;
}

/**
 * seahorse_ops_data_encrypt_sign:
 * @sctx: #SeahorseContext with signers
 * @recips: Keys to encrypt to
 * @plain: Data to encrypt
 * @cipher: Will contain encrypted & signed data
 *
 * Encrypts @plain to @recips, then signs with the signers in @sctx.
 * Encrypted and signed data will be stored in @cipher.
 * @data and @recips will be released upon successful completion.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_data_encrypt_sign (const SeahorseContext *sctx, GpgmeRecipients recips,
				GpgmeData plain, GpgmeData *cipher)
{
	g_return_val_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx), FALSE);
	g_return_val_if_fail (recips != NULL && plain != NULL, FALSE);
	g_return_val_if_fail (gpgme_data_new (cipher) == GPGME_No_Error, FALSE);
	g_return_val_if_fail (gpgme_op_encrypt_sign (sctx->ctx, recips, plain, *cipher) == GPGME_No_Error, FALSE);
	
	gpgme_recipients_release (recips);
	gpgme_data_release (plain);
	
	return TRUE;
}

/**
 * seahorse_ops_data_decrypt:
 * @sctx: #SeahorseContext
 * @cipher: Encrypted data
 * @plain: Will contain decrypted data
 *
 * Decrypts @cipher to @plain.
 * @cipher will be released upon successful completion.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_data_decrypt (const SeahorseContext *sctx, GpgmeData cipher, GpgmeData *plain)
{
	g_return_val_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx), FALSE);
	g_return_val_if_fail (cipher != NULL, FALSE);
	g_return_val_if_fail (gpgme_data_new (plain) == GPGME_No_Error, FALSE);
	g_return_val_if_fail (gpgme_op_decrypt (sctx->ctx, cipher, *plain) == GPGME_No_Error, FALSE);
	
	gpgme_data_release (cipher);
	
	return TRUE;
}

/**
 * seahorse_ops_data_verify:
 * @sctx: #SeahorseContext
 * @sig: Signature or signed data
 * @plain: If @sig is a signature, @plain is the signed data.
 * Otherwise, plain will contain the verified data from @sig.
 * @status: Will contain the verification status
 *
 * Verifies @sig and @plain, saving @status.
 * @sig will be released upon successful completion.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_data_verify (const SeahorseContext *sctx, GpgmeData sig, GpgmeData plain, GpgmeSigStat *status)
{
	g_return_val_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx), FALSE);
	g_return_val_if_fail (sig != NULL && plain != NULL, FALSE);
	g_return_val_if_fail (gpgme_op_verify (sctx->ctx, sig, plain, status) == GPGME_No_Error, FALSE);
	
	gpgme_data_release (sig);
	
	return TRUE;
}

/**
 * seahorse_ops_data_decrypt_verify:
 * @sctx: #SeahorseContext
 * @cipher: Encrypted, possibly signed, data
 * @plain: Will contain decrypted data
 * @status: Will contain the verification status of any signatures in @cipher
 *
 * Decrypts @cipher to @plain. The verification status of any signatures in
 * @cipher will be saved in @status.
 * @cipher will be released upon successful completion.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_data_decrypt_verify (const SeahorseContext *sctx, GpgmeData cipher, GpgmeData *plain, GpgmeSigStat *status)
{
	g_return_val_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx), FALSE);
	g_return_val_if_fail (cipher != NULL, FALSE);
	g_return_val_if_fail (gpgme_data_new (plain) == GPGME_No_Error, FALSE);
	g_return_val_if_fail (gpgme_op_decrypt_verify (sctx->ctx, cipher, *plain, status) == GPGME_No_Error, FALSE);
	
	gpgme_data_release (cipher);
	
	return TRUE;
}
