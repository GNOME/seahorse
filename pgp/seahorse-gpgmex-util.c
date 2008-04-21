/*
 * Seahorse
 *
 * Copyright (C) 2004 Stefan Walter
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

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "config.h"
#include <glib.h>

#include "pgp/seahorse-gpgmex.h"

/* -----------------------------------------------------------------------------
 * DATA
 */

gpgme_data_t 
gpgmex_data_new ()
{
    gpgme_error_t gerr;
    gpgme_data_t data;
    
    gerr = gpgme_data_new (&data);
    if (!GPG_IS_OK (gerr)) {
        if (gpgme_err_code_to_errno (gerr) == ENOMEM || 
            gpgme_err_code (gerr) == GPG_ERR_ENOMEM) {
                
            g_error ("%s: failed to allocate gpgme_data_t", G_STRLOC);
                
        } else {
            /* The only reason this should fail is above */
            g_assert_not_reached ();
            
            /* Just in case */
            abort ();
        }
    }
    
    return data;
}

gpgme_data_t
gpgmex_data_new_from_mem (const char *buffer, size_t size, gboolean copy)
{
    gpgme_data_t data;
    gpgme_error_t gerr;
    
    gerr = gpgme_data_new_from_mem (&data, buffer, size, copy ? 1 : 0);
    if (!GPG_IS_OK (gerr)) {
        if (gpgme_err_code_to_errno (gerr) == ENOMEM || 
            gpgme_err_code (gerr) == GPG_ERR_ENOMEM) {
                
            g_error ("%s: failed to allocate gpgme_data_t", G_STRLOC);
                
        } else {
            /* The only reason this should fail is above */
            g_assert_not_reached ();
            
            /* Just in case */
            abort ();
        }
    }
    
    return data;
}

int 
gpgmex_data_write_all (gpgme_data_t data, const void* buffer, size_t len)
{
	guchar *text = (guchar*)buffer;
	gint written;
    
	if (len < 0)
		len = strlen ((gchar*)text);
    
	while (len > 0) {
		written = gpgme_data_write (data, (void*)text, len);
		if (written < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			return -1;
		}
        
		len -= written;
		text += written;
	}
    
	return written;
}

void
gpgmex_data_release (gpgme_data_t data)
{
    if (data)
        gpgme_data_release (data);
}

/* -----------------------------------------------------------------------------
 * KEYS
 */
 
/* Our special keylist mode flag */
#define SEAHORSE_KEYLIST_MODE    0x04000000

/* Because we really try to be good and not use internal
 * fields in the gpgme_key_t structure: */
typedef struct _sukey {
    struct _gpgme_key key;
    int refs;
} sukey;

gpgme_key_t 
gpgmex_key_alloc ()
{
    /* Allocate a gpgme_key_t structure without subkeys,
     * signatures or uids, with all defaults */
  
    sukey* key;
    
    key = g_new0 (sukey, 1);
    key->key.protocol = GPGME_PROTOCOL_OpenPGP;
    key->key.keylist_mode = SEAHORSE_KEYLIST_MODE | GPGME_KEYLIST_MODE_EXTERN;
    key->refs = 1;
    return (gpgme_key_t)key;
}

static void
add_subkey_to_key (gpgme_key_t key, gpgme_subkey_t subkey)
{
    gpgme_subkey_t sk;
    
    if (!key->subkeys) {
        /* Copy certain values into the key */
        key->revoked = subkey->revoked;
        key->expired = subkey->expired;
        key->disabled = subkey->disabled;
        key->subkeys = subkey;
        
    } else {
        sk = key->subkeys;
        while (sk->next != NULL)
            sk = sk->next;
        sk->next = subkey;   
    }
}

void        
gpgmex_key_add_subkey (gpgme_key_t key, const char *fpr, guint flags, 
                              long int timestamp, long int expires, 
                              unsigned int length, gpgme_pubkey_algo_t algo)
{
    gpgme_subkey_t subkey;
    guint n;

    g_return_if_fail (key != NULL);
    g_return_if_fail (key->keylist_mode & SEAHORSE_KEYLIST_MODE);
    
    subkey = g_new0 (struct _gpgme_subkey, 1);
    
    subkey->fpr = g_strdup (fpr);
    subkey->revoked = flags & GPGMEX_KEY_REVOKED;
    subkey->disabled = flags & GPGMEX_KEY_DISABLED;
    subkey->expired = expires > 0 && expires <= ((long int)time (NULL));
    subkey->pubkey_algo = algo;
    subkey->length = length;
    subkey->timestamp = timestamp;
    subkey->expires = expires;
    
    /* Figure out the key id */
    n = strlen (fpr);
    if (n < 8) 
        fpr = "INVALID INVALID ";
    if (n >= 16) {
        fpr += n - 16;
        subkey->keyid = g_strdup (fpr);
    } else if (n < 16) {
        subkey->keyid = g_new0 (char, 17);
        memset (subkey->keyid, ' ', 16);
        strcpy (subkey->keyid + (16 - n), fpr);
    }
    
    add_subkey_to_key (key, subkey);
}

