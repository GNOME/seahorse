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

/**
 * A bunch of (mostly) PGP specific operations to be done on files
 * text etc...
 */
 
#ifndef __SEAHORSE_OP_H__
#define __SEAHORSE_OP_H__

#include <glib.h>
#include <gpgme.h>

#include "seahorse-pgp-source.h"
#include "seahorse-pgp-key.h"

typedef gchar*    (*SeahorseEncryptFunc)    (GList              *keys,
                                             const gchar        *data,
                                             gpgme_error_t      *err);

gint        seahorse_op_import_file         (SeahorsePGPSource  *psrc,
                                             const gchar        *path,
                                             GError             **err);

gint        seahorse_op_import_text         (SeahorsePGPSource  *psrc,
                                             const gchar        *text,
                                             GError             **err);

void        seahorse_op_encrypt_file        (GList              *keys,
                                             const gchar        *path,
                                             const gchar        *epath,
                                             gpgme_error_t      *err);

gchar*      seahorse_op_encrypt_text        (GList              *keys,
                                             const gchar        *text,
                                             gpgme_error_t      *err);

void        seahorse_op_sign_file           (SeahorsePGPKey     *signer,
                                             const gchar        *path,
                                             const gchar        *spath,
                                             gpgme_error_t      *err);

gchar*      seahorse_op_sign_text           (SeahorsePGPKey     *signer,
                                             const gchar        *text,
                                             gpgme_error_t      *err);

void        seahorse_op_encrypt_sign_file   (GList              *keys,
                                             SeahorsePGPKey     *signer,
                                             const gchar        *path,
                                             const gchar        *epath,
                                             gpgme_error_t      *err);

gchar*      seahorse_op_encrypt_sign_text   (GList              *keys,
                                             SeahorsePGPKey     *signer,
                                             const gchar        *text,
                                             gpgme_error_t      *err);

void        seahorse_op_verify_file         (SeahorsePGPSource  *psrc,
                                             const gchar        *path,
                                             const gchar        *original,
                                             gpgme_verify_result_t *status,
                                             gpgme_error_t      *err);

gchar*      seahorse_op_verify_text         (SeahorsePGPSource  *psrc,
                                             const gchar        *text,
                                             gpgme_verify_result_t *status,
                                             gpgme_error_t      *err);

void        seahorse_op_decrypt_verify_file (SeahorsePGPSource  *psrc,
                                             const gchar        *path,
                                             const gchar        *dpath,
                                             gpgme_verify_result_t *status,
                                             gpgme_error_t      *err);

gchar*      seahorse_op_decrypt_verify_text (SeahorsePGPSource  *psrc,
                                             const gchar        *text,
                                             gpgme_verify_result_t *status,
                                             gpgme_error_t      *err);

#endif /* __SEAHORSE_OP_H__ */
