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

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "seahorse-gpgmex.h"
#include <glib.h>

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
    key->key.keylist_mode = SEAHORSE_KEYLIST_MODE;
    key->refs = 1;
    return (gpgme_key_t)key;
}

void        
gpgmex_key_add_subkey (gpgme_key_t key, const char *fpr, guint flags, 
                              long int timestamp, long int expires, 
                              unsigned int length, gpgme_pubkey_algo_t algo)
{
    gpgme_subkey_t subkey;
    gpgme_subkey_t sk;

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
    
    /* This isn't actually a valid keyid, but we don't use 
     * this field in any case */
    subkey->keyid = g_strdup("");
      
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
gpgmex_key_add_uid (gpgme_key_t key, const gchar *uid, 
                           guint flags)
{
    gpgme_user_id_t userid;
    gpgme_user_id_t u;
    
    g_return_if_fail (key != NULL);
    g_return_if_fail (key->keylist_mode & SEAHORSE_KEYLIST_MODE);

    userid = g_new0 (struct _gpgme_user_id, 1);
    userid->uid = g_strdup (uid);
    userid->revoked = flags & GPGMEX_KEY_REVOKED;
    
    /* These aren't valid, but we don't use these fields */
    userid->name = g_strdup ("");
    userid->email = g_strdup ("");
    userid->comment = g_strdup ("");
    
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
       
        if (--(((sukey*)key)->refs) <= 0) {
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

