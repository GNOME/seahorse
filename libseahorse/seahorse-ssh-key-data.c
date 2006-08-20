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

#include "config.h"
#include <string.h>

#include "seahorse-ssh-key-data.h"
#include "seahorse-ssh-source.h"
#include "seahorse-algo.h"

/* -----------------------------------------------------------------------------
 * HELPERS
 */

static gsize
calc_bits (guint algo, gsize len)
{
    gsize n;
    
    /*
     * To keep us from having to parse a BIGNUM and link to 
     * openssl, these are from the hip guesses at the bits 
     * of a key based on the size of the binary blob in 
     * the public key.
     */

    switch (algo) {
        
    case SSH_ALGO_RSA:
        /* Seems accurate to nearest 8 bits */
        return ((len - 21) * 8);
    
    case SSH_ALGO_DSA:
        /* DSA keys seem to only work at 'bits % 64 == 0' boundaries */
        n = ((len - 50) * 8) / 3; 
        return ((n / 64) + (((n % 64) > 32) ? 1 : 0)) * 64; /* round to 64 */
    
    default:
        return 0;
    }
}

static guint 
parse_algo (const gchar *type)
{
    if (strstr (type, "rsa") || strstr (type, "RSA"))
        return SSH_ALGO_RSA;
    if (strstr (type, "dsa") || strstr (type, "dss") || 
        strstr (type, "DSA") || strstr (type, "DSS"))
        return SSH_ALGO_DSA;
    
    return SSH_ALGO_UNK;
}

static gboolean
parse_key_blob (guchar *bytes, gsize len, SeahorseSSHKeyData *data)
{
    md5_ctx_t ctx;
    guchar digest[MD5_LEN];
    gchar *fingerprint;
    guint i;
    
    seahorse_md5_init (&ctx);
    seahorse_md5_update (&ctx, bytes, len);
    seahorse_md5_final (digest, &ctx);
    
    fingerprint = g_new0 (gchar, MD5_LEN * 3 + 1);
    for (i = 0; i < MD5_LEN; i++) {
        char hex[4];
        snprintf (hex, sizeof (hex), "%02x:", (int)(digest[i]));
        strncat (fingerprint, hex, MD5_LEN * 3 + 1);
    }

    /* Remove the trailing ':' character */
    fingerprint[(MD5_LEN * 3) - 1] = '\0';
    data->fingerprint = fingerprint;
    
    return TRUE;
}

static gboolean
parse_key_data (gchar *line, SeahorseSSHKeyData *data)
{
    gchar* space;
    guchar *bytes;
    gboolean ret;
    gsize len;
    
    /* Get the type */
    space = strchr (line, ' ');
    if (space == NULL)
        return FALSE;
    *space = '\0';
    data->algo = parse_algo (line);
    if (data->algo == SSH_ALGO_UNK)
        return FALSE;
    
    line = space + 1;
    if (!*line)
        return FALSE;
    
    /* Prepare for decoding */
    g_strchug (line);
    space = strchr (line, ' ');
    if (space)
        *space = 0;
    g_strchomp (line);
        
    /* Decode it, and parse binary stuff */
    bytes = seahorse_base64_decode (line, &len);
    ret = parse_key_blob (bytes, len, data);
    g_free (bytes);
    
    if (!ret)
        return FALSE;
    
    /* The number of bits */
    data->length = calc_bits (data->algo, len);
    
    /* And the rest is the comment */
    if (space) {
        ++space;
        
        /* If not utf8 valid, assume latin 1 */
        if (!g_utf8_validate (space, -1, NULL))
            data->comment = g_convert (space, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
        else
            data->comment = g_strdup (space);
    }
    
    /* Make a key id */
    if (data->fingerprint)
        data->keyid = g_strndup (data->fingerprint, 11);
    
    return TRUE;
}
    
static gchar**
get_file_lines (const gchar* filename, GError **err)
{
    gchar **ret;
    gchar *results;
    
    /* TODO: What about insanely large files? */
    if(!g_file_get_contents (filename, &results, NULL, err))
        return NULL;
    
    ret = g_strsplit (results, "\n", -1);
    g_free (results);
    return ret;
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

gboolean
seahorse_ssh_key_data_parse (const gchar *line, gint length, SeahorseSSHKeyData* data)
{
    gboolean ret;
    gchar *x;
    
    /* Skip leading whitespace. */
    for (; *line && g_ascii_isspace (*line); line++)
        length--;
    
    /* Comments and empty lines, not a parse error, but no data */
    if (!*line || *line == '#')
        return TRUE;
    
    x = g_strndup (line, length == -1 ? strlen (line) : length);
    ret = parse_key_data (x, data);
    g_free (x);
    
    return ret;
}

SeahorseSSHKeyData*
seahorse_ssh_key_data_read (const gchar *filename, gboolean pub)
{
    SeahorseSSHKeyData *data;
    GError *error = NULL;
    gchar **lines, **l;

    data = g_new0 (SeahorseSSHKeyData, 1);
    if (pub) {
        data->filename = NULL;
        data->filepub = g_strdup (filename);
    } else {
        data->filename = g_strdup (filename);
        data->filepub = g_strdup_printf ("%s.pub", filename);
    }
    
    lines = get_file_lines (data->filepub, &error);
    if (!lines) {
        g_warning ("couldn't read public SSH file: %s (%s)", data->filepub, error->message);
        g_error_free(error);
        return data;
    }
    
    for (l = lines; *l; l++) {
        if (seahorse_ssh_key_data_parse (*l, -1, data) && 
            seahorse_ssh_key_data_is_valid (data))
            break;
    }
    
    g_strfreev (lines);
    return data;
    
    return data;
}

gboolean
seahorse_ssh_key_data_is_valid (SeahorseSSHKeyData *data)
{
    g_return_val_if_fail (data != NULL, FALSE);
    return data->fingerprint != NULL;
}

void 
seahorse_ssh_key_data_free (SeahorseSSHKeyData *data)
{
    if (!data)
        return;
    
    g_free (data->filename);
    g_free (data->filepub);
    g_free (data->comment);
    g_free (data->keyid);
    g_free (data->fingerprint);
    g_free (data);
}
