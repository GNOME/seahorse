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

#include <string.h>

#include "seahorse-op.h"
#include "seahorse-util.h"
#include "seahorse-vfs-data.h"

/* helper function for importing @data. @data will be released.
 * returns number of keys imported or -1. */
static gint
import_data (SeahorseContext *sctx, gpgme_data_t data, gpgme_error_t *err)
{
	gint keys = 0;
	SeahorseContext *new_ctx;
	
	new_ctx = seahorse_context_new ();
	*err = gpgme_op_import (new_ctx->ctx, data);
	if (GPG_IS_OK (*err)) {
	        gpgme_import_result_t result = gpgme_op_import_result (new_ctx->ctx);
		keys = result->considered;
	}
	gpgme_data_release (data);
	seahorse_context_destroy (new_ctx);
	
	if (GPG_IS_OK (*err)) {
		seahorse_context_keys_added (sctx, keys);
		return keys;
	}
	else
		g_return_val_if_reached (-1);
}

/**
 * seahorse_op_import_file:
 * @sctx: #SeahorseContext
 * @path: Path of file to import
 * @err: Optional error value
 *
 * Tries to import any keys contained in the file @path, saving any error in @err.
 *
 * Returns: Number of keys imported or -1 if import fails
 **/
gint
seahorse_op_import_file (SeahorseContext *sctx, const gchar *path, gpgme_error_t *err)
{
	gpgme_data_t data;
	gpgme_error_t error;
	
	if (err == NULL)
		err = &error;
	/* new data from file */
    data = seahorse_vfs_data_create (path, FALSE, err);
    g_return_val_if_fail (data != NULL, -1);
	
	return import_data (sctx, data, err);
}

/**
 * seahorse_op_import_text:
 * @sctx: #SeahorseContext
 * @text: Text to import
 * @err: Optional error value
 *
 * Tries to import any keys contained in @text, saving any errors in @err.
 *
 * Returns: Number of keys imported or -1 if import fails
 **/
gint
seahorse_op_import_text (SeahorseContext *sctx, const gchar *text, gpgme_error_t *err)
{
	gpgme_data_t data;
	gpgme_error_t error;
	
	if (err == NULL)
		err = &error;

    g_return_val_if_fail (text != NULL, 0);
         
	/* new data from text */
	*err = gpgme_data_new_from_mem (&data, text, strlen (text), TRUE);
	g_return_val_if_fail (GPG_IS_OK (*err), -1);
	
	return import_data (sctx, data, err);
}

/* helper function for exporting @recips. @recips will be released.
 * returns exported data. */
static gpgme_data_t
export_data (SeahorseContext *sctx, gpgme_key_t *recips, gpgme_error_t *err)
{
	gpgme_data_t data = NULL;
	gpgme_key_t *k;
    gpgme_error_t error;
   
   if (err == NULL)
       err = &error;
       
	*err = gpgme_data_new (&data);

	k = recips;
	while (*k) 
        *err = gpgme_op_export (sctx->ctx, (*(k++))->subkeys->fpr, 0, data);
 
	return data;
}

/**
 * seahorse_op_export_file:
 * @sctx: #SeahorseContext
 * @path: Path of a new file to export to
 * @recips: Keys to export
 * @err: Optional error value
 *
 * Tries to export @recips to the new file @path, saving an errors in @err.
 * @recips will be released upon completion.
 **/
void
seahorse_op_export_file (SeahorseContext *sctx, const gchar *path,
			 gpgme_key_t * recips, gpgme_error_t *err)
{
	gpgme_data_t data;
	gpgme_error_t error;
	
	if (err == NULL)
		err = &error;
	/* export data */
	data = export_data (sctx, recips, err);
	g_return_if_fail (GPG_IS_OK (*err));
	/* write to file */
	*err = seahorse_util_write_data_to_file (path, data);
}

/**
 * seahorse_op_export_text:
 * @sctx: #SeahorseContext
 * @recips: Keys to export
 * @err: Optional error value
 *
 * Tries to export @recips to text using seahorse_util_write_data_to_text(),
 * saving any errors in @err. @recips will be released upon completion.
 * ASCII Armor setting of @sctx will be ignored.
 *
 * Returns: The exported text or NULL if the operation fails
 **/
