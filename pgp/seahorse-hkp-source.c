/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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

#include "seahorse-hkp-source.h"

#include "seahorse-gpgmex.h"
#include "seahorse-pgp-key.h"

#include "seahorse-gconf.h"
#include "seahorse-operation.h"
#include "seahorse-util.h"

#include <libsoup/soup.h>

#ifdef WITH_HKP

/* Override the DEBUG_HKP_ENABLE switch here */
/* #define DEBUG_HKP_ENABLE 1 */

#ifndef DEBUG_HKP_ENABLE
#if _DEBUG
#define DEBUG_HKP_ENABLE 1
#else
#define DEBUG_HKP_ENABLE 0
#endif
#endif

#define PGP_KEY_BEGIN   "-----BEGIN PGP PUBLIC KEY BLOCK-----"
#define PGP_KEY_END     "-----END PGP PUBLIC KEY BLOCK-----"

/* Amount of keys to load in a batch */
#define DEFAULT_LOAD_BATCH 30

#define SOUP_MESSAGE_IS_ERROR(msg) \
    (SOUP_STATUS_IS_TRANSPORT_ERROR((msg)->status_code) || \
     SOUP_STATUS_IS_CLIENT_ERROR((msg)->status_code) || \
     SOUP_STATUS_IS_SERVER_ERROR((msg)->status_code))
    
#define HKP_ERROR_DOMAIN (get_hkp_error_domain())

#define GCONF_USE_HTTP_PROXY    "/system/http_proxy/use_http_proxy"
#define GCONF_HTTP_PROXY_HOST   "/system/http_proxy/host"
#define GCONF_PROXY_PORT        "/system/http_proxy/port"
#define GCONF_USE_AUTH          "/system/http_proxy/use_authentication"
#define GCONF_AUTH_USER         "/system/http_proxy/authentication_user"
#define GCONF_AUTH_PASS         "/system/http_proxy/authentication_password"

static GQuark
get_hkp_error_domain ()
{
    static GQuark q = 0;
    if(q == 0)
        q = g_quark_from_static_string ("seahorse-hkp-error");
    return q;
}

static SoupURI*
get_http_server_uri (SeahorseKeySource *src, const char *path)
{
    SoupURI *uri;
    gchar *server, *port;

    g_object_get (src, "key-server", &server, NULL);
    g_return_val_if_fail (server != NULL, NULL);

    uri = soup_uri_new (NULL);
    soup_uri_set_scheme (uri, SOUP_URI_SCHEME_HTTP);

    /* If it already has a port then use that */
    port = strchr (server, ':');
    if (port) {
        *port++ = '\0';
        soup_uri_set_port (uri, atoi (port));
    } else {
        /* default HKP port */
        soup_uri_set_port (uri, 11371); 
    }

    soup_uri_set_host (uri, server);
    soup_uri_set_path (uri, path);
    g_free (server);

    return uri;
}

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
    SeahorseHKPSource *hsrc;        /* The source  */
    SoupSession *session;           /* The HTTP session */
    guint total;                    /* Number of requests queued */
    guint requests;                 /* Number of requests remaining */
    gboolean cancelling;            /* Cancelling the request, don't process */
END_DECLARE_OPERATION

IMPLEMENT_OPERATION (HKP, hkp)

