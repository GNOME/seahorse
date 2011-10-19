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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <string.h>

#include "seahorse-ssh-key-data.h"
#include "seahorse-ssh-source.h"
#include "seahorse-util.h"

#define SSH_PRIVATE_BEGIN "-----BEGIN "
#define SSH_PRIVATE_END   "-----END "

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
	GString *fingerprint;
	gchar *digest;
	gsize n_digest;
	gsize i;

	digest = g_compute_checksum_for_data (G_CHECKSUM_MD5, bytes, len);
	if (!digest)
		return FALSE;

	n_digest = strlen (digest);
	fingerprint = g_string_sized_new ((n_digest * 3) / 2);
	for (i = 0; i < n_digest; i += 2) {
		if (i > 0)
			g_string_append_c (fingerprint, ':');
		g_string_append_len (fingerprint, digest + i, 2);
	}

	g_free (digest);
	data->fingerprint = g_string_free (fingerprint, FALSE);

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
    *space = ' ';
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
    bytes = g_base64_decode (line, &len);
    ret = parse_key_blob (bytes, len, data);
    g_free (bytes);
    
    if (!ret)
        return FALSE;
    
    /* The number of bits */
    data->length = calc_bits (data->algo, len);
    
    /* And the rest is the comment */
    if (space) {
        *space = ' ';
        ++space;
        
        /* If not utf8 valid, assume latin 1 */
        if (!g_utf8_validate (space, -1, NULL))
            data->comment = g_convert (space, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
        else
            data->comment = g_strdup (space);
    }
    
    return TRUE;
}

static gchar *
parse_lines_block (gchar ***lx, const gchar *start, const gchar* end)
{
    GString *result;
    gchar **lines;
    
    result = g_string_new ("");
    lines = *lx;
     
    /* Look for the beginning */
    for ( ; *lines; lines++) {
        if (strstr (*lines, start)) {
            g_string_append (result, *lines);
            g_string_append_c (result, '\n');
            lines++;
            break;
        }
    }
    
    /* Look for the end */
    for ( ; *lines; lines++) {
        g_string_append (result, *lines);
        g_string_append_c (result, '\n');
        if (strstr (*lines, end)) 
            break;
    }
    
    *lx = lines;
    return g_string_free (result, result->len == 0);
}