gchar*
seahorse_op_export_text (SeahorseContext *sctx, gpgme_key_t * recips, gpgme_error_t *err)
{
	gpgme_data_t data;
	gpgme_error_t error;
	gboolean armor;
	
	if (err == NULL)
		err = &error;
	
	/* export data with armor */
	armor = gpgme_get_armor (sctx->ctx);
	gpgme_set_armor (sctx->ctx, TRUE);
	data = export_data (sctx, recips, err);
	gpgme_set_armor (sctx->ctx, armor);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
	
	return seahorse_util_write_data_to_text (data);
}

/* common encryption function definition. */
typedef gpgme_error_t (* EncryptFunc) (gpgme_ctx_t ctx, gpgme_key_t recips[],
				       gpgme_encrypt_flags_t flags,
				       gpgme_data_t plain, gpgme_data_t cipher);

/* helper function for encrypting @plain to @recips using @func. @plain and
 * @recips will be released. returns the encrypted data. */
static gpgme_data_t
encrypt_data_common (SeahorseContext *sctx, gpgme_data_t plain, gpgme_key_t * recips,
		     EncryptFunc func, gpgme_error_t *err)
{
	gpgme_data_t cipher = NULL;
	gpgme_key_t * key;
	
	/* if don't already have an error, do encrypt */
	if (GPG_IS_OK (*err)) {
		*err = gpgme_data_new (&cipher);
		if (GPG_IS_OK (*err))
			*err = func (sctx->ctx, recips, GPGME_ENCRYPT_ALWAYS_TRUST, plain, cipher);
	}
	/* release plain and recips */
	gpgme_data_release (plain);
	key = recips;
	return cipher;
}

/* common file encryption helper to encrypt @path to @recips using @func.
 * returns the encrypted file's path. */
static gchar*
encrypt_file_common (SeahorseContext *sctx, const gchar *path, gpgme_key_t * recips,
		     EncryptFunc func, gpgme_error_t *err)
{
	gpgme_data_t plain, cipher;
	gpgme_error_t error;
	gchar *new_path = NULL;
	
	if (err == NULL)
		err = &error;

    plain = seahorse_vfs_data_create (path, FALSE, err);
    g_return_val_if_fail (plain != NULL, NULL);
 
	cipher = encrypt_data_common (sctx, plain, recips, func, err);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
	/* write cipher to file */
	new_path = seahorse_util_add_suffix (sctx->ctx, path, SEAHORSE_CRYPT_SUFFIX);
	*err = seahorse_util_write_data_to_file (new_path, cipher);
	
	/* free new_path if error */
	if (!GPG_IS_OK (*err)) {
		g_free (new_path);
		g_return_val_if_reached (NULL);
	}
	
	return new_path;
}

/* common text encryption helper to encrypt @text to @recips using @func.
 * ASCII Armor setting of @sctx is ignored. returns the encrypted text. */
static gchar*
encrypt_text_common (SeahorseContext *sctx, const gchar *text, gpgme_key_t * recips,
		     EncryptFunc func, gpgme_error_t *err)
{
	gpgme_data_t plain, cipher;
	gpgme_error_t error;
	gboolean armor;
	
	if (err == NULL)
		err = &error;
	/* new data form text */
	*err = gpgme_data_new_from_mem (&plain, text, strlen (text), TRUE);
	/* encrypt with armor */
	armor = gpgme_get_armor (sctx->ctx);
	gpgme_set_armor (sctx->ctx, TRUE);
	cipher = encrypt_data_common (sctx, plain, recips, func, err);
	gpgme_set_armor (sctx->ctx, armor);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
	
	return seahorse_util_write_data_to_text (cipher);
}