static void 
seahorse_hkp_operation_init (SeahorseHKPOperation *hop)
{
    SoupURI *uri;
    gchar *host;
#if DEBUG_HKP_ENABLE
    SoupLogger *logger;
#endif
    
    if (seahorse_gconf_get_boolean (GCONF_USE_HTTP_PROXY)) {
        
        host = seahorse_gconf_get_string (GCONF_HTTP_PROXY_HOST);
        if (host) {
            uri = soup_uri_new (NULL);
            
            if (!uri) {
                g_warning ("creation of SoupURI from '%s' failed", host);
                
            } else {

                soup_uri_set_scheme (uri, SOUP_URI_SCHEME_HTTP);
                soup_uri_set_host (uri, host);
                g_free (host);
                soup_uri_set_port (uri, seahorse_gconf_get_integer (GCONF_PROXY_PORT));
                
                if (seahorse_gconf_get_boolean (GCONF_USE_AUTH)) {
                    char *user, *pass;

                    user = seahorse_gconf_get_string (GCONF_AUTH_USER);
                    soup_uri_set_user (uri, user);
                    g_free (user);
                    pass = seahorse_gconf_get_string (GCONF_AUTH_PASS);
                    soup_uri_set_password (uri, pass);
                    g_free (pass);
                }
                
                hop->session = soup_session_async_new_with_options (SOUP_SESSION_PROXY_URI, uri, NULL);
                soup_uri_free (uri);
            }
        }
    }
    
    /* Without a proxy */
    if (!hop->session)
        hop->session = soup_session_async_new ();

#if DEBUG_HKP_ENABLE
    logger = soup_logger_new (SOUP_LOGGER_LOG_BODY, -1);
    soup_logger_attach (logger, hop->session);
    g_object_unref (logger);
#endif
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
    
    g_assert (SEAHORSE_IS_HKP_OPERATION (operation));
    hop = SEAHORSE_HKP_OPERATION (operation);
    
    g_return_if_fail (seahorse_operation_is_running (operation));
    hop->cancelling = TRUE;
    
    if (hop->session != NULL) 
        soup_session_abort (hop->session);
    
    seahorse_operation_mark_done (operation, TRUE, NULL);
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
    
    g_return_if_fail (line);

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

/* Cancels operation and marks the HKP operation as failed */
static void
fail_hkp_operation (SeahorseHKPOperation *hop, SoupMessage *msg, const gchar *text)
{
    gchar *t, *server;
    GError *error = NULL;
    
    if (!seahorse_operation_is_running (SEAHORSE_OPERATION (hop)))
        return;

    g_object_get (hop->hsrc, "key-server", &server, NULL);

    if (text) {
        error = g_error_new (HKP_ERROR_DOMAIN, msg ? msg->status_code : 0, text);

    } else if (msg) {
        /* Make the body lower case, and no tags */
        t = g_strndup (msg->response_body->data, msg->response_body->length);
        if (t != NULL) {
            dehtmlize (t);        
            seahorse_util_string_lower (t);
        }

        if (t && strstr (t, "no keys"))
            error = NULL; /* not found is not an error */
        else if (t && strstr (t, "too many"))
            error = g_error_new (HKP_ERROR_DOMAIN, 0, _("Search was not specific enough. Server '%s' found too many keys."), server);
        else 
            error = g_error_new (HKP_ERROR_DOMAIN, msg->status_code, _("Couldn't communicate with server '%s': %s"),
                                 server, msg->reason_phrase);
        g_free (t);
    } else {

        /* We should always have msg or text */
        g_assert (FALSE);
    }        

    seahorse_operation_mark_done (SEAHORSE_OPERATION (hop), FALSE, error);
    g_free (server);
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
add_key (SeahorseHKPSource *ssrc, gpgme_key_t key)
{
    SeahorseKey *prev;
    SeahorsePGPKey *pkey;
    GQuark keyid;
       
    keyid = seahorse_pgp_key_get_cannonical_id (seahorse_pgp_key_get_id (key, 0));
    prev = seahorse_context_get_key (SCTX_APP (), SEAHORSE_KEY_SOURCE (ssrc), keyid);
    
    if (prev != NULL) {
        g_return_if_fail (SEAHORSE_IS_PGP_KEY (prev));
        gpgmex_combine_keys (SEAHORSE_PGP_KEY (prev)->pubkey, key);
        seahorse_key_changed (prev, SKEY_CHANGE_UIDS);
        return;
    }

    /* A public key */
    pkey = seahorse_pgp_key_new (SEAHORSE_KEY_SOURCE (ssrc), key, NULL);

    /* Add to context */ 
    seahorse_context_add_key (SCTX_APP (), SEAHORSE_KEY (pkey));
}

static void 
refresh_callback (SoupSession *session, SoupMessage *msg, SeahorseHKPOperation *hop) 
{
    GList *keys, *k;
    
    if (hop->cancelling)
        return;
    
    if (SOUP_MESSAGE_IS_ERROR (msg)) {
        fail_hkp_operation (hop, msg, NULL);
        return;
    }
    
    keys = parse_hkp_index (msg->response_body->data);
    
    for (k = keys; k; k = g_list_next (k)) {
        add_key (hop->hsrc, (gpgme_key_t)(k->data));
        gpgmex_key_unref ((gpgme_key_t)(k->data));
    }
    
    g_list_free (keys);

    if (--hop->requests <= 0)
        seahorse_operation_mark_done (SEAHORSE_OPERATION (hop), FALSE, NULL);
    else
        seahorse_operation_mark_progress_full (SEAHORSE_OPERATION (hop), _("Searching for keys..."), 
                                               hop->requests, hop->total);
}

static gchar*
get_send_result (const gchar *response)
{
    gchar **lines, **l;
    gchar *t;
    gchar *last = NULL;
    gboolean is_error = FALSE;
    
    if (!*response)
        return g_strdup ("");
    
    lines = g_strsplit (response, "\n", 0); 

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
    
    return last;
}

static void 
send_callback (SoupSession *session, SoupMessage *msg, SeahorseHKPOperation *hop) 
{
    gchar *errmsg;

    if (hop->cancelling)
        return;

    if (SOUP_MESSAGE_IS_ERROR (msg)) {
        fail_hkp_operation (hop, msg, NULL);
        return;
    }
    
    errmsg = get_send_result (msg->response_body->data);
    if (errmsg) {
        fail_hkp_operation (hop, NULL, errmsg);
        g_free (errmsg);
        return;
    }
    
    /* A successful status from the server is all we want
       in this case */
    
    if (--hop->requests <= 0)
        seahorse_operation_mark_done (SEAHORSE_OPERATION (hop), FALSE, NULL);
    else
        seahorse_operation_mark_progress_full (SEAHORSE_OPERATION (hop), _("Uploading keys..."), 
                                               hop->requests, hop->total);
}

gboolean
detect_key (const gchar *text, gint len, const gchar **start, const gchar **end)
{
    const gchar *t;

    if (len == -1)
        len = strlen (text);
    
    /* Find the first of the headers */
    if((t = g_strstr_len (text, len, PGP_KEY_BEGIN)) == NULL)
        return FALSE;
    if (start)
        *start = t;
        
    /* Find the end of that block */
    if((t = g_strstr_len (t, len - (t - text), PGP_KEY_END)) == NULL)
        return FALSE;
    if (end)
        *end = t;
    
    return TRUE; 
}

static void 
get_callback (SoupSession *session, SoupMessage *msg, SeahorseHKPOperation *hop) 
{
    GError *err = NULL;
    const gchar *start;
    const gchar *end;
    GOutputStream *output;
    const gchar *text;
    gboolean ret;
    guint len;
    gsize written;
    
    if (hop->cancelling)
        return;
    
    if (SOUP_MESSAGE_IS_ERROR (msg)) {
        fail_hkp_operation (hop, msg, NULL);
        return;
    }
    
    end = text = msg->response_body->data;
    len = msg->response_body->length;
    
    for (;;) {

        len -= end - text;
        text = end;
        
        if(!detect_key (text, len, &start, &end))
            break;
        
		/* Any key blocks get written to our result data */
		output = seahorse_operation_get_result (SEAHORSE_OPERATION (hop));
		g_return_if_fail (G_IS_OUTPUT_STREAM (output));

		ret = g_output_stream_write_all (output, start, end - start, &written, NULL, &err) &&
		      g_output_stream_write_all (output, "\n", 1, &written, NULL, &err) &&
		      g_output_stream_flush (output, NULL, &err);

		if (!ret) {
			seahorse_operation_mark_done (SEAHORSE_OPERATION (hop), FALSE, err);
			return;
		}
    	}
        
    	if (--hop->requests <= 0) {
    		output = seahorse_operation_get_result (SEAHORSE_OPERATION (hop));
    		g_return_if_fail (G_IS_OUTPUT_STREAM (output));
    		g_output_stream_close (output, NULL, &err);
    		seahorse_operation_mark_done (SEAHORSE_OPERATION (hop), FALSE, err);
    	} else {
    		seahorse_operation_mark_progress_full (SEAHORSE_OPERATION (hop), _("Retrieving keys..."), 
    		                                       hop->requests, hop->total);
    	}
}

 
/* -----------------------------------------------------------------------------
 *  SEAHORSE HKP SOURCE
 */

enum {
    PROP_0,
    PROP_KEY_TYPE,
    PROP_KEY_DESC
};

G_DEFINE_TYPE (SeahorseHKPSource, seahorse_hkp_source, SEAHORSE_TYPE_SERVER_SOURCE);

static void 
seahorse_hkp_source_init (SeahorseHKPSource *hsrc)
{

}

static void 
seahorse_hkp_source_get_property (GObject *object, guint prop_id, GValue *value,
                                  GParamSpec *pspec)
{
    switch (prop_id) {
    case PROP_KEY_TYPE:
        g_value_set_uint (value, SEAHORSE_PGP);
        break;
    case PROP_KEY_DESC:
        g_value_set_string (value, _("PGP Key"));
        break;
    };        
}

static SeahorseOperation*
seahorse_hkp_source_search (SeahorseKeySource *src, const gchar *match)
{
    SeahorseHKPOperation *hop;
    SoupMessage *message;
    GHashTable *form;
    gchar *t;
    SoupURI *uri;
    
    g_assert (SEAHORSE_IS_KEY_SOURCE (src));
    g_assert (SEAHORSE_IS_HKP_SOURCE (src));

    hop = setup_hkp_operation (SEAHORSE_HKP_SOURCE (src));
    
    uri = get_http_server_uri (src, "/pks/lookup");
    g_return_val_if_fail (uri, FALSE);
    
    form = g_hash_table_new (g_str_hash, g_str_equal);
    g_hash_table_insert (form, "op", "index");
    g_hash_table_insert (form, "search", (char *)match);
    soup_uri_set_query_from_form (uri, form);
    g_hash_table_destroy (form);
    
    message = soup_message_new_from_uri ("GET", uri);
    
    soup_session_queue_message (hop->session, message, 
                                (SoupSessionCallback)refresh_callback, hop);

    hop->total = hop->requests = 1;
    t = g_strdup_printf (_("Searching for keys on: %s"), uri->host);
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (hop), t, -1);
    g_free (t);

    soup_uri_free (uri);

    seahorse_server_source_take_operation (SEAHORSE_SERVER_SOURCE (src), 
                                           SEAHORSE_OPERATION (hop));
    g_object_ref (hop);
    return SEAHORSE_OPERATION (hop);
}

static SeahorseOperation*
seahorse_hkp_source_load (SeahorseKeySource *src, GQuark keyid)
{
    SeahorseOperation *op;
    
    g_assert (SEAHORSE_IS_KEY_SOURCE (src));
    g_assert (SEAHORSE_IS_HKP_SOURCE (src));
    
    op = SEAHORSE_KEY_SOURCE_CLASS (seahorse_hkp_source_parent_class)->load (src, keyid);
    if (op != NULL)
        return op;

    /* No way to find new all or new keys */
    if (!keyid)
        return seahorse_operation_new_complete (NULL);

    /* TODO: Does this actually work? */
    return seahorse_hkp_source_search (src, seahorse_key_get_rawid (keyid));        
}

static SeahorseOperation* 
seahorse_hkp_source_import (SeahorseKeySource *sksrc, GInputStream *input)
{
    SeahorseHKPOperation *hop;
    SeahorseHKPSource *hsrc;
    SoupMessage *message;
    GSList *keydata = NULL;
    GString *buf = NULL;
    GHashTable *form;
    gchar *key, *t;
    SoupURI *uri;
    GSList *l;
    guint len;
    
    g_return_val_if_fail (SEAHORSE_IS_HKP_SOURCE (sksrc), NULL);
    hsrc = SEAHORSE_HKP_SOURCE (sksrc);
    
    g_return_val_if_fail (G_IS_INPUT_STREAM (input), NULL);
    g_object_ref (input);
    
    for (;;) {
     
        buf = g_string_sized_new (2048);
        len = seahorse_util_read_data_block (buf, input, "-----BEGIN PGP PUBLIC KEY BLOCK-----",
                                             "-----END PGP PUBLIC KEY BLOCK-----");
    
        if (len > 0) {
            keydata = g_slist_prepend (keydata, g_string_free (buf, FALSE));
        } else {
            g_string_free (buf, TRUE);
            break;
        }
    }
    
    	if (g_slist_length (keydata) == 0) {
    		g_object_unref (input);
    		return seahorse_operation_new_complete (NULL);
    	}
    
    /* Figure out the URI we're sending to */    
    uri = get_http_server_uri (sksrc, "/pks/add");
    g_return_val_if_fail (uri, FALSE);

    /* New operation and away we go */
    keydata = g_slist_reverse (keydata);   
    hop = setup_hkp_operation (hsrc);
    
    form = g_hash_table_new (g_str_hash, g_str_equal);
    for (l = keydata; l; l = g_slist_next (l)) {
        g_assert (l->data != NULL);

        g_hash_table_insert (form, "keytext", l->data);
        key = soup_form_encode_urlencoded (form);

        message = soup_message_new_from_uri ("POST", uri);
        soup_message_set_request (message, "application/x-www-form-urlencoded",
                                  SOUP_MEMORY_TAKE, key, strlen (key));
        
        soup_session_queue_message (hop->session, message, 
                                    (SoupSessionCallback)send_callback, hop);
        hop->requests++;
    }
    g_hash_table_destroy (form);

    hop->total = hop->requests;
    t = g_strdup_printf (_("Connecting to: %s"), uri->host);
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (hop), t, -1);
    g_free (t);

    soup_uri_free (uri);
    
    	seahorse_util_string_slist_free (keydata);
    	g_object_unref (input);
    
    return SEAHORSE_OPERATION (hop);
}