void 
gpgmex_key_copy_subkey (gpgme_key_t key, gpgme_subkey_t subkey)
{
    gpgme_subkey_t sk;

    g_return_if_fail (key != NULL);
    g_return_if_fail (key->keylist_mode & SEAHORSE_KEYLIST_MODE);

    sk = g_new0 (struct _gpgme_subkey, 1);
    
    sk->fpr = g_strdup (subkey->fpr);
    sk->revoked = subkey->revoked;
    sk->disabled = subkey->disabled;
    sk->expired = subkey->expired;
    sk->pubkey_algo = subkey->pubkey_algo;    
    sk->length = subkey->length;
    sk->timestamp = subkey->timestamp;
    sk->expires = subkey->expires;
    sk->keyid = g_strdup (subkey->keyid);
    
    add_subkey_to_key (key, sk);
}

/* Copied from GPGME */
static void
parse_user_id (const gchar *uid, gchar **name, gchar **email, gchar **comment)
{
    gchar *src, *tail, *x;
    int in_name = 0;
    int in_email = 0;
    int in_comment = 0;

    x = tail = src = g_strdup (uid);
    
    while (*src) {
        if (in_email) {
	        if (*src == '<')
	            /* Not legal but anyway.  */
	            in_email++;
	        else if (*src == '>') {
	            if (!--in_email && !*email) {
		            *email = tail;
                    *src = 0;
                    tail = src + 1;
		        }
	        }
	    } else if (in_comment) {
	        if (*src == '(')
	            in_comment++;
	        else if (*src == ')') {
	            if (!--in_comment && !*comment) {
		            *comment = tail;
                    *src = 0;
                    tail = src + 1;
		        }
	        }
	    } else if (*src == '<') {
	        if (in_name) {
	            if (!*name) {
		            *name = tail;
                    *src = 0;
                    tail = src + 1;
		        }
	            in_name = 0;
	        }
	        in_email = 1;
	    } else if (*src == '(') {
	        if (in_name) {
	            if (!*name) {
		            *name = tail;
                    *src = 0;
                    tail = src + 1;
		        }
	            in_name = 0;
	        }
	        in_comment = 1;
	    } else if (!in_name && *src != ' ' && *src != '\t') {
	        in_name = 1;
	    }    
        src++;
    }
 
    if (in_name) {
        if (!*name) {
	        *name = tail;
            *src = 0;
            tail = src + 1;
	    }
    }
 
    /* Let unused parts point to an EOS.  */
    *name = g_strdup (*name ? *name : "");
    *email = g_strdup (*email ? *email : "");
    *comment = g_strdup (*comment ? *comment : "");
    
    g_free (x);
}

static void
add_uid_to_key (gpgme_key_t key, gpgme_user_id_t userid)
{
    gpgme_user_id_t u;
    
    if (!key->uids)
        key->uids = userid;
    else {
        u = key->uids;
        while (u->next != NULL)
            u = u->next;
        u->next = userid;
    }
}    

void
gpgmex_key_add_uid (gpgme_key_t key, const gchar *uid, 
                           guint flags)
{
    gpgme_user_id_t userid;
    
    g_return_if_fail (key != NULL);
    g_return_if_fail (key->keylist_mode & SEAHORSE_KEYLIST_MODE);

    userid = g_new0 (struct _gpgme_user_id, 1);
    userid->uid = g_strdup (uid);
    userid->revoked = flags & GPGMEX_KEY_REVOKED;
    
    /* Parse out the parts of the uid */
    parse_user_id (uid, &(userid->name), &(userid->email), &(userid->comment));
   
    add_uid_to_key (key, userid);
}

void
gpgmex_key_copy_uid (gpgme_key_t key, gpgme_user_id_t userid)
{
    gpgme_user_id_t u;
    
    g_return_if_fail (key != NULL);
    g_return_if_fail (key->keylist_mode & SEAHORSE_KEYLIST_MODE);

    u = g_new0 (struct _gpgme_user_id, 1);
    u->uid = g_strdup (userid->uid);
    u->revoked = userid->revoked;
    u->name = g_strdup (userid->name);
    u->email = g_strdup (userid->email);
    u->comment = g_strdup (userid->comment);
    
    add_uid_to_key (key, u);
}

void        
gpgmex_key_ref (gpgme_key_t key)
{
    g_return_if_fail (key != NULL);
    
    if (key->keylist_mode & SEAHORSE_KEYLIST_MODE) 
        ((sukey*)key)->refs++;
    else
        gpgme_key_ref (key);
}
    