/**
 * seahorse_op_encrypt_file:
 * @sctx: #SeahorseContext
 * @path: Path of file to encrypt
 * @recips: Keys to encrypt to
 * @err: Optional error value
 *
 * Tries to encrypt the file @path to @recips, saving any errors in @err.
 * The encrypted file's encoding and suffix depends on the ASCII Armor setting of @sctx.
 * If ASCII Armor is enabled, the suffix will be '.asc'. Otherwise it will be '.gpg'.
 * @recips will be released upon completion.
 *
 * Returns: The path of the encrypted file or NULL if encryption fails
 **/
gchar*
seahorse_op_encrypt_file (SeahorseContext *sctx, const gchar *path,
			  gpgme_key_t * recips, gpgme_error_t *err)
{
	return encrypt_file_common (sctx, path, recips, gpgme_op_encrypt, err);
}

/**
 * seahorse_op_encrypt_text:
 * @sctx: #SeahorseContext
 * @text: Text to encrypt
 * @recips: Keys to encrypt with
 * @err: Optional error value
 *
 * Tries to encrypt @text to @recips, saving any errors in @err. The ASCII Armor
 * setting of @sctx will be ignored. @recips will be released upon completion.
 *
 * Returns: The encrypted text or NULL if encryption fails
 **/
gchar*
seahorse_op_encrypt_text (SeahorseContext *sctx, const gchar *text,
			  gpgme_key_t * recips, gpgme_error_t *err)
{
	return encrypt_text_common (sctx, text, recips, gpgme_op_encrypt, err);
}

/* helper function for signing @plain with @mode. @plain will be released.
 * returns the signed data. */
static gpgme_data_t
sign_data (SeahorseContext *sctx, gpgme_data_t plain, gpgme_sig_mode_t mode, gpgme_error_t *err)
{
	gpgme_data_t sig = NULL;
	
	*err = gpgme_data_new (&sig);
	if (GPG_IS_OK (*err))
		*err = gpgme_op_sign (sctx->ctx, plain, sig, mode);
	
	gpgme_data_release (plain);
	return sig;
}

/**
 * seahorse_op_sign_file:
 * @sctx: #SeahorseContext
 * @path: Path of file to sign
 * @err: Optional error value
 *
 * Tries to create a detached signature file for the file @path, saving any errors
 * in @err. If ASCII Armor is enabled, the signature file will have a suffix of '.asc',
 * otherwise the suffix will be '.sig'. Signing will be done by the default key
 * of @sctx.
 *
 * Returns: The path of the signature file or NULL if the operation fails
 **/
gchar*
seahorse_op_sign_file (SeahorseContext *sctx, const gchar *path, gpgme_error_t *err)
{
	gpgme_data_t plain, sig;
	gpgme_error_t error;
	gchar *new_path = NULL;
	
	if (err == NULL)
		err = &error;
	/* new data from file */
    plain = seahorse_vfs_data_create (path, FALSE, err);
    g_return_val_if_fail (plain != NULL, NULL);
  
	/* get detached signature */
	sig = sign_data (sctx, plain, GPGME_SIG_MODE_DETACH, err);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
	/* write sig to file */
	new_path = seahorse_util_add_suffix (sctx->ctx, path, SEAHORSE_SIG_SUFFIX);
	*err = seahorse_util_write_data_to_file (new_path, sig);
	
	/* free new_path if error */
	if (!GPG_IS_OK (*err)) {
		g_free (new_path);
		g_return_val_if_reached (NULL);
	}
	
	return new_path;
}

/**
 * seahorse_op_sign_text:
 * @sctx: #SeahorseContext
 * @text: Text to sign
 * @err: Optional error value
 *
 * Tries to sign @text using a clear text signature, saving any errors in @err.
 * Signing will be done by the default key of @sctx.
 *
 * Returns: The clear signed text or NULL if signing fails
 **/
gchar*
seahorse_op_sign_text (SeahorseContext *sctx, const gchar *text, gpgme_error_t *err)
{
	gpgme_data_t plain, sig;
	gpgme_error_t error;
	
	if (err == NULL)
		err = &error;
	/* new data from text */
	*err = gpgme_data_new_from_mem (&plain, text, strlen (text), TRUE);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
	/* clear sign data already ignores ASCII Armor */
	sig = sign_data (sctx, plain, GPGME_SIG_MODE_CLEAR, err);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
	
	return seahorse_util_write_data_to_text (sig);
}