static SeahorseOperation*  
seahorse_hkp_source_export_raw (SeahorseKeySource *sksrc, GSList *keyids,
                                GOutputStream *output)
{
    SeahorseHKPOperation *hop;
    SeahorseHKPSource *hsrc;
    SoupMessage *message;
    gchar *t;
    SoupURI *uri;
    const gchar *fpr;
    gchar hexfpr[11];
    GHashTable *form;
    guint len;
    GSList *l;
    
    g_return_val_if_fail (SEAHORSE_IS_HKP_SOURCE (sksrc), NULL);
    g_return_val_if_fail (output == NULL || G_IS_OUTPUT_STREAM (output), NULL);

    hsrc = SEAHORSE_HKP_SOURCE (sksrc);
    
    if (g_slist_length (keyids) == 0)
        return seahorse_operation_new_complete (NULL);

    uri = get_http_server_uri (sksrc, "/pks/lookup");
    g_return_val_if_fail (uri, FALSE);

    /* New operation started */    
    hop = setup_hkp_operation (hsrc);

	g_object_ref (output);
	seahorse_operation_mark_result (SEAHORSE_OPERATION (hop), output, g_object_unref);

    /* prepend the hex prefix (0x) to make keyservers happy */
    strncpy(hexfpr, "0x", 3);

    form = g_hash_table_new (g_str_hash, g_str_equal);
    for (l = keyids; l; l = g_slist_next (l)) {

        /* Get the key id and limit it to 8 characters */
        fpr = (const char*)seahorse_key_get_rawid (GPOINTER_TO_UINT (l->data));
        len = strlen (fpr);
        if (len > 8)
            fpr += (len - 8);
	
        strncpy(hexfpr + 2, fpr, 9);

        /* The get key URI */
        g_hash_table_insert (form, "op", "get");
        g_hash_table_insert (form, "search", (char *)hexfpr);
        soup_uri_set_query_from_form (uri, form);

        message = soup_message_new_from_uri ("GET", uri);
        
        soup_session_queue_message (hop->session, message, 
                                    (SoupSessionCallback)get_callback, hop);

        hop->requests++;
    }
    g_hash_table_destroy (form);
    
    hop->total = hop->requests;
    t = g_strdup_printf (_("Connecting to: %s"), uri->host);
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (hop), t, -1);
    g_free (t);

    soup_uri_free (uri);
    return SEAHORSE_OPERATION (hop);    
}