void        
gpgmex_key_unref (gpgme_key_t key)
{
    g_return_if_fail (key != NULL);
    
    if (key->keylist_mode & SEAHORSE_KEYLIST_MODE) {
       
        if ((--(((sukey*)key)->refs)) <= 0) {
            gpgme_user_id_t nu;
            gpgme_user_id_t u;
            gpgme_subkey_t nsk;
            gpgme_subkey_t sk;
    
            u = key->uids;
            while (u) {
                nu = u->next;
                g_free (u->uid);
                g_free (u->name);
                g_free (u->email);
                g_free (u->comment);
                g_free (u);
                u = nu;
            }
            
            sk = key->subkeys;
            while (sk) {
                nsk = sk->next;
                g_free (sk->fpr);
                g_free (sk->keyid);
                g_free (sk);
                sk = nsk;
            }
    
            g_free (key);        
        }
                
    } else {
        gpgme_key_unref (key);
    }
}

gboolean
gpgmex_key_is_gpgme (gpgme_key_t key)
{
    g_return_val_if_fail (key != NULL, FALSE);
    return !(key->keylist_mode & SEAHORSE_KEYLIST_MODE);
}

void 
gpgmex_combine_keys (gpgme_key_t k, gpgme_key_t key)
{
	gpgme_user_id_t uid;
	gpgme_user_id_t u;
	gpgme_subkey_t subkey;
	gpgme_subkey_t s;
	gboolean found;

	g_assert (k != NULL);
	g_assert (key != NULL);

	/* Go through user ids */
	for (uid = key->uids; uid != NULL; uid = uid->next) {
		g_assert (uid->uid);
		found = FALSE;

		for (u = k->uids; u != NULL; u = u->next) {
			g_assert (u->uid);

			if (strcmp (u->uid, uid->uid) == 0) {
				found = TRUE;
				break;
			}
		}

		if (!found)
			gpgmex_key_copy_uid (k, uid);
	}

	/* Go through subkeys */
	for (subkey = key->subkeys; subkey != NULL; subkey = subkey->next) {
		g_assert (subkey->fpr);
		found = FALSE;

		for (s = k->subkeys; s != NULL; s = s->next) {
			g_assert (s->fpr);

			if (strcmp (s->fpr, subkey->fpr) == 0) {
				found = TRUE;
				break;
			}
		}

		if (!found)
			gpgmex_key_copy_subkey (k, subkey);
	}
}

gpgmex_photo_id_t 
gpgmex_photo_id_alloc (guint uid)
{
    gpgmex_photo_id_t photoid = g_new0 (struct _gpgmex_photo_id, 1);
    photoid->uid = uid;
    return photoid;
}

void        
gpgmex_photo_id_free (gpgmex_photo_id_t photoid)
{
    if (photoid) {
        if (photoid->photo)
            g_object_unref (photoid->photo);
        g_free (photoid);
    }
}

void 
gpgmex_photo_id_free_all (gpgmex_photo_id_t photoid)
{
    while (photoid) {
        gpgmex_photo_id_t next = photoid->next;
        gpgmex_photo_id_free (photoid);
        photoid = next;
    }
}

/* -----------------------------------------------------------------------------
 * OTHER MISC FUNCTIONS
 */
 
#ifndef HAVE_STRSEP
/* code taken from glibc-2.2.1/sysdeps/generic/strsep.c */
char *
strsep (char **stringp, const char *delim)
{
    char *begin, *end;

    begin = *stringp;
    if (begin == NULL)
        return NULL;

      /* A frequent case is when the delimiter string contains only one
         character.  Here we don't need to call the expensive `strpbrk'
         function and instead work using `strchr'.  */
      if (delim[0] == '\0' || delim[1] == '\0') {
        char ch = delim[0];

        if (ch == '\0')
            end = NULL; 
        else {
            if (*begin == ch)
                end = begin;
            else if (*begin == '\0')
                end = NULL;
            else
                end = strchr (begin + 1, ch);
        }
    } else
        /* Find the end of the token.  */
        end = strpbrk (begin, delim);

    if (end) {
      /* Terminate the token and set *STRINGP past NUL character.  */
      *end++ = '\0';
      *stringp = end;
    } else
        /* No more delimiters; this is the last token.  */
        *stringp = NULL;

    return begin;
}
#endif /*HAVE_STRSEP*/

SeahorseValidity    
gpgmex_validity_to_seahorse (gpgme_validity_t validity)
{
    switch (validity) {
    case GPGME_VALIDITY_NEVER:
        return SEAHORSE_VALIDITY_NEVER;
    case GPGME_VALIDITY_MARGINAL:
        return SEAHORSE_VALIDITY_MARGINAL;
    case GPGME_VALIDITY_FULL:
        return SEAHORSE_VALIDITY_FULL;
    case GPGME_VALIDITY_ULTIMATE:
        return SEAHORSE_VALIDITY_ULTIMATE;
    case GPGME_VALIDITY_UNDEFINED:
    case GPGME_VALIDITY_UNKNOWN:
    default:
        return SEAHORSE_VALIDITY_UNKNOWN;
    }
}
