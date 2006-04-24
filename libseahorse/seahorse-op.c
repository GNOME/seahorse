/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2005 Nate Nielsen
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

#include "seahorse-gpgmex.h"
#include "seahorse-op.h"
#include "seahorse-util.h"
#include "seahorse-context.h"
#include "seahorse-pgp-source.h"
#include "seahorse-pgp-key.h"
#include "seahorse-vfs-data.h"
#include "seahorse-gconf.h"

/* helper function for importing @data. @data will be released.
 * returns number of keys imported or -1. */
static gint
import_data (SeahorsePGPSource *psrc, gpgme_data_t data, 
             GError **err)
{
    SeahorseOperation *operation;
    GList *keylist;
	gint keys = 0;
    
    g_return_val_if_fail (!err || !err[0], -1);

    operation = seahorse_key_source_import (SEAHORSE_KEY_SOURCE (psrc), data);
    g_return_val_if_fail (operation != NULL, -1);
    
    seahorse_operation_wait (operation);
    
    if (seahorse_operation_is_successful (operation)) {
        keylist = (GList*)seahorse_operation_get_result (operation);
        keys = g_list_length (keylist);
    } else {
        seahorse_operation_steal_error (operation, err);
        keys = -1;
    }
    
    g_object_unref (operation);
    gpgmex_data_release (data);

    return keys;    
}

/**
 * seahorse_op_import_file:
 * @sksrc: #SeahorseKeySource to import into
 * @path: Path of file to import
 * @err: Optional error value
 *
 * Tries to import any keys contained in the file @path, saving any error in @err.
 *
 * Returns: Number of keys imported or -1 if import fails
 **/
gint
seahorse_op_import_file (SeahorsePGPSource *psrc, const gchar *path, GError **err)
{
    gpgme_data_t data;
  
    /* new data from file */
    data = seahorse_vfs_data_create (path, SEAHORSE_VFS_READ, err);
    if (!data) 
        return -1;
    
    return import_data (psrc, data, err);
}

/**
 * seahorse_op_import_text:
 * @sksrc: #SeahorseKeySource to import into 
 * @text: Text to import
 * @err: Optional error value
 *
 * Tries to import any keys contained in @text, saving any errors in @err.
 *
 * Returns: Number of keys imported or -1 if import fails
 **/
gint
seahorse_op_import_text (SeahorsePGPSource *psrc, const gchar *text, GError **err)
{
    gpgme_data_t data;
    
    g_return_val_if_fail (text != NULL, 0);
         
    /* new data from text */
    data = gpgmex_data_new_from_mem (text, strlen (text), TRUE);
    return import_data (psrc, data, err);
}

/* common encryption function definition. */
typedef gpgme_error_t (* EncryptFunc) (gpgme_ctx_t ctx, gpgme_key_t recips[],
				       gpgme_encrypt_flags_t flags,
				       gpgme_data_t plain, gpgme_data_t cipher);

/* helper function for encrypting @plain to @recips using @func. @plain and
 * @keys will be released.*/
static void
encrypt_data_common (SeahorsePGPSource *psrc, GList *keys, gpgme_data_t plain, 
                     gpgme_data_t cipher, EncryptFunc func, gboolean force_armor, 
                     gpgme_error_t *err)
{
    SeahorseKey *skey;
    gpgme_key_t *recips;

    /* if don't already have an error, do encrypt */
    if (GPG_IS_OK (*err)) {

        /* Add the default key if set and necessary */
        if (seahorse_gconf_get_boolean (ENCRYPTSELF_KEY)) {
            skey = seahorse_context_get_default_key (SCTX_APP ());
            if (skey != NULL)
                keys = g_list_append (keys, skey);
        }

        /* Make keys into the right format */
        recips = seahorse_util_keylist_to_keys (keys);
        
        gpgme_set_armor (psrc->gctx, force_armor || seahorse_gconf_get_boolean (ARMOR_KEY));
        *err = func (psrc->gctx, recips, GPGME_ENCRYPT_ALWAYS_TRUST, plain, cipher);
        
        seahorse_util_free_keys (recips);
    }
    
    /* release plain  */
    gpgmex_data_release (plain);
}

/* common text encryption helper to encrypt @text to @recips using @func.
 * returns the encrypted text. */
