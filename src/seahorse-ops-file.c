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
#include <glib.h>

#include "seahorse-ops-file.h"
#include "seahorse-context.h"
#include "seahorse-key.h"
#include "seahorse-ops-data.h"

#define ASC ".asc"
#define GPG ".gpg"
#define SIG ".sig"

/**
 * seahorse_ops_file_add_suffix:
 * @file: Original filename
 * @type: Type of suffix
 * @sctx: #SeahorseContext
 *
 * Appends the correct suffix to @file, depending on @type and the ascii armor
 * setting of @sctx.
 * If ascii armor is enabled, the suffix will be .asc. Otherwise, it will be
 * .sig for #SEAHORSE_SIG_FILE, or .gpg for #SEAHORSE_CRYPT_FILE.
 *
 * Returns: @file with a suffix
 **/
gchar*
seahorse_ops_file_add_suffix (const gchar *file, SeahorseFileType type, SeahorseContext *sctx)
{
	if (gpgme_get_armor (sctx->ctx))
		return g_strdup_printf ("%s%s", file, ASC);
	else if (type == SEAHORSE_SIG_FILE)
		return g_strdup_printf ("%s%s", file, SIG);
	else
		return g_strdup_printf ("%s%s", file, GPG);
}

/**
 * seahorse_ops_file_del_suffix:
 * @file: Original filename
 * @type: Type of suffix
 *
 * Checks @file with seahorse_ops_file_check_suffix(), then returns @file
 * with the suffix removed.
 *
 * Returns: @file without a suffix, or NULL if @file had a bad suffix
 **/
gchar*
seahorse_ops_file_del_suffix (const gchar *file, SeahorseFileType type)
{
	if (seahorse_ops_file_check_suffix (file, type))
		return g_strndup (file, strlen (file) - 4);
	else
		return NULL;
}

/* Writes data to a file, borrowed from gpa */
static gboolean
write_data (GpgmeData data, const gchar *file)
{
	FILE *fp;
	gchar *buffer;
	gint nread;
	
	fp = fopen (file, "a");
	if (fp == NULL)
		return FALSE;
	
	buffer = g_new0 (gchar, 128);
	
	while (gpgme_data_read (data, buffer, sizeof (buffer), &nread) == GPGME_No_Error)
		fwrite (buffer, nread, 1, fp);
	
	fflush (fp);
	fclose (fp);
	gpgme_data_release (data);
	
	return TRUE;
}

/**
 * seahorse_ops_file_check_suffix:
 * @file: Filename to check
 * @type: Suffix type
 *
 * Checks the suffix of @file against @type. The suffix must be .asc,
 * or .sig for #SEAHORSE_SIG_FILE, or .gpg for #SEAHORSE_CRYPT_FILE.
 *
 * Returns: %TRUE if suffix is good, %FALSE otherwise
 **/
gboolean
seahorse_ops_file_check_suffix (const gchar *file, SeahorseFileType type)
{
	gchar *suffix;
	
	if (type == SEAHORSE_SIG_FILE)
		suffix = SIG;
	else
		suffix = GPG;
	
	return (g_pattern_match_simple (g_strdup_printf ("*%s", ASC), file) ||
		g_pattern_match_simple (g_strdup_printf ("*%s", suffix), file));
}

