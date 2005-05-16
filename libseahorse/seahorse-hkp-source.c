/*
 * Seahorse
 *
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
 
#include <stdlib.h>
#include <libintl.h>
#include <gnome.h>
#include <libsoup/soup.h>

#include "config.h"
#include "seahorse-gpgmex.h"
#include "seahorse-operation.h"
#include "seahorse-hkp-source.h"
#include "seahorse-multi-source.h"
#include "seahorse-util.h"
#include "seahorse-key.h"
#include "seahorse-key-pair.h"

#ifdef WITH_HKP

/* Amount of keys to load in a batch */
#define DEFAULT_LOAD_BATCH 30

#define SOUP_MESSAGE_IS_ERROR(msg) \
    (SOUP_STATUS_IS_TRANSPORT_ERROR((msg)->status_code) || \
     SOUP_STATUS_IS_CLIENT_ERROR((msg)->status_code) || \
     SOUP_STATUS_IS_SERVER_ERROR((msg)->status_code))
    
#define HKP_ERROR_DOMAIN (get_hkp_error_domain())

static GQuark
get_hkp_error_domain ()
{
    static GQuark q = 0;
    if(q == 0)
        q = g_quark_from_static_string ("seahorse-hkp-error");
    return q;
}

#ifdef _DEBUG

static void
dump_soup_header (const gchar *name, const gchar *value, gpointer user_data)
{
    g_printerr ("    %s: %s\n", name, value);
}

static void
dump_soup_request (SoupMessage *msg)
{
    const SoupUri *uri = soup_message_get_uri (msg);
    gchar *t;
    
    t = soup_uri_to_string (uri, FALSE);
    g_printerr ("method: %s uri: %s\n", msg->method, t);
    g_free (t);
    
    soup_message_foreach_header (msg->request_headers, 
                                 (GHFunc)dump_soup_header, NULL);
    
    if (msg->request.body) {
        t = g_strndup (msg->request.body, msg->request.length);
        g_printerr ("request: %s\n", t);
        g_free (t);
    }    
}

static void
dump_soup_response (SoupMessage *msg)
{
    gchar *t;
    
    g_printerr ("status: %d reason: %s\n", msg->status_code, msg->reason_phrase);
    soup_message_foreach_header (msg->response_headers, 
                                 (GHFunc)dump_soup_header, NULL);
    
    if (msg->response.body) {
        t = g_strndup (msg->response.body, msg->response.length);
        g_printerr ("response: %s\n", t);
        g_free (t);
    }    
}

#endif

/* -----------------------------------------------------------------------------
 *  HKP OPERATION     
 */
 
#define SEAHORSE_TYPE_HKP_OPERATION            (seahorse_hkp_operation_get_type ())
#define SEAHORSE_HKP_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_HKP_OPERATION, SeahorseHKPOperation))
#define SEAHORSE_HKP_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_HKP_OPERATION, SeahorseHKPOperationClass))
#define SEAHORSE_IS_HKP_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_HKP_OPERATION))
#define SEAHORSE_IS_HKP_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_HKP_OPERATION))
#define SEAHORSE_HKP_OPERATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_HKP_OPERATION, SeahorseHKPOperationClass))

typedef gboolean (*OpHKPCallback)   (SeahorseOperation *op, SoupMessage *message);
    
DECLARE_OPERATION (HKP, hkp)
    SeahorseHKPSource *hsrc;        /* The source to add keys to */
    SoupSession *session;           /* The HTTP session */
    guint total;                    /* Number of requests queued */
    guint requests;                 /* Number of requests remaining */
END_DECLARE_OPERATION

IMPLEMENT_OPERATION (HKP, hkp)

static void 
seahorse_hkp_operation_init (SeahorseHKPOperation *hop)
{
    hop->session = soup_session_async_new ();
}

static void 
seahorse_hkp_operation_dispose (GObject *gobject)
{
    SeahorseHKPOperation *hop = SEAHORSE_HKP_OPERATION (gobject);
    
    if (hop->hsrc) {
        g_object_unref (hop->hsrc);
        hop->hsrc = NULL;
    }

    if (hop->session) {
        g_object_unref (hop->session);
        hop->session = NULL;
    }
    
    G_OBJECT_CLASS (operation_parent_class)->dispose (gobject);  
}