static gchar*
encrypt_text_common (SeahorsePGPSource *psrc, GList *keys, const gchar *text, 
                     EncryptFunc func, gpgme_error_t *err)
{
    gpgme_data_t plain, cipher;
    gpgme_error_t error;
    
    if (err == NULL)
        err = &error;
    
    /* new data form text */
    plain = gpgmex_data_new_from_mem (text, strlen (text), TRUE);
    cipher = gpgmex_data_new ();
   
    /* encrypt with armor */
    gpgme_set_textmode (psrc->gctx, TRUE);    
    encrypt_data_common (psrc, keys, plain, cipher, func, TRUE, err);
    if (!GPG_IS_OK (*err))
        return NULL;
    
    return seahorse_util_write_data_to_text (cipher, TRUE);
}

/**
 * seahorse_op_encrypt_text:
 * @keys: List of #SeahorseKey objects to encrypt to
 * @text: Text to encrypt
 * @err: Optional error value
 *
 * Tries to encrypt @text to @recips, saving any errors in @err. @recips will be 
 * released upon completion.
 *
 * Returns: The encrypted text or NULL if encryption fails
 **/
gchar*
seahorse_op_encrypt_text (GList *keys, const gchar *text, gpgme_error_t *err)
{
    SeahorsePGPSource *psrc;

    /* TODO: When other key types are supported we need to make sure they're
       all from the same PGP source */
    g_return_val_if_fail (keys && SEAHORSE_IS_PGP_KEY (keys->data), NULL);
    psrc = SEAHORSE_PGP_SOURCE (seahorse_key_get_source (SEAHORSE_KEY (keys->data)));
    g_return_val_if_fail (psrc != NULL && SEAHORSE_IS_PGP_SOURCE (psrc), NULL);
    
	return encrypt_text_common (psrc, keys, text, gpgme_op_encrypt, err);
}

/* Helper function to set a key pair as the signer for its keysource */
static void
set_signer (SeahorsePGPSource *psrc, SeahorsePGPKey *signer)
{
    gpgme_signers_clear (psrc->gctx);
    if (signer)
        gpgme_signers_add (psrc->gctx, signer->seckey);
}

/* helper function for signing @plain with @mode. @plain will be released. */
static void
sign_data (SeahorsePGPSource *psrc, gpgme_data_t plain, gpgme_data_t sig,
           gpgme_sig_mode_t mode, gpgme_error_t *err)
{
    *err = gpgme_op_sign (psrc->gctx, plain, sig, mode);
    gpgmex_data_release (plain);
}

/**
 * seahorse_op_sign_text:
 * @sksrc: SeahorseKeyStore to sign with 
 * @text: Text to sign
 * @err: Optional error value
 *
 * Tries to sign @text using a clear text signature, saving any errors in @err.
 * Signing will be done by the default key.
 *
 * Returns: The clear signed text or NULL if signing fails
 **/
gchar*
seahorse_op_sign_text (SeahorsePGPKey *signer, const gchar *text, 
                       gpgme_error_t *err)
{
    SeahorsePGPSource *psrc;
    gpgme_data_t plain, sig;
    gpgme_error_t error;

    g_return_val_if_fail (signer && SEAHORSE_IS_PGP_KEY (signer), NULL);
    g_return_val_if_fail (seahorse_key_get_flags (SEAHORSE_KEY (signer)) & SKEY_FLAG_CAN_SIGN, NULL);
    
    if (err == NULL)
        err = &error;

    psrc = SEAHORSE_PGP_SOURCE (seahorse_key_get_source (SEAHORSE_KEY (signer)));
    g_return_val_if_fail (psrc != NULL && SEAHORSE_IS_PGP_SOURCE (psrc), NULL);
    
    set_signer (psrc, signer);
            
    /* new data from text */
    plain = gpgmex_data_new_from_mem (text, strlen (text), TRUE);
    sig = gpgmex_data_new ();
    
    /* clear sign data already ignores ASCII Armor */
    gpgme_set_textmode (psrc->gctx, TRUE);
    gpgme_set_armor (psrc->gctx, TRUE);
    sign_data (psrc, plain, sig, GPGME_SIG_MODE_CLEAR, err);
    if (!GPG_IS_OK (*err))
        return NULL;
    return seahorse_util_write_data_to_text (sig, TRUE);
}