/* Initialize the basic class stuff */
static void
seahorse_hkp_source_class_init (SeahorseHKPSourceClass *klass)
{
	GObjectClass *gobject_class;
	SeahorseKeySourceClass *key_class;
   
	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->get_property = seahorse_hkp_source_get_property;

	key_class = SEAHORSE_KEY_SOURCE_CLASS (klass);
	key_class->canonize_keyid = seahorse_pgp_key_get_cannonical_id;
	key_class->load = seahorse_hkp_source_load;
	key_class->search = seahorse_hkp_source_search;
	key_class->import = seahorse_hkp_source_import;
	key_class->export_raw = seahorse_hkp_source_export_raw;

	seahorse_hkp_source_parent_class = g_type_class_peek_parent (klass);

	g_object_class_install_property (gobject_class, PROP_KEY_TYPE,
	        g_param_spec_uint ("key-type", "Key Type", "Key type that originates from this key source.", 
	                           0, G_MAXUINT, SKEY_UNKNOWN, G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_KEY_DESC,
	        g_param_spec_string ("key-desc", "Key Desc", "Description for keys that originate here.",
	                             NULL, G_PARAM_READABLE));
	    
	seahorse_registry_register_type (NULL, SEAHORSE_TYPE_HKP_SOURCE, "key-source", "remote", SEAHORSE_PGP_STR, NULL);
}


/**
 * seahorse_hkp_source_new
 * @uri: The server to connect to 
 * 
 * Creates a new key source for an HKP PGP server.
 * 
 * Returns: A new HKP Key Source
 */
SeahorseHKPSource*   
seahorse_hkp_source_new (const gchar *uri, const gchar *host)
{
    g_return_val_if_fail (seahorse_hkp_is_valid_uri (uri), NULL);
    g_return_val_if_fail (host && *host, NULL);
    return g_object_new (SEAHORSE_TYPE_HKP_SOURCE, "uri", uri,
                         "key-server", host, NULL);
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
    SoupURI *soup;
    gboolean ret = FALSE;
    gchar *t;
    
    g_return_val_if_fail (uri && uri[0], FALSE);
    
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
        if ((soup->scheme == SOUP_URI_SCHEME_HTTP || soup->scheme == SOUP_URI_SCHEME_HTTPS) && 
            !soup->user && !soup->password && !soup->query && !soup->fragment &&
            g_str_equal (soup->path, "/"))
            ret = TRUE;
        soup_uri_free (soup);
    }

    return ret;
}

#endif /* WITH_HKP */