static void 
seahorse_hkp_operation_finalize (GObject *gobject)
{
    SeahorseHKPOperation *hop = SEAHORSE_HKP_OPERATION (gobject);

    g_assert (hop->hsrc == NULL);
    g_assert (hop->session == NULL);
    
    G_OBJECT_CLASS (operation_parent_class)->finalize (gobject);  
}

static void 
seahorse_hkp_operation_cancel (SeahorseOperation *operation)
{
    SeahorseHKPOperation *hop;
    
    g_return_if_fail (SEAHORSE_IS_HKP_OPERATION (operation));
    hop = SEAHORSE_HKP_OPERATION (operation);
    
    if (hop->session != NULL) 
        soup_session_abort (hop->session);

    seahorse_operation_mark_done (operation, TRUE, NULL);
}

/* Cancels operation and marks the HKP operation as failed */
static void
fail_hkp_operation (SeahorseHKPOperation *hop, guint status, const gchar *msg)
{
    gchar *t;
    
    g_object_get (hop->hsrc, "key-server", &t, NULL);
    seahorse_operation_mark_done (SEAHORSE_OPERATION (hop), FALSE, 
            g_error_new (HKP_ERROR_DOMAIN, status, _("Couldn't communicate with '%s': %s"), 
                         t, msg ? msg : soup_status_get_phrase (status)));
    g_free (t);
}

static SeahorseHKPOperation*
setup_hkp_operation (SeahorseHKPSource *hsrc)
{
    SeahorseHKPOperation *hop;

    g_return_val_if_fail (SEAHORSE_IS_HKP_SOURCE (hsrc), NULL);
    
    hop = g_object_new (SEAHORSE_TYPE_HKP_OPERATION, NULL);
    hop->hsrc = hsrc;
    g_object_ref (hsrc);

    seahorse_operation_mark_start (SEAHORSE_OPERATION (hop));
    return hop;    
}

/* Thanks to GnuPG */
static void
dehtmlize(gchar *line)
{
    /* 
     * Remove anything <between brackets> and de-urlencode in place.  Note
     * that this requires all brackets to be closed on the same line.  It
     * also means that the result is never larger than the input. 
     */
    int parsedindex = 0;
    gchar *parsed = line;

    while (*line != '\0') {
        switch(*line) {
	    case '<':
	        while (*line != '>' && *line != '\0')
	            line++;
	        if (*line!='\0')
	            line++;
	        break;
	    case '&':
	        if ((*(line + 1) != '\0' && g_ascii_tolower (*(line + 1)) == 'l') &&
	            (*(line + 2) != '\0' && g_ascii_tolower (*(line + 2)) == 't') &&
	            (*(line + 3) != '\0' && *(line + 3) == ';')) {
	            parsed[parsedindex++] = '<';
	            line += 4;
	            break;
	        } else if ((*(line + 1) != '\0' && g_ascii_tolower (*(line + 1)) == 'g') &&
		               (*(line + 2) != '\0' && g_ascii_tolower (*(line + 2)) == 't') &&
		               (*(line + 3) != '\0' && *(line + 3) == ';')) {
	            parsed[parsedindex++] = '>';
	            line += 4;
	            break;
	        } else if((*(line + 1) != '\0' && g_ascii_tolower (*(line + 1)) == 'a') &&
		              (*(line + 2) != '\0' && g_ascii_tolower (*(line + 2))=='m') &&
		              (*(line + 3) != '\0' && g_ascii_tolower (*(line + 3))=='p') &&
		              (*(line + 4) != '\0' && *(line + 4) == ';')) {
	            parsed[parsedindex++] = '&';
	            line += 5;
	            break;
	        }
            /* Fall through */
        default:
        	parsed[parsedindex++] = *line;
	        line++;
	        break;
	    };
    }

    parsed[parsedindex] = '\0';

    /* 
     * Chop off any trailing whitespace.  Note that the HKP servers have
     * \r\n as line endings, and the NAI HKP servers have just \n. 
     */

    if(parsedindex > 0) {
        parsedindex--;
        while (g_ascii_isspace(((unsigned char*)parsed)[parsedindex])) {
	        parsed[parsedindex] = '\0';
	        if(parsedindex == 0)
	            break;
	        parsedindex--;
	    }
    }
}