/**
 * seahorse_op_encrypt_sign_file:
 * @sctx: #SeahorseContext
 * @path: Path of file to encrypt and sign
 * @recips: Keys to encrypt with
 * @err: Optional error value
 *
 * Tries to encrypt and sign the file @path to @recips, saving any errors in @err.
 * Signing will be done with the default key of @sctx. @recips will be released
 * upon completion.
 *
 * Returns: The path of the encrypted and signed file, or NULL if the operation fails
 **/
gchar*
seahorse_op_encrypt_sign_file (SeahorseContext *sctx, const gchar *path,
			       gpgme_key_t * recips, gpgme_error_t *err)
{
	return encrypt_file_common (sctx, path, recips, gpgme_op_encrypt_sign, err);
}

/**
 * seahorse_op_encrypt_sign_text:
 * @sctx: #SeahorseContext
 * @text: Text to encrypt and sign
 * @recips: Keys to encrypt with
 * @err: Optional error value
 *
 * Tries to encrypt and sign @text to @recips, saving any errors in @err.
 * Signing will be done with the default key of @sctx. @recips will be released
 * upon completion. The ASCII Armor setting of @sctx will be ignored.
 *
 * Returns: The encrypted and signed text or NULL if the operation fails
 **/
gchar*
seahorse_op_encrypt_sign_text (SeahorseContext *sctx, const gchar *text,
			       gpgme_key_t * recips, gpgme_error_t *err)
{
	return encrypt_text_common (sctx, text, recips, gpgme_op_encrypt_sign, err);
}

/**
 * seahorse_op_verify_file:
 * @sctx: #SeahorseContext
 * @path: Path of detached signature file
 * @status: Will contain the status of any verified signatures
 * @err: Optional error value
 *
 * Tries to verify the signature file @path, saving any errors in @err. The
 * signed file to check against is assumed to be @path without the suffix.
 * The status of any verified signatures will be saved in @status.
 **/
void
seahorse_op_verify_file (SeahorseContext *sctx, const gchar *path,
			 gpgme_verify_result_t *status, gpgme_error_t *err)
{
	gpgme_data_t sig, plain;
	gpgme_error_t error;
	gchar *plain_path;
	
	if (err == NULL)
		err = &error;
	/* new data from sig file */
    sig = seahorse_vfs_data_create (path, FALSE, err);
    g_return_if_fail (plain != NULL);

	/* new data from plain file */
	plain_path = seahorse_util_remove_suffix (path);
    plain = seahorse_vfs_data_create (plain_path, FALSE, err);
	g_free (plain_path);
  
	/* release sig data if error */
	if (!plain) {
		gpgme_data_release (sig);
		g_return_if_reached ();
	}
 
	/* verify sig file, release plain data */
    *err = gpgme_op_verify (sctx->ctx, sig, plain, NULL);
    *status = gpgme_op_verify_result (sctx->ctx);
    gpgme_data_release (sig); 
	gpgme_data_release (plain);
	g_return_if_fail (GPG_IS_OK (*err));
}

/**
 * seahorse_op_verify_text:
 * @sctx: #SeahorseContext
 * @text: Text to verify
 * @status: Will contain the status of any verified signatures
 * @err: Optional error value
 *
 * Tries to verify @text, saving any errors in @err. The status of any verified
 * signatures will be saved in @status. The ASCII Armor setting of @sctx will
 * be ignored.
 *
 * Returns: The verified text or NULL if the operation fails
 **/