static SeahorseSSHSecData*
parse_private_data (gchar ***lx)
{
    SeahorseSSHSecData *secdata = NULL;
    gchar *rawdata;
    gchar *comment;
    
    comment = strstr (**lx, SSH_KEY_SECRET_SIG);
    if (comment) 
        comment += strlen (SSH_KEY_SECRET_SIG);
    
    rawdata = parse_lines_block (lx, SSH_PRIVATE_BEGIN, SSH_PRIVATE_END);
    if (rawdata) {
        secdata = g_new0 (SeahorseSSHSecData, 1);
        if (comment)
        secdata->comment = g_strdup (g_strstrip (comment));
        secdata->rawdata = rawdata;
        
        /* Guess at the algorithm type */
        if (strstr (secdata->rawdata, " RSA "))
            secdata->algo = SSH_ALGO_RSA;
        else if (strstr (secdata->rawdata, " DSA "))
            secdata->algo = SSH_ALGO_DSA;
        else
            secdata->algo = SSH_ALGO_UNK;
    } 
    
    /* caller knows how many lines were consumed */
    return secdata;
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

guint
seahorse_ssh_key_data_parse (const gchar *data, SeahorseSSHPublicKeyParsed public_cb,
                             SeahorseSSHSecretKeyParsed secret_cb, gpointer arg)
{
    SeahorseSSHKeyData *keydata;
    SeahorseSSHSecData *secdata;
    guint nkeys = 0;
    gchar **lines, **l;
    gchar *line;
    
    g_return_val_if_fail (data, 0);
    
    if (!*data)
	    return 0;
    
    lines = g_strsplit (data, "\n", 0);
    for (l = lines; *l; l++) {
        
        line = *l;

        /* Skip leading whitespace. */
        for (; *line && g_ascii_isspace (*line); line++)
            ;

        /* See if we have a private key coming up */
        if (strstr (line, SSH_KEY_SECRET_SIG) || 
            strstr (line, SSH_PRIVATE_BEGIN)) {
            
            secdata = parse_private_data (&l);
            if (secdata) {
            
                /* Let it fall on the floor :( */
                if (!secret_cb) {
                    seahorse_ssh_sec_data_free (secdata);
                    continue;
                }
                
                if (!(secret_cb) (secdata, arg))
                    break;
            }

            if (!*l)
                break;
            line = *l;
        }
        
        /* Comments and empty lines, not a parse error, but no data */
        if (!*line || *line == '#')
            continue;
        
        /* See if we have a public key */
        keydata = seahorse_ssh_key_data_parse_line (line, -1);
        if (keydata) {
            nkeys++;
            
            /* Let it fall on the floor :( */
            if (!public_cb) {
                seahorse_ssh_key_data_free (keydata);
                continue;
            }
            
            if (!(public_cb) (keydata, arg))
                break;
        }
    }
    
    g_strfreev (lines);
    return nkeys;
}

guint
seahorse_ssh_key_data_parse_file (const gchar *filename,  SeahorseSSHPublicKeyParsed public_cb,
                                  SeahorseSSHSecretKeyParsed secret_cb, gpointer arg,
                                  GError **err)
{
    gchar *contents;
    guint nkeys;
    
    if (!g_file_get_contents (filename, &contents, NULL, err))
        return 0;
    
    nkeys = seahorse_ssh_key_data_parse (contents, public_cb, secret_cb, arg);
    g_free (contents);
    return nkeys;
}

SeahorseSSHKeyData*
seahorse_ssh_key_data_parse_line (const gchar *line, guint length)
{
    SeahorseSSHKeyData *keydata = NULL;
    gchar *x;
    
    if (length == -1)
        length = strlen (line);
    
    /* Skip leading whitespace. */
    for (; *line && g_ascii_isspace (*line); line++)
        length--;
    
    /* Comments and empty lines, not a parse error, but no data */
    if (!*line || *line == '#')
        return NULL;
    
    x = g_strndup (line, length == -1 ? strlen (line) : length);
    
    keydata = g_new0 (SeahorseSSHKeyData, 1);
    if (!parse_key_data (x, keydata)) {
        g_free (keydata);
        keydata = NULL;
    }
    
    if (keydata)
        keydata->rawdata = x;
    else 
        g_free (x);
    
    return keydata;
}

gboolean
seahorse_ssh_key_data_match (const gchar *line, gint length, SeahorseSSHKeyData *match)
{
    SeahorseSSHKeyData *keydata;
    gboolean ret = FALSE;
    
    g_return_val_if_fail (match->fingerprint, FALSE);
    
    keydata = seahorse_ssh_key_data_parse_line (line, -1);
    if (keydata && keydata->fingerprint && 
        strcmp (match->fingerprint, keydata->fingerprint) == 0)
        ret = TRUE;
    
    seahorse_ssh_key_data_free (keydata);
    return ret;
}

gboolean
seahorse_ssh_key_data_filter_file (const gchar *filename, SeahorseSSHKeyData *add, 
                                   SeahorseSSHKeyData *remove, GError **err)
{
    GString *results;
    gchar *contents = NULL;
    gchar **lines, **l;
    gboolean ret;
    gboolean first = TRUE;
    
    /* By default filter out teh one we're adding */
    if (!remove)
        remove = add;
    
    if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
        if (!g_file_get_contents (filename, &contents, NULL, err))
            return FALSE;
    }

    lines = g_strsplit (contents ? contents : "", "\n", -1);
    g_free (contents);
    
    results = g_string_new ("");
    
    /* Load each line */
    for (l = lines; *l; l++) {
        if (seahorse_ssh_key_data_match (*l, -1, remove))
            continue;
        if (!first)
            g_string_append_c (results, '\n');
        first = FALSE;
        g_string_append (results, *l);
    }
    
    /* Add any that need adding */
    if (add) {
        if(!first)
            g_string_append_c (results, '\n');
        g_string_append (results, add->rawdata);
    }
    
    g_strfreev (lines);
    
    ret = seahorse_util_write_file_private (filename, results->str, err);
    g_string_free (results, TRUE);
    
    return ret;
}

gboolean
seahorse_ssh_key_data_is_valid (SeahorseSSHKeyData *data)
{
    g_return_val_if_fail (data != NULL, FALSE);
    return data->fingerprint != NULL;
}

SeahorseSSHKeyData*
seahorse_ssh_key_data_dup (SeahorseSSHKeyData *data)
{
    SeahorseSSHKeyData *n = g_new0 (SeahorseSSHKeyData, 1);
    n->privfile = g_strdup (data->privfile);
    n->pubfile = g_strdup (data->pubfile);
    n->rawdata = g_strdup (data->rawdata);
    n->comment = g_strdup (data->comment);
    n->fingerprint = g_strdup (data->fingerprint);
    n->authorized = data->authorized;
    n->partial = data->partial;
    n->algo = data->algo;
    n->length = data->length;
    return n;
}

void
seahorse_ssh_key_data_free (SeahorseSSHKeyData *data)
{
    if (!data)
        return;
    
    g_free (data->privfile);
    g_free (data->pubfile);
    g_free (data->rawdata);
    g_free (data->comment);
    g_free (data->fingerprint);
    g_free (data);
}

void
seahorse_ssh_sec_data_free (SeahorseSSHSecData *data)
{
    if (!data)
        return;
    
    g_free (data->rawdata);
    g_free (data->comment);
}