unsigned int
parse_hkp_date (const gchar *text)
{
    int year, month, day;
    struct tm tmbuf;
    time_t stamp;

    if (strlen (text) != 10 || text[4] != '-' || text[7] != '-')
	    return 0;
    
    /* YYYY-MM-DD */
    sscanf (text, "%4d-%2d-%2d", &year, &month, &day);

    /* some basic checks */
    if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31)
	    return 0;
    
    memset (&tmbuf, 0, sizeof tmbuf);
    tmbuf.tm_mday = day;
    tmbuf.tm_mon = month - 1;
    tmbuf.tm_year = year - 1900;
    tmbuf.tm_isdst = -1;
    
    stamp = mktime (&tmbuf);
    return stamp == (time_t)-1 ? 0 : stamp;
}

static GList*
parse_hkp_index (const gchar *response)
{
    /* 
     * Luckily enough, both the HKP server and NAI HKP interface to their
     * LDAP server are close enough in output so the same function can
     * parse them both. 
     */

    /* pub  2048/<a href="/pks/lookup?op=get&search=0x3CB3B415">3CB3B415</a> 1998/04/03 David M. Shaw &lt;<a href="/pks/lookup?op=get&search=0x3CB3B415">dshaw@jabberwocky.com</a>&gt; */

    gchar **lines, **l;
    gchar **v;
    gchar *line, *t;
    
    GList *keys = NULL;
    gpgme_key_t key = NULL;
    
    lines = g_strsplit (response, "\n", 0);
    
    for (l = lines; *l; l++) {

        line = *l;
        dehtmlize(line);

        /* Start a new key */
        if (g_ascii_strncasecmp (line, "pub ", 4) == 0) {
            
            t = line + 4;
            while (*t && g_ascii_isspace (*t))
                t++;
            
            v = g_strsplit_set (t, " ", 3);
            if (!v[0] || !v[1] || !v[2]) {
                g_warning ("Invalid key line from server: %s", line);
                
            } else {
                gchar *fpr = NULL;
                unsigned int flags = 0;
                gpgme_pubkey_algo_t algo;
                gboolean has_uid = TRUE;

                /* Allocate a new key */
                key = gpgmex_key_alloc ();
                g_return_val_if_fail (key != NULL, keys);
                keys = g_list_prepend (keys, key);
                
                /* Cut the length and fingerprint */
                fpr = strchr (v[0], '/');
                if (fpr == NULL) {
                    g_warning ("couldn't find key fingerprint in line from server: %s", line);
                    fpr = "";
                } else {
                    *(fpr++) = 0;
                }
                
                /* Check out the key type */
                switch (g_ascii_toupper (v[0][strlen(v[0]) - 1])) {
                case 'D':
                    algo = GPGME_PK_DSA;
                    break;
                case 'R':
                    algo = GPGME_PK_RSA;
                    break;
                default:
                    algo = 0;
                    break;
                };

                /* Format the date for our parse function */
                g_strdelimit (v[1], "/", '-');
                
                /* Cleanup the UID */
                g_strstrip (v[2]);
            
                if (g_ascii_strcasecmp (v[2], "*** KEY REVOKED ***") == 0) {
                    flags |= GPGMEX_KEY_REVOKED;
                    has_uid = FALSE;
                } 
                
                /* Add all the info to the key */
                gpgmex_key_add_subkey (key, fpr, flags, 
                                       parse_hkp_date (v[1]), 
                                       0, strtol (v[0], NULL, 10), algo);

                /* And the UID if one was found */                
                if (has_uid)
                    gpgmex_key_add_uid (key, v[2], 0);
            }
            
            g_strfreev (v);
            
        /* A UID for the key */
        } else if (key && g_ascii_strncasecmp (line, "    ", 4) == 0) {
            
            g_strstrip (line);
            gpgmex_key_add_uid (key, line, 0);
            
        /* Signatures */
        } else if (key && g_ascii_strncasecmp (line, "sig ", 4) == 0) {
            
            /* TODO: Implement signatures */
            
        /* Other junk */
        } else {
            
            /* The end of the key */
            key = NULL;
        }
    }
    
    g_strfreev (lines);
                        
    return keys; 
}