gchar*
seahorse_op_verify_text (SeahorseContext *sctx, const gchar *text,
			 gpgme_verify_result_t *status, gpgme_error_t *err)
{
	gpgme_data_t sig, plain;
	gpgme_error_t error;
	gboolean armor;
	
	if (err == NULL)
		err = &error;
	/* new data from text */
	*err = gpgme_data_new_from_mem (&sig, text, strlen (text), TRUE);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
	/* new data to save verified text */
	*err = gpgme_data_new (&plain);
	/* free text data if error */
	if (!GPG_IS_OK (*err)) {
		gpgme_data_release (sig);
		g_return_val_if_reached (NULL);
	}
	/* verify data with armor */
	armor = gpgme_get_armor (sctx->ctx);
	gpgme_set_armor (sctx->ctx, TRUE);
    /* verify sig file, release plain data */
    *err = gpgme_op_verify (sctx->ctx, sig, NULL, plain);
    *status = gpgme_op_verify_result (sctx->ctx);
    gpgme_data_release (sig);     
	gpgme_set_armor (sctx->ctx, armor);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
	/* return verified text */
	return seahorse_util_write_data_to_text (plain);
}

/* helper function to decrypt and verify @cipher. @cipher will be released.
 * returns the verified data. */
static gpgme_data_t
decrypt_verify_data (SeahorseContext *sctx, gpgme_data_t cipher,
		     gpgme_verify_result_t *status, gpgme_error_t *err)
{
	gpgme_data_t plain;
	
	*err = gpgme_data_new (&plain);
	if (GPG_IS_OK (*err))
		*err = gpgme_op_decrypt_verify (sctx->ctx, cipher, plain);
    
    if (status)
    	*status = gpgme_op_verify_result (sctx->ctx);
     
	gpgme_data_release (cipher);
	return plain;
}

/**
 * seahorse_op_decrypt_verify_file:
 * @sctx: #SeahorseContext
 * @path: Path of file to decrypt and verify
 * @status: Will contain the status of any verified signatures
 * @err: Optional error value
 *
 * Tries to decrypt and verify the file @path, saving any errors in @err. The
 * status of any verified signatures will be saved in @status. A new file
 * will be created to contain the decrypted data. Its path will be the same as
 * @path without the suffix.
 *
 * Returns: The path of the decrypted file or NULL if the operation fails
 **/
gchar*
seahorse_op_decrypt_verify_file (SeahorseContext *sctx, const gchar *path,
				 gpgme_verify_result_t *status, gpgme_error_t *err)
{
	gpgme_data_t cipher, plain;
	gpgme_error_t error;
	gchar *new_path = NULL;
	
	if (err == NULL)
		err = &error;
	/* new data from file */
    cipher = seahorse_vfs_data_create (path, FALSE, err);
    g_return_val_if_fail (plain != NULL, NULL);

	/* verify data */
	plain = decrypt_verify_data (sctx, cipher, status, err);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
	/* write data to new file */
	new_path = seahorse_util_remove_suffix (path);
	*err = seahorse_util_write_data_to_file (new_path, plain);
	/* free path if there is an error */
	if (!GPG_IS_OK (*err)) {
		g_free (new_path);
		g_return_val_if_reached (NULL);
	}
	
	return new_path;
}

/**
 * seahorse_op_decrypt_verify_text:
 * @sctx: #SeahorseContext
 * @text: Text to decrypt and verify
 * @status: Will contain the status of any verified signatures
 * @err: Optional error value
 *
 * Tries to decrypt and verify @text, saving any errors in @err. The status of
 * any verified signatures will be saved in @status. The ASCII Armor setting
 * of @sctx will be ignored.
 *
 * Returns: The decrypted text or NULL if the operation fails
 **/
gchar*
seahorse_op_decrypt_verify_text (SeahorseContext *sctx, const gchar *text,
				 gpgme_verify_result_t *status, gpgme_error_t *err)
{
	gpgme_data_t cipher, plain;
	gpgme_error_t error;
	gboolean armor;
	
	if (err == NULL)
		err = &error;
	/* new data from text */
	*err = gpgme_data_new_from_mem (&cipher, text, strlen (text), TRUE);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
	/* get decrypted data with armor */
	armor = gpgme_get_armor (sctx->ctx);
	gpgme_set_armor (sctx->ctx, TRUE);
	plain = decrypt_verify_data (sctx, cipher, status, err);
	gpgme_set_armor (sctx->ctx, armor);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
	/* return text of decrypted data */
	return seahorse_util_write_data_to_text (plain);
}
