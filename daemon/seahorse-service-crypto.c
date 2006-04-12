/*
 * Seahorse
 *
 * Copyright (C) 2006 Nate Nielsen
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

#include <glib.h>

#include "seahorse-service.h"
#include "seahorse-key.h"
#include "seahorse-pgp-key.h"
#include "seahorse-op.h"
#include "seahorse-pgp-operation.h"
#include "seahorse-gconf.h"
#include "seahorse-util.h"
#include "seahorse-libdialogs.h"

/* flags from seahorse-service-cyrpto.xml */
#define FLAG_QUIET 0x01

G_DEFINE_TYPE (SeahorseServiceCrypto, seahorse_service_crypto, G_TYPE_OBJECT);

/* -----------------------------------------------------------------------------
 * PUBLIC METHODS
 */
 
SeahorseServiceCrypto* 
seahorse_service_crypto_new ()
{
    return g_object_new (SEAHORSE_TYPE_SERVICE_CRYPTO, NULL);
}

/* -----------------------------------------------------------------------------
 * INTERNAL HELPERS 
 */

/* Returns result in result. Frees pop and cryptdata */
static gboolean
process_crypto_result (SeahorsePGPOperation *pop, gpgme_error_t gstarterr, 
                       gpgme_data_t cryptdata, gchar **result, GError **error)
{
    size_t len;
    char *data;
    
    /* Starting the operation failed? */
    if (!GPG_IS_OK (gstarterr)) {
        gpgme_data_release (cryptdata);
        
        g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_CRYPTO,
                     "%s", gpgme_strerror (gstarterr));
        return FALSE;
    }
    
    /* Wait for it to finish (can run main loop stuff) */
    seahorse_operation_wait (SEAHORSE_OPERATION (pop));
    
    if (seahorse_operation_is_successful (SEAHORSE_OPERATION (pop))) {
        
        data = gpgme_data_release_and_get_mem (cryptdata, &len);
        *result = g_strndup (data, len);
        free (data);
        
        return TRUE;
        
    } else {
        
        /* A failed operation always has an error */
        g_assert (seahorse_operation_get_error (SEAHORSE_OPERATION (pop)));
        g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_CRYPTO, 
                     "%s", seahorse_operation_get_error (SEAHORSE_OPERATION (pop))->message);
        
        gpgme_data_release (cryptdata);
        return FALSE;
    }
}

/* -----------------------------------------------------------------------------
 * DBUS METHODS 
 */