static void 
refresh_callback (SoupMessage *msg, SeahorseHKPOperation *hop) 
{
    GList *keys, *k;
    gchar *t;
    
#ifdef _DEBUG
    g_printerr ("[hkp] Search Result:\n");
    dump_soup_response (msg);
#endif      
    
    if (SOUP_MESSAGE_IS_ERROR (msg)) {
        fail_hkp_operation (hop, msg->status_code, NULL);
        return;
    }
    
    t = g_new0 (gchar, msg->response.length + 1);
    strncpy (t, msg->response.body, msg->response.length);
    keys = parse_hkp_index (t);
    g_free (t);
    
    for (k = keys; k; k = g_list_next (k)) {
        seahorse_server_source_add_key (SEAHORSE_SERVER_SOURCE (hop->hsrc), (gpgme_key_t)(k->data));
        gpgmex_key_unref ((gpgme_key_t)(k->data));
    }
    
    g_list_free (keys);

    if (--hop->requests <= 0)
        seahorse_operation_mark_done (SEAHORSE_OPERATION (hop), FALSE, NULL);
    else
        seahorse_operation_mark_progress (SEAHORSE_OPERATION (hop), _("Searching for keys..."), 
                                          hop->requests, hop->total);
}

static gchar*
get_send_result (const gchar *response, unsigned int length)
{
    gchar **lines, **l;
    gchar *text, *t;
    gchar *last = NULL;
    gboolean is_error = FALSE;
    
    text = g_strndup (response, length);
    lines = g_strsplit (text, "\n", 0);    

    for (l = lines; *l; l++) {

        dehtmlize (*l);
        g_strstrip (*l);
        
        if (!(*l)[0])
            continue;

        t = g_ascii_strdown (*l, -1);
        
        /* Look for the word 'error' */
        if (strstr (t, "error"))
            is_error = TRUE;
        
        g_free (t);

        if ((*l)[0])
            last = *l;
    }
    
    /* Use last line as the message */
    last = is_error ? g_strdup (last) : NULL;
    
    g_strfreev (lines);
    g_free (text);
    
    return last;
}

static void 
send_callback (SoupMessage *msg, SeahorseHKPOperation *hop) 
{
    gchar *errmsg;

#ifdef _DEBUG
    g_printerr ("[hkp] Send Result:\n");
    dump_soup_response (msg);
#endif      
    
    if (SOUP_MESSAGE_IS_ERROR (msg)) {
        fail_hkp_operation (hop, msg->status_code, NULL);
        return;
    }
    
    errmsg = get_send_result (msg->response.body, msg->response.length);
    if (errmsg) {
        fail_hkp_operation (hop, 0, errmsg);
        g_free (errmsg);
        return;
    }
    
    /* A successful status from the server is all we want
       in this case */
    
    if (--hop->requests <= 0)
        seahorse_operation_mark_done (SEAHORSE_OPERATION (hop), FALSE, NULL);
    else
        seahorse_operation_mark_progress (SEAHORSE_OPERATION (hop), _("Uploading keys..."), 
                                          hop->requests, hop->total);
}

