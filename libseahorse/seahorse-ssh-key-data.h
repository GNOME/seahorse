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

struct _SeahorseSSHKeyData {
    gchar *filename;
    gchar *filepub;
    gchar *comment;
    gchar *keyid;
    gchar *fingerprint;
    guint length;
    guint algo;
};

typedef struct _SeahorseSSHKeyData SeahorseSSHKeyData;

SeahorseSSHKeyData*     seahorse_ssh_key_data_read            (const gchar *filename,
                                                               gboolean pub);

gboolean                seahorse_ssh_key_data_parse           (const gchar *line, 
                                                               gint length,
                                                               SeahorseSSHKeyData *data);

gboolean                seahorse_ssh_key_data_is_valid        (SeahorseSSHKeyData *data);

void                    seahorse_ssh_key_data_free            (SeahorseSSHKeyData *data);

#endif /* __SEAHORSE_KEY_DATA_H__ */