/**
 * seahorse_op_encrypt_sign_text:
 * @keys: List of #SeahorseKey objects to encrypt to
 * @text: Text to encrypt and sign
 * @recips: Keys to encrypt with
 * @err: Optional error value
 *
 * Tries to encrypt and sign @text to @recips, saving any errors in @err.
 * Signing will be done with the default key. @recips will be released
 * upon completion.
 *
 * Returns: The encrypted and signed text or NULL if the operation fails
 **/
gchar*
seahorse_op_encrypt_sign_text (GList *keys, SeahorsePGPKey *signer, 
                               const gchar *text, gpgme_error_t *err)
{
    SeahorsePGPSource *psrc;
    
    g_return_val_if_fail (signer && SEAHORSE_IS_PGP_KEY (signer), NULL);
    g_return_val_if_fail (seahorse_key_get_flags (SEAHORSE_KEY (signer)) & SKEY_FLAG_CAN_SIGN, NULL);

    /* TODO: When other key types are supported we need to make sure they're
       all from the same PGP source */
    g_return_val_if_fail (keys && SEAHORSE_IS_PGP_KEY (keys->data), NULL);
    psrc = SEAHORSE_PGP_SOURCE (seahorse_key_get_source (SEAHORSE_KEY (keys->data)));
    g_return_val_if_fail (psrc != NULL && SEAHORSE_IS_PGP_SOURCE (psrc), NULL);

    set_signer (psrc, signer);
	return encrypt_text_common (psrc, keys, text, gpgme_op_encrypt_sign, err);
}

/**
 * seahorse_op_verify_text:
 * @sksrc: #SeahorseKeySource to verify against
 * @text: Text to verify
 * @status: Will contain the status of any verified signatures
 * @err: Optional error value
 *
 * Tries to verify @text, saving any errors in @err. The status of any verified
 * signatures will be saved in @status. 
 *
 * Returns: The verified text or NULL if the operation fails
 **/
gchar*
seahorse_op_verify_text (SeahorsePGPSource *psrc, const gchar *text,
                         gpgme_verify_result_t *status, gpgme_error_t *err)
{
    gpgme_data_t sig, plain;
    gpgme_error_t error;
    
    if (err == NULL)
        err = &error;
    /* new data from text */
    sig = gpgmex_data_new_from_mem (text, strlen (text), TRUE);
    /* new data to save verified text */
    plain = gpgmex_data_new ();

    /* verify data with armor */
    gpgme_set_armor (psrc->gctx, TRUE);
    /* verify sig file, release plain data */
    *err = gpgme_op_verify (psrc->gctx, sig, NULL, plain);
    *status = gpgme_op_verify_result (psrc->gctx);
    gpgmex_data_release (sig);     
    if (!GPG_IS_OK (*err))
        return NULL;
    return seahorse_util_write_data_to_text (plain, TRUE);
}

/* helper function to decrypt and verify @cipher. @cipher will be released. */
static void
decrypt_verify_data (SeahorsePGPSource *psrc, gpgme_data_t cipher,
                     gpgme_data_t plain, gpgme_verify_result_t *status, 
                     gpgme_error_t *err)
{
    *err = gpgme_op_decrypt_verify (psrc->gctx, cipher, plain);
        
    if (status)
        *status = gpgme_op_verify_result (psrc->gctx);
     
    gpgmex_data_release (cipher);
}

/**
 * seahorse_op_decrypt_verify_text:
 * @sksrc: #SeahorseKeySource to verify against
 * @text: Text to decrypt and verify
 * @status: Will contain the status of any verified signatures
 * @err: Optional error value
 *
 * Tries to decrypt and verify @text, saving any errors in @err. The status of
 * any verified signatures will be saved in @status. 
 *
 * Returns: The decrypted text or NULL if the operation fails
 **/
gchar*
seahorse_op_decrypt_verify_text (SeahorsePGPSource *psrc, const gchar *text,
                                 gpgme_verify_result_t *status, gpgme_error_t *err)
{
    gpgme_data_t cipher, plain;
    gpgme_error_t error;
    
    if (err == NULL)
        err = &error;
    /* new data from text */
    cipher = gpgmex_data_new_from_mem (text, strlen (text), TRUE);
    plain = gpgmex_data_new ();
    
    /* get decrypted data with armor */
    gpgme_set_armor (psrc->gctx, TRUE);
    decrypt_verify_data (psrc, cipher, plain, status, err);
    if (!GPG_IS_OK (*err))
        return NULL;
    /* return text of decrypted data */
    return seahorse_util_write_data_to_text (plain, TRUE);
}