static void 
get_callback (SoupMessage *msg, SeahorseHKPOperation *hop) 
{
    SeahorseTextType type;
    const gchar *start;
    const gchar *end;
    gpgme_data_t data;
    const gchar *text;
    guint len;
    int r;
    
#ifdef _DEBUG
    g_printerr ("[hkp] Get Result:\n");
    dump_soup_response (msg);
#endif      
    
    if (SOUP_MESSAGE_IS_ERROR (msg)) {
        fail_hkp_operation (hop, msg->status_code, NULL);
        return;
    }
    
    end = text = msg->response.body;
    len = msg->response.length;
    
    do {

        len -= end - text;
        text = end;
        
        type = seahorse_util_detect_text (text, len, &start, &end);

        /* Any key blocks get written to our result data */
        if (type == SEAHORSE_TEXT_TYPE_KEY) {
            data = (gpgme_data_t)g_object_get_data (G_OBJECT (hop), "result");
            g_return_if_fail (data != NULL);
            
            r = gpgme_data_write (data, start, end - start);
            g_return_if_fail (r != -1);
        }

    } while (type != SEAHORSE_TEXT_TYPE_NONE);        
        
    if (--hop->requests <= 0)
        seahorse_operation_mark_done (SEAHORSE_OPERATION (hop), FALSE, NULL);
    else
        seahorse_operation_mark_progress (SEAHORSE_OPERATION (hop), _("Uploading keys..."), 
                                          hop->requests, hop->total);
}

/* -----------------------------------------------------------------------------
 *  SEAHORSE HKP SOURCE
 */
 
/* GObject handlers */
static void seahorse_hkp_source_class_init (SeahorseHKPSourceClass *klass);

/* SeahorseKeySource methods */
static SeahorseOperation*  seahorse_hkp_source_refresh    (SeahorseKeySource *src,
                                                           const gchar *key);
static SeahorseOperation*  seahorse_hkp_source_import     (SeahorseKeySource *sksrc, 
                                                           gpgme_data_t data);
static SeahorseOperation*  seahorse_hkp_source_export     (SeahorseKeySource *sksrc, 
                                                           GList *keys,     
                                                           gboolean complete,
                                                           gpgme_data_t data);

static SeahorseKeySourceClass *parent_class = NULL;

GType
seahorse_hkp_source_get_type (void)
{
    static GType type = 0;
 
    if (!type) {
        
        static const GTypeInfo tinfo = {
            sizeof (SeahorseHKPSourceClass), NULL, NULL,
            (GClassInitFunc) seahorse_hkp_source_class_init, NULL, NULL,
            sizeof (SeahorseHKPSource), 0, NULL
        };
        
        type = g_type_register_static (SEAHORSE_TYPE_SERVER_SOURCE, 
                                       "SeahorseHKPSource", &tinfo, 0);
    }
  
    return type;
}

/* Initialize the basic class stuff */
static void
seahorse_hkp_source_class_init (SeahorseHKPSourceClass *klass)
{
    SeahorseKeySourceClass *key_class;
   
    key_class = SEAHORSE_KEY_SOURCE_CLASS (klass);
    key_class->refresh = seahorse_hkp_source_refresh;
    key_class->import = seahorse_hkp_source_import;
    key_class->export = seahorse_hkp_source_export;

    parent_class = g_type_class_peek_parent (klass);
}

static SeahorseOperation*
seahorse_hkp_source_refresh (SeahorseKeySource *src, const gchar *key)
{
    SeahorseOperation *op;
    SeahorseHKPOperation *hop;
    SoupMessage *message;
    gchar *pattern = NULL;
    gchar *t, *uri, *server;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (src), NULL);
    g_return_val_if_fail (SEAHORSE_IS_HKP_SOURCE (src), NULL);
    g_return_val_if_fail (key != NULL, NULL);
    
    op = parent_class->refresh (src, key);
    if (op != NULL)
        return op;

    /* No way to find new keys */
    if (g_str_equal (key, SEAHORSE_KEY_SOURCE_NEW))
        return seahorse_operation_new_complete (NULL);
        
    /* All keys, parent_class will have cleared */
    else if(g_str_equal (key, SEAHORSE_KEY_SOURCE_ALL)) {
        
        g_object_get (src, "pattern", &pattern, NULL);
        g_return_val_if_fail (pattern && pattern[0], NULL);
        
        t = soup_uri_encode (pattern, "+=/\\()");
        g_free (pattern);
        pattern = t;
        
    /* Load a specific key */
    } else {

        /* TODO: Does this actually work? */
        pattern = soup_uri_encode (key, NULL);    
    } 
        
    hop = setup_hkp_operation (SEAHORSE_HKP_SOURCE (src));
    
    g_object_get (src, "key-server", &server, NULL);
    g_return_val_if_fail (server && server[0], FALSE);
    
    uri = g_strdup_printf ("http://%s/pks/lookup?op=index&search=%s", 
                           server, pattern);
    g_free (pattern);
    
    message = soup_message_new ("GET", uri);
    g_free (uri);
    
    soup_session_queue_message (hop->session, message, 
                                (SoupMessageCallbackFn)refresh_callback, hop);