gboolean
seahorse_service_crypto_encrypt_text (SeahorseServiceCrypto *crypto, 
                                      const char **recipients, const char *signer, 
                                      int flags, const char *cleartext, 
                                      char **crypttext, GError **error)
{
    GList *recipkeys = NULL;
    SeahorsePGPOperation *pop; 
    SeahorseKey *signkey = NULL;
    SeahorseKey *skey;
    gpgme_key_t *recips;
    gpgme_data_t plain, cipher;
    gpgme_error_t gerr;
    gboolean ret = TRUE;
    
    /* 
     * TODO: Once we support different kinds of keys that support encryption
     * then all this logic will need to change. 
     */
    
    /* The signer */
    if (signer && signer[0]) {
        signkey = seahorse_service_key_from_dbus (signer, NULL);
        if (!signkey) {
            g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
                         _("Invalid or unrecognized signer: %s"), signer);
            return FALSE;
        }
        
        if (!SEAHORSE_IS_PGP_KEY (signkey) || 
            !(seahorse_key_get_flags (signkey) & SKEY_FLAG_CAN_SIGN)) {
            g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID,
                         _("Key is not valid for signing: %s"), signer);
            return FALSE;
        }
    }
    
    /* The recipients */
    for( ; recipients[0]; recipients++)
    {
        skey = seahorse_service_key_from_dbus (recipients[0], NULL);
        if (!skey) {
            g_list_free (recipkeys);
            g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
                         _("Invalid or unrecognized recipient: %s"), recipients[0]);
            return FALSE;
        }
        
        if (!SEAHORSE_IS_PGP_KEY (skey) ||
            !(seahorse_key_get_flags (skey) & SKEY_FLAG_CAN_ENCRYPT)) {
            g_list_free (recipkeys);
            g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID,
                         _("Key is not a valid recipient for encryption: %s"), recipients[0]);
            return FALSE;
        }
        
        recipkeys = g_list_prepend (recipkeys, SEAHORSE_PGP_KEY (skey));
    }
    
    if (!recipkeys) {
        g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID,
                     _("No recipients specified"));
        return FALSE;
    }
    
    pop = seahorse_pgp_operation_new (NULL);
    
    /* new data form text */
    gerr = gpgme_data_new_from_mem (&plain, cleartext, strlen (cleartext), FALSE);
    g_return_val_if_fail (GPG_IS_OK (gerr), FALSE);
    gerr = gpgme_data_new (&cipher);
    g_return_val_if_fail (GPG_IS_OK (gerr), FALSE);
   
    /* encrypt with armor */
    gpgme_set_textmode (pop->gctx, TRUE);
    gpgme_set_armor (pop->gctx, TRUE);

    /* Add the default key if set and necessary */
    if (seahorse_gconf_get_boolean (ENCRYPTSELF_KEY)) {
        skey = seahorse_context_get_default_key (SCTX_APP ());
        if (SEAHORSE_IS_PGP_KEY (skey))
            recipkeys = g_list_append (recipkeys, skey);
    }

    /* Make keys into the right format for GPGME */
    recips = seahorse_util_keylist_to_keys (recipkeys);
    g_list_free (recipkeys);
    
    /* Do the encryption */
    if (signkey) {
        gpgme_signers_add (pop->gctx, SEAHORSE_PGP_KEY (signkey)->seckey);
        gerr = gpgme_op_encrypt_sign_start (pop->gctx, recips, GPGME_ENCRYPT_ALWAYS_TRUST, 
                                            plain, cipher);
    } else {
        gerr = gpgme_op_encrypt_start (pop->gctx, recips, GPGME_ENCRYPT_ALWAYS_TRUST, 
                                       plain, cipher);
    }
    
    seahorse_util_free_keys (recips);

    /* Frees cipher */
    ret = process_crypto_result (pop, gerr, cipher, crypttext, error);
    
    g_object_unref (pop);
    gpgme_data_release (plain);
    return ret;
}

gboolean
seahorse_service_crypto_sign_text (SeahorseServiceCrypto *crypto, const char *signer, 
                                   int flags, const char *cleartext, char **crypttext,
                                   GError **error)
{
    SeahorseKey *signkey = NULL;
    gpgme_error_t gerr;
    SeahorsePGPOperation *pop; 
    gpgme_data_t plain, cipher;
    gboolean ret = TRUE;
    
    /* 
     * TODO: Once we support different kinds of keys that support encryption
     * then all this logic will need to change. 
     */
    
    /* The signer */
    if (!signer || !signer[0]) 
        g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID,
                     _("No signer specified"));
    
    signkey = seahorse_service_key_from_dbus (signer, NULL);
    if (!signkey) {
        g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
                     _("Invalid or unrecognized signer: %s"), signer);
        return FALSE;
    }
        
    if (!SEAHORSE_IS_PGP_KEY (signkey) || 
        !(seahorse_key_get_flags (signkey) & SKEY_FLAG_CAN_SIGN)) {
        g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID,
                     _("Key is not valid for signing: %s"), signer);
        return FALSE;
    }
    
    pop = seahorse_pgp_operation_new (NULL);
    
    /* new data from text */
    gerr = gpgme_data_new_from_mem (&plain, cleartext, strlen (cleartext), FALSE);
    g_return_val_if_fail (GPG_IS_OK (gerr), FALSE);
    gerr = gpgme_data_new (&cipher);
    g_return_val_if_fail (GPG_IS_OK (gerr), FALSE);
   
    /* encrypt with armor */
    gpgme_set_textmode (pop->gctx, TRUE);
    gpgme_set_armor (pop->gctx, TRUE);

    /* Do the signage */
    gpgme_signers_add (pop->gctx, SEAHORSE_PGP_KEY (signkey)->seckey);
    gerr = gpgme_op_sign_start (pop->gctx, plain, cipher, GPGME_SIG_MODE_CLEAR);

    /* Frees cipher */
    ret = process_crypto_result (pop, gerr, cipher, crypttext, error);
    
    g_object_unref (pop);
    gpgme_data_release (plain);
    return ret;
}

