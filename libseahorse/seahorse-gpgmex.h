/*
 * Seahorse
 *
 * Copyright (C) 2004 Nate Nielsen
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

#ifndef __SEAHORSE_GPGMEX_H__
#define __SEAHORSE_GPGMEX_H__

#include <gpgme.h>

/* -----------------------------------------------------------------------------
 * ERROR HANDLING 
 */

#define GPG_IS_OK(e)        (gpgme_err_code (e) == GPG_ERR_NO_ERROR)
#define GPG_OK              (gpgme_error (GPG_ERR_NO_ERROR))
#define GPG_E(e)            (gpgme_error (e))

/* -----------------------------------------------------------------------------
 * KEY ALLOCATION
 */
 
/*
 * Functions for allocating keys we find from sources other than 
 * GPGME, like servers for example. 
 * 
 * Yes, we allocate gpgme_key_t structures and use them. The structure 
 * is open, we play by the rules and don't use fields we're asked not 
 * to. Of course we should never pass these structures to GPGME functions
 * like gpgme_key_unref etc....
 */

enum {
    GPGMEX_KEY_REVOKED  = 0x01,
    GPGMEX_KEY_DISABLED = 0x02
};

gpgme_key_t gpgmex_key_alloc         ();

void        gpgmex_key_add_subkey    (gpgme_key_t key,
                                      const char *fpr,
                                      unsigned int flags,
                                      long int timestamp,
                                      long int expires,
                                      unsigned int length,
                                      gpgme_pubkey_algo_t algo);

void        gpgmex_key_add_uid       (gpgme_key_t key,
                                      const char *uid,
                                      unsigned int flags);

void        gpgmex_key_copy_subkey   (gpgme_key_t key,
                                      gpgme_subkey_t subkey);

void        gpgmex_key_copy_uid      (gpgme_key_t key,
                                      gpgme_user_id_t uid);                                      
                                      
int         gpgmex_key_is_gpgme      (gpgme_key_t key);

void        gpgmex_key_ref           (gpgme_key_t key);

void        gpgmex_key_unref         (gpgme_key_t key);

#ifdef WITH_KEYSERVER

/* -----------------------------------------------------------------------------
 * KEYSERVER ACCESS
 */
 
typedef void*  gpgmex_keyserver_op_t;

/* This callback is expected to use or unref key. */
typedef void    (*gpgmex_keyserver_list_cb) (gpgme_ctx_t ctx,
                                             gpgmex_keyserver_op_t op,
                                             gpgme_key_t key,
                                             unsigned int total,
                                             void *userdata);

typedef void    (*gpgmex_keyserver_done_cb) (gpgme_ctx_t ctx,
                                             gpgmex_keyserver_op_t op,
                                             gpgme_error_t status,
                                             const char *message,
                                             void *userdata);
                                             
enum {
    GPGMEX_KEYLIST_REVOKED = 0x01,
    GPGMEX_KEYLIST_SUBKEYS = 0x02
};

/* Lists keys from a key server. Note that this actually lists UIDs.
 * Calls lcb for each key found, and dcb when done */
void         gpgmex_keyserver_start_list      (gpgme_ctx_t ctx, 
                                               const char *server, 
                                               const char *pattern, 
                                               unsigned int flags, 
                                               gpgmex_keyserver_list_cb lcb,
                                               gpgmex_keyserver_done_cb dcb,
                                               void *userdata,
                                               gpgmex_keyserver_op_t *op);

/* Retrieves the raw data for a key by fingerprint from a key server */
void         gpgmex_keyserver_start_retrieve  (gpgme_ctx_t ctx, 
                                               const char *server, 
                                               const char *fpr,
                                               gpgme_data_t data,
                                               gpgmex_keyserver_done_cb dcb,
                                               void *userdata,
                                               gpgmex_keyserver_op_t *op);

/* Uploads a key to the keyserver */
void         gpgmex_keyserver_start_send      (gpgme_ctx_t ctx, 
                                               const char *server, 
                                               const char *fpr,
                                               gpgme_data_t data,
                                               gpgmex_keyserver_done_cb dcb,
                                               void *userdata,
                                               gpgmex_keyserver_op_t *op);

void         gpgmex_keyserver_cancel          (gpgmex_keyserver_op_t op);

#endif /* WITH_KEYSERVER */

#endif /* __SEAHORSE_GPGMEX_H__ */