#ifdef _DEBUG
    g_printerr ("[hkp] Search Request:\n");
    dump_soup_request (message);
#endif      
    
    hop->total = hop->requests = 1;
    t = g_strdup_printf (_("Searching for keys on: %s"), server);
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (hop), t, 0, hop->total);
    g_free (t);

    g_free (server);    
    
    seahorse_server_source_set_operation (SEAHORSE_SERVER_SOURCE (src), 
                                          SEAHORSE_OPERATION (hop));
    return SEAHORSE_OPERATION (hop);
}

static SeahorseOperation* 
seahorse_hkp_source_import (SeahorseKeySource *sksrc, gpgme_data_t data)
{
    SeahorseHKPOperation *hop;
    SeahorseHKPSource *hsrc;
    SoupMessage *message;
    GSList *keydata = NULL;
    GString *buf = NULL;
    gchar *key, *t, *server, *uri;
    GSList *l;
    guint len;
    
    g_return_val_if_fail (SEAHORSE_IS_HKP_SOURCE (sksrc), NULL);
    hsrc = SEAHORSE_HKP_SOURCE (sksrc);
    
    for (;;) {
     
        buf = g_string_sized_new (2048);
        len = seahorse_util_read_data_block (buf, data, "-----BEGIN PGP PUBLIC KEY BLOCK-----",
                                             "-----END PGP PUBLIC KEY BLOCK-----");
    
        if (len > 0) {
            keydata = g_slist_prepend (keydata, g_string_free (buf, FALSE));
        } else {
            g_string_free (buf, TRUE);
            break;
        }
    }
    
    if (g_slist_length (keydata) == 0)
        return seahorse_operation_new_complete (NULL);
    
    /* Figure out the URI we're sending to */    
    g_object_get (hsrc, "key-server", &server, NULL);
    g_return_val_if_fail (server && server[0], FALSE);
    uri = g_strdup_printf ("http://%s/pks/add", server);

    /* New operation and away we go */
    keydata = g_slist_reverse (keydata);   
    hop = setup_hkp_operation (hsrc);
    
    for (l = keydata; l; l = g_slist_next (l)) {
        g_return_val_if_fail (l->data != NULL, FALSE);
        t = soup_uri_encode((gchar*)(l->data), "+=/\\()");

        key = g_strdup_printf ("keytext=%s", t);
        g_free (t);

        message = soup_message_new ("POST", uri);
        soup_message_set_request (message, "application/x-www-form-urlencoded",
                                  SOUP_BUFFER_SYSTEM_OWNED, key, strlen (key));
        
        soup_session_queue_message (hop->session, message, 
                                    (SoupMessageCallbackFn)send_callback, hop);
        hop->requests++;

#ifdef _DEBUG
        g_printerr ("[hkp] Send Request:\n");
        dump_soup_request (message);
#endif      
    }

    hop->total = hop->requests;
    t = g_strdup_printf (_("Connecting to: %s"), server);
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (hop), t, 0, hop->total);
    g_free (t);

    g_free (server);
    g_free (uri);
    
    seahorse_util_string_slist_free (keydata);
    
    return SEAHORSE_OPERATION (hop);
}