/**
 * seahorse_ops_file_import:
 * @sctx: #SeahorseContext
 * @file: Filename of file to import
 *
 * Imports @file using seahorse_ops_data_import().
 * seahorse_context_show_status() will be called upon completion.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_file_import (SeahorseContext *sctx, const gchar *file)
{
	GpgmeData data;
	gboolean success;
	
	success = ((gpgme_data_new_from_file (&data, file, TRUE) == GPGME_No_Error) &&
		seahorse_ops_data_import (sctx, data));
	
	if (!success)
		gpgme_data_release (data);
	
	return success;
}

/**
 * seahorse_ops_file_export:
 * @sctx: #SeahorseContext
 * @file: Filename of file to export to
 * @skey: Key to export
 *
 * Sugar to export one key using seahorse_ops_file_export_multiple().
 * Exports @skey to @file.
 * Call seahorse_context_show_status() upon completion.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_file_export (SeahorseContext *sctx, const gchar *file, const SeahorseKey *skey)
{
	GpgmeData data;
	gboolean success;
	
	success = (seahorse_ops_data_export (sctx, &data, skey) &&
		write_data (data, file));
	
	if (!success)
		gpgme_data_release (data);
	
	seahorse_context_show_status (sctx, _("Export File"), success);
	return success;
}

/**
 * seahorse_ops_file_export_multiple:
 * @sctx: #SeahorseContext
 * @file: Filename of file to export to
 * @recips: Keys to export
 *
 * Exports @recips to @file using seahorse_ops_data_export_multiple().
 * Calls seahorse_context_show_status() upon completion.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_file_export_multiple (SeahorseContext *sctx, const gchar *file, GpgmeRecipients recips)
{
	GpgmeData data;
	gboolean success;
	
	success = (seahorse_ops_data_export_multiple (sctx, &data, recips) &&
		write_data (data, file));
	
	if (!success)
		gpgme_data_release (data);
	
	seahorse_context_show_status (sctx, _("Export File"), success);
	return success;
}

/**
 * seahorse_ops_file_sign:
 * @sctx: #SeahorseContext with signers
 * @file: Filename of file to sign
 *
 * Signs file with signers in @sctx, using seahorse_ops_data_sign().
 * Signed file will be stored at the same location as @file,
 * but with a .sig or .asc extension, depending on the ascii armor setting
 * of @sctx. See seahorse_ops_file_add_suffix() and #SEAHORSE_SIG_FILE.
 * Calls seahorse_context_show_status() upon completion.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_file_sign (SeahorseContext *sctx, const gchar *file)
{
	GpgmeData plain;
	GpgmeData sig;
	gboolean success;
	
	success = ((gpgme_data_new_from_file (&plain, file, TRUE) == GPGME_No_Error) &&
		seahorse_ops_data_sign (sctx, plain, &sig, GPGME_SIG_MODE_DETACH) &&
		write_data (sig, seahorse_ops_file_add_suffix (file, SEAHORSE_SIG_FILE, sctx)));
	
	if (!success) {
		gpgme_data_release (plain);
		gpgme_data_release (sig);
	}
	
	seahorse_context_show_status (sctx, _("Detached Signature"), success);
	return success;
}

/**
 * seahorse_ops_file_encrypt:
 * @sctx: #SeahorseContext
 * @file: Filename of file to encrypt
 * @recips: Keys to encrypt to
 *
 * Encrypts @file to @recips using seahorse_ops_data_encrypt().
 * Encrypted file will be stored at the same location as @file,
 * but with a .gpg or .asc extension, depending on the ascii armor setting of
 * @sctx. See seahorse_ops_file_add_suffix() and #SEAHORSE_CRYPT_FILE.
 * Calls seahorse_context_show_status() upon completion.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_file_encrypt (SeahorseContext *sctx, const gchar *file, GpgmeRecipients recips)
{
	GpgmeData plain;
	GpgmeData cipher;
	gboolean success;
	
	success = ((gpgme_data_new_from_file (&plain, file, TRUE) == GPGME_No_Error) &&
		seahorse_ops_data_encrypt (sctx, recips, plain, &cipher) &&
		write_data (cipher, seahorse_ops_file_add_suffix (file, SEAHORSE_CRYPT_FILE, sctx)));
	
	if (!success) {
		gpgme_data_release (plain);
		gpgme_data_release (cipher);
	}
	
	seahorse_context_show_status (sctx, _("Encrypt File"), success);
	return success;
}

/**
 * seahorse_ops_file_encrypt_sign:
 * @sctx: #SeahorseContext with signers
 * @file: Filename of file to encrypt and sign
 * @recips: Keys to encrypt to
 *
 * Encrypts @file to @recips, then signs with signers in @sctx,
 * using seahorse_ops_data_encrypt_sign(). Encrypted and signed file will be
 * stored at the same location as @file, but with a .gpg or .asc extension,
 * depending on the ascii armor setting of @sctx.
 * See seahorse_ops_file_add_suffix() and #SEAHORSE_CRYPT_FILE.
 * Calls seahorse_context_show_status() upon completion.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_file_encrypt_sign (SeahorseContext *sctx, const gchar *file, GpgmeRecipients recips)
{
	GpgmeData plain;
	GpgmeData cipher;
	gboolean success;
	
	success = ((gpgme_data_new_from_file (&plain, file, TRUE) == GPGME_No_Error) &&
		seahorse_ops_data_encrypt_sign (sctx, recips, plain, &cipher) &&
		write_data (cipher, seahorse_ops_file_add_suffix (file, SEAHORSE_CRYPT_FILE, sctx)));
	
	if (!success) {
		gpgme_data_release (plain);
		gpgme_data_release (cipher);
	}
	
	seahorse_context_show_status (sctx, _("Encrypt & Sign File"), success);
	return success;
}

/**
 * seahorse_ops_file_decrypt:
 * @sctx: #SeahorseContext
 * @file: Filename of encrypted file. The file's extension must be .gpg or .asc.
 *
 * Decrypts @file using seahorse_ops_data_decrypt(). The decrypted file will be
 * stored at the same location as @file, but without the .gpg or .asc extension.
 * See seahorse_ops_file_del_suffix() and #SEAHORSE_CRYPT_FILE.
 * Calls seahorse_context_show_status() upon completion.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_file_decrypt (SeahorseContext *sctx, const gchar *file)
{
	GpgmeData cipher;
	GpgmeData plain;
	gboolean success;
	
	success = ((gpgme_data_new_from_file (&cipher, file, TRUE) == GPGME_No_Error) &&
		seahorse_ops_data_decrypt (sctx, cipher, &plain) &&
		write_data (plain, seahorse_ops_file_del_suffix (file, SEAHORSE_CRYPT_FILE)));
	
	if (!success) {
		gpgme_data_release (cipher);
		gpgme_data_release (plain);
	}

	seahorse_context_show_status (sctx, _("Decrypt File"), success);
	return success;
}

/**
 * seahorse_ops_file_verify:
 * @sctx: #SeahorseContext
 * @file: Filename of signature file. The file's extension must be .sig or .asc.
 * @status: Will contain the verification status of @file
 *
 * Verifies the signature @file, assuming the signed data file is @file without
 * the extension, using seahorse_ops_file_del_suffix(), #SEAHORSE_SIG_FILE,
 * and seahorse_ops_data_verify().
 * Calls seahorse_context_show_status() upon completion.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_file_verify (SeahorseContext *sctx, const gchar *file, GpgmeSigStat *status)
{
	GpgmeData sig;
	GpgmeData plain;
	gboolean success;
	
	success = ((gpgme_data_new_from_file (&plain,
		seahorse_ops_file_del_suffix (file, SEAHORSE_SIG_FILE), TRUE) == GPGME_No_Error) &&
		(gpgme_data_new_from_file (&sig, file, TRUE) == GPGME_No_Error) &&
		seahorse_ops_data_verify (sctx, sig, plain, status));
	
	gpgme_data_release (plain);
	
	if (!success) {
		gpgme_data_release (sig);
		gpgme_data_release (plain);
	}
	
	seahorse_context_show_status (sctx, _("Verify Signature File"), success);
	return success;
}

/**
 * seahorse_ops_file_decrypt_verify:
 * @sctx: #SeahorseContext
 * @file: Filename of encrypted, and possibly signed, file.
 * The file's extension must be .gpg or .asc.
 * @status: Will contain the verification status of any signatures in @file
 *
 * Decrypts @file, and verifies any signatures it contains.
 * The decrypted data will be stored at the same location as @file,
 * but without the .gpg or .asc extension.
 * Calls seahorse_context_show_status() upon completion.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_file_decrypt_verify (SeahorseContext *sctx, const gchar *file, GpgmeSigStat *status)
{
	GpgmeData cipher;
	GpgmeData plain;
	gboolean success;
	
	success = ((gpgme_data_new_from_file (&cipher, file, TRUE) == GPGME_No_Error) &&
		seahorse_ops_data_decrypt_verify (sctx, cipher, &plain, status) &&
		write_data (plain, seahorse_ops_file_del_suffix (file, SEAHORSE_CRYPT_FILE)));
	
	if (!success) {
		gpgme_data_release (cipher);
		gpgme_data_release (plain);
	}
	
	seahorse_context_show_status (sctx, _("Decrypt & Verify File"), success);
	return success;
}

