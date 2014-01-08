/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

/**
 * SeahorseSSHKeyData: The data in an SSH key
 */
 
#ifndef __SEAHORSE_SSH_KEY_DATA_H__
#define __SEAHORSE_SSH_KEY_DATA_H__

#include <gtk/gtk.h>

/* The various algorithm types */
enum {
    SSH_ALGO_UNK,
    SSH_ALGO_RSA,
    SSH_ALGO_DSA
};

/* 
 * We prefix SSH keys that come from seahorse with a unique 
 * prefix. Otherwise SSH private keys look like any other 
 * PEM encoded 'x509' keys. 
 */
#define SSH_KEY_SECRET_SIG "# SSH PRIVATE KEY: "


struct _SeahorseSSHKeyData {
    /* Used by callers */
    gchar *pubfile;         /* The public key file */
    gboolean partial;       /* Only part of the public key file */
    gchar *privfile;        /* The secret key file */
    
    /* Filled in by parser */
    gchar *rawdata;         /* The raw data of the public key */
    gchar *comment;         /* The comment for the public key */
    gchar *fingerprint;     /* The full fingerprint hash */
    guint length;           /* Number of bits */
    guint algo;             /* Key algorithm */
    gboolean authorized;    /* Is in authorized_keys */
};

struct _SeahorseSSHSecData {
    gchar *rawdata;
    gchar *comment;
    guint algo;
};

typedef struct _SeahorseSSHKeyData SeahorseSSHKeyData;
typedef struct _SeahorseSSHSecData SeahorseSSHSecData;
    
typedef gboolean (*SeahorseSSHPublicKeyParsed)(SeahorseSSHKeyData *data, gpointer arg);
typedef gboolean (*SeahorseSSHSecretKeyParsed)(SeahorseSSHSecData *data, gpointer arg);

guint                   seahorse_ssh_key_data_parse           (const gchar *data, 
                                                               SeahorseSSHPublicKeyParsed public_cb,
                                                               SeahorseSSHSecretKeyParsed secret_cb,
                                                               gpointer arg);

guint                   seahorse_ssh_key_data_parse_file      (const gchar *filename, 
                                                               SeahorseSSHPublicKeyParsed public_cb,
                                                               SeahorseSSHSecretKeyParsed secret_cb,
                                                               gpointer arg,
                                                               GError **error);

SeahorseSSHKeyData*     seahorse_ssh_key_data_parse_line      (const gchar *line,
                                                               gssize length);

gboolean                seahorse_ssh_key_data_match           (const gchar *line,
                                                               gint length,
                                                               SeahorseSSHKeyData *match);

gboolean                seahorse_ssh_key_data_filter_file     (const gchar *filename,
                                                               SeahorseSSHKeyData *add,
                                                               SeahorseSSHKeyData *remove,
                                                               GError **error);

gboolean                seahorse_ssh_key_data_is_valid        (SeahorseSSHKeyData *data);

SeahorseSSHKeyData*     seahorse_ssh_key_data_dup             (SeahorseSSHKeyData *data);

void                    seahorse_ssh_key_data_free            (SeahorseSSHKeyData *data);

void                    seahorse_ssh_sec_data_free            (SeahorseSSHSecData *data);


#endif /* __SEAHORSE_KEY_DATA_H__ */