static SeahorseOperation* 
seahorse_hkp_source_export (SeahorseKeySource *sksrc, GList *keys, 
                            gboolean complete, gpgme_data_t data)
{
    SeahorseHKPOperation *hop;
    SeahorseHKPSource *hsrc;
    SoupMessage *message;
    gchar *t, *server, *uri;
    const gchar *fpr;
    guint l;
    
    g_return_val_if_fail (SEAHORSE_IS_HKP_SOURCE (sksrc), NULL);
    hsrc = SEAHORSE_HKP_SOURCE (sksrc);
    
    if (g_list_length (keys) == 0)
        return seahorse_operation_new_complete (NULL);

    g_object_get (hsrc, "key-server", &server, NULL);
    g_return_val_if_fail (server && server[0], FALSE);

    /* New operation started */    
    hop = setup_hkp_operation (hsrc);

    if (data) {
        /* Note that we don't auto-free this */
        g_object_set_data (G_OBJECT (hop), "result", data);
    } else {
        /* But when we auto create a data object then we free it */
        gpgme_data_new (&data);
        g_return_val_if_fail (data != NULL, NULL);
        g_object_set_data_full (G_OBJECT (hop), "result", data, 
                                (GDestroyNotify)gpgme_data_release);
    }
    
    for ( ; keys; keys = g_list_next (keys)) {
        g_return_val_if_fail (SEAHORSE_IS_KEY (keys->data), NULL);

        /* Get the key id and limit it to 8 characters */
        fpr = seahorse_key_get_id (SEAHORSE_KEY (keys->data)->key);
        l = strlen (fpr);
        if (l > 8)
            fpr += (l - 8);

        /* The get key URI */
        uri = g_strdup_printf ("http://%s/pks/lookup?op=get&search=0x%s", 
                               server, fpr);

        message = soup_message_new ("GET", uri);
        g_free (uri);
        
        soup_session_queue_message (hop->session, message, 
                                    (SoupMessageCallbackFn)get_callback, hop);

        hop->requests++;

#ifdef _DEBUG
        g_printerr ("[hkp] Get Request:\n");
        dump_soup_request (message);
#endif              
    }
    
    hop->total = hop->requests;
    t = g_strdup_printf (_("Connecting to: %s"), server);
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (hop), t, 0, hop->total);
    g_free (t);

    g_free (server);
    return SEAHORSE_OPERATION (hop);    
}

/**
 * seahorse_hkp_source_new
 * @locsrc: Local source to base this key source off of
 * @server: The server to connect to 
 * @pattern: The pattern to search for
 * 
 * Creates a new key source for an HKP PGP server.
 * 
 * Returns: A new HKP Key Source
 */
SeahorseHKPSource*   
seahorse_hkp_source_new (SeahorseKeySource *locsrc, const gchar *server,
                          const gchar *pattern)
{
    SeahorseHKPSource *hsrc;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (locsrc) && 
                          !SEAHORSE_IS_SERVER_SOURCE (locsrc), NULL);
    g_return_val_if_fail (server && server[0], NULL);
    
    hsrc = g_object_new (SEAHORSE_TYPE_HKP_SOURCE, "local-source", locsrc,
                         "key-server", server, "pattern", pattern, NULL);

    return hsrc;  
}

/**
 * seahorse_hkp_is_valid_uri
 * @uri: The uri to check
 * 
 * Returns: Whether the passed uri is valid for an HKP key source
 */
gboolean              
seahorse_hkp_is_valid_uri (const gchar *uri)
{
    SoupUri *soup;
    gboolean ret = FALSE;
    gchar *t;
    
    /* Replace 'hkp' with 'http' at the beginning of the URI */
    if (strncasecmp (uri, "hkp:", 4) == 0) {
        t = g_strdup_printf("http:%s", uri + 4);
        soup = soup_uri_new (t);
        g_free (t);
        
    /* Not 'hkp', but maybe 'http' */
    } else {
        soup = soup_uri_new (uri);
    }
    
    if (soup) {
        /* Must be http or https, have a host. No querystring, user, path, passwd etc... */
        if ((soup->protocol == SOUP_PROTOCOL_HTTP || soup->protocol == SOUP_PROTOCOL_HTTPS) && 
            (soup->host && soup->host[0]) && !(soup->passwd && soup->passwd[0]) && 
            !(soup->query && soup->query[0]) && !(soup->user && soup->user[0]) && 
            !(soup->fragment && soup->fragment[0]) && 
            !(soup->path && soup->path[0] && !g_str_equal (soup->path, "/")))
            ret = TRUE;
        soup_uri_free (soup);
    }

    return ret;
}

#endif /* WITH_HKP */