gboolean
seahorse_service_crypto_decrypt_text (SeahorseServiceCrypto *crypto, 
                                      const char *ktype, int flags, 
                                      const char *crypttext, char **cleartext,
                                      char **signer, GError **error)
{
    gpgme_verify_result_t status;
    gpgme_error_t gerr;
    SeahorsePGPOperation *pop; 
    gpgme_data_t plain, cipher;
    gboolean ret = TRUE;
    
    if (!g_str_equal (ktype, g_quark_to_string (SKEY_PGP))) {
        g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID,
                     _("Invalid key type for decryption: %s"), ktype);
        return FALSE;        
    }
    
    /* 
     * TODO: Once we support different kinds of keys that support encryption
     * then all this logic will need to change. 
     */
    
    pop = seahorse_pgp_operation_new (NULL);
    
    /* new data from text */
    gerr = gpgme_data_new_from_mem (&cipher, crypttext, strlen (crypttext), FALSE);
    g_return_val_if_fail (GPG_IS_OK (gerr), FALSE);
    gerr = gpgme_data_new (&plain);
    g_return_val_if_fail (GPG_IS_OK (gerr), FALSE);
   
    /* encrypt with armor */
    gpgme_set_textmode (pop->gctx, TRUE);
    gpgme_set_armor (pop->gctx, TRUE);

    /* Do the decryption */
    gerr = gpgme_op_decrypt_verify_start (pop->gctx, cipher, plain);

    /* Frees plain */
    ret = process_crypto_result (pop, gerr, plain, cleartext, error);
    
    if (ret) {
        *signer = NULL;
        status = gpgme_op_verify_result (pop->gctx);
    
        if (status->signatures) {
            if (!(flags & FLAG_QUIET))
                seahorse_signatures_notify (NULL, status);
            if (status->signatures->summary & GPGME_SIGSUM_GREEN ||
                status->signatures->summary & GPGME_SIGSUM_VALID) 
                    *signer = seahorse_service_keyid_to_dbus (SKEY_PGP, 
                                                status->signatures->fpr, 0);
        }
    }

    g_object_unref (pop);
    gpgme_data_release (cipher);
    return ret;
}

gboolean
seahorse_service_crypto_verify_text (SeahorseServiceCrypto *crypto, 
                                     const gchar *ktype, int flags, 
                                     const gchar *crypttext, gchar **cleartext,
                                     char **signer, GError **error)
{
    gpgme_verify_result_t status;
    gpgme_error_t gerr;
    SeahorsePGPOperation *pop; 
    gpgme_data_t plain, cipher;
    gboolean ret = TRUE;
    
    if (!g_str_equal (ktype, g_quark_to_string (SKEY_PGP))) {
        g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID,
                     _("Invalid key type for verifying: %s"), ktype);
        return FALSE;        
    }

    /* 
     * TODO: Once we support different kinds of keys that support encryption
     * then all this logic will need to change. 
     */

    pop = seahorse_pgp_operation_new (NULL);
    
    /* new data from text */
    gerr = gpgme_data_new_from_mem (&cipher, crypttext, strlen (crypttext), FALSE);
    g_return_val_if_fail (GPG_IS_OK (gerr), FALSE);
    gerr = gpgme_data_new (&plain);
    g_return_val_if_fail (GPG_IS_OK (gerr), FALSE);
   
    /* encrypt with armor */
    gpgme_set_textmode (pop->gctx, TRUE);
    gpgme_set_armor (pop->gctx, TRUE);

    /* Do the decryption */
    gerr = gpgme_op_verify_start (pop->gctx, cipher, NULL, plain);

    /* Frees plain */
    ret = process_crypto_result (pop, gerr, plain, cleartext, error);
    
    if (ret) {
        *signer = NULL;
        status = gpgme_op_verify_result (pop->gctx);
    
        if (status->signatures) {
            if (!(flags & FLAG_QUIET))
                seahorse_signatures_notify (NULL, status);
            if (status->signatures->summary & GPGME_SIGSUM_GREEN ||
                status->signatures->summary & GPGME_SIGSUM_VALID) 
                    *signer = seahorse_service_keyid_to_dbus (SKEY_PGP, 
                                                status->signatures->fpr, 0);
        }
    }
    
    g_object_unref (pop);
    gpgme_data_release (cipher);
    return TRUE;
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_service_crypto_init (SeahorseServiceCrypto *crypto)
{

}

static void
seahorse_service_crypto_class_init (SeahorseServiceCryptoClass *klass)
{
    /* GObjectClass *gclass = G_OBJECT_CLASS (klass); */
    seahorse_service_crypto_parent_class = g_type_class_peek_parent (klass);
}

