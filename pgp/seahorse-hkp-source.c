/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */
 
#include "config.h"
 
#include <stdlib.h>
#include <string.h>
 
#include <glib/gi18n.h>

#include "seahorse-hkp-source.h"

#include "seahorse-pgp-key.h"
#include "seahorse-pgp-subkey.h"
#include "seahorse-pgp-uid.h"
#include "seahorse-servers.h"

#include "seahorse-object-list.h"
#include "seahorse-progress.h"
#include "seahorse-util.h"

#include <libsoup/soup.h>

#define DEBUG_FLAG SEAHORSE_DEBUG_HKP
#include "seahorse-debug.h"

/**
 * SECTION: seahorse-hkp-source
 * @short_description: Implements the HKP (HTTP Keyserver protocol) Source object
 * @include: seahorse-hkp-source.h
 *
 * See: http://tools.ietf.org/html/draft-shaw-openpgp-hkp-00
 **/

#ifdef WITH_HKP

#define PGP_KEY_BEGIN   "-----BEGIN PGP PUBLIC KEY BLOCK-----"
#define PGP_KEY_END     "-----END PGP PUBLIC KEY BLOCK-----"

#define SOUP_MESSAGE_IS_ERROR(msg) \
    (SOUP_STATUS_IS_TRANSPORT_ERROR((msg)->status_code) || \
     SOUP_STATUS_IS_CLIENT_ERROR((msg)->status_code) || \
     SOUP_STATUS_IS_SERVER_ERROR((msg)->status_code))
    
#define HKP_ERROR_DOMAIN (get_hkp_error_domain())

/**
*
* Returns The GQuark for the HKP error
*
**/
static GQuark
get_hkp_error_domain (void)
{
    static GQuark q = 0;
    if(q == 0)
        q = g_quark_from_static_string ("seahorse-hkp-error");
    return q;
}

/**
* src: The SeahorseSource to use as server for the uri
* path: The path to add to the SOUP uri
*
*
*
* Returns A Soup uri with server, port and paths
**/
static SoupURI*
get_http_server_uri (SeahorseHKPSource *self, const char *path)
{
    SoupURI *uri;
    gchar *server, *port;

    g_object_get (self, "key-server", &server, NULL);
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

static SoupSession *
create_hkp_soup_session (void)
{
	SoupSession *session;
#if WITH_DEBUG
	SoupLogger *logger;
#endif

        session = soup_session_async_new_with_options (SOUP_SESSION_ADD_FEATURE_BY_TYPE,
                                                       SOUP_TYPE_PROXY_RESOLVER_DEFAULT,
                                                       NULL);


#if WITH_DEBUG
	if (seahorse_debugging) {
		logger = soup_logger_new (SOUP_LOGGER_LOG_BODY, -1);
		soup_session_add_feature (session, SOUP_SESSION_FEATURE (logger));
		g_object_unref (logger);
	}
#endif

	return session;
}


/* Thanks to GnuPG */
/**
* line: The line to modify
*
*
* Remove anything <between brackets> and de-urlencode in place.  Note
* that this requires all brackets to be closed on the same line.  It
* also means that the result is never larger than the input.
*
**/
static void
dehtmlize(gchar *line)
{
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

/**
 * parse_hkp_date:
 * @text: The date string to parse, YYYY-MM-DD
 *
 *
 *
 * Returns: 0 on error or the timestamp
 */
static unsigned int
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

/**
* response: The HKP server response to parse
*
* Extracts the key data from the HKP server response
*
* Returns A GList of keys
**/
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
    
	SeahorsePgpKey *key = NULL;
	GList *keys = NULL;
	GList *subkeys = NULL;
	GList *uids = NULL;
	guint flags;
    
	lines = g_strsplit (response, "\n", 0);
    
	for (l = lines; *l; l++) {

		line = *l;	
		dehtmlize (line);

		seahorse_debug ("%s", line);

		/* Start a new key */
		if (g_ascii_strncasecmp (line, "pub ", 4) == 0) {
            
			t = line + 4;
			while (*t && g_ascii_isspace (*t))
				t++;
            
			v = g_strsplit_set (t, " ", 3);
			if (!v[0] || !v[1] || !v[2]) {
				g_message ("Invalid key line from server: %s", line);
                
			} else {
				gchar *fingerprint, *fpr = NULL;
				const gchar *algo;
				gboolean has_uid = TRUE;
				SeahorsePgpSubkey *subkey;
				
				flags = SEAHORSE_FLAG_EXPORTABLE;
       	
				/* Cut the length and fingerprint */
				fpr = strchr (v[0], '/');
				if (fpr == NULL) {
					g_message ("couldn't find key fingerprint in line from server: %s", line);
					fpr = "";
				} else {
					*(fpr++) = 0;
				}
                
				/* Check out the key type */
				switch (g_ascii_toupper (v[0][strlen(v[0]) - 1])) {
				case 'D':
					algo = "DSA";
					break;
				case 'R':
					algo = "RSA";
					break;
				default:
					algo = "";
					break;
				};

				/* Format the date for our parse function */
				g_strdelimit (v[1], "/", '-');
                
				/* Cleanup the UID */
				g_strstrip (v[2]);
            
				if (g_ascii_strcasecmp (v[2], "*** KEY REVOKED ***") == 0) {
					flags |= SEAHORSE_FLAG_REVOKED;
					has_uid = FALSE;
				} 
				
				if (key) {
					seahorse_pgp_key_set_uids (SEAHORSE_PGP_KEY (key), uids);
					seahorse_object_list_free (uids);
					seahorse_pgp_key_set_subkeys (SEAHORSE_PGP_KEY (key), subkeys);
					seahorse_object_list_free (subkeys);
					seahorse_pgp_key_realize (SEAHORSE_PGP_KEY (key));
					uids = subkeys = NULL;
					key = NULL;
				}

				key = seahorse_pgp_key_new ();
				keys = g_list_prepend (keys, key);
				g_object_set (key, "object-flags", flags, NULL);

				/* Add all the info to the key */
				subkey = seahorse_pgp_subkey_new ();
				seahorse_pgp_subkey_set_keyid (subkey, fpr);
				fingerprint = seahorse_pgp_subkey_calc_fingerprint (fpr);
				seahorse_pgp_subkey_set_fingerprint (subkey, fingerprint);
				g_free (fingerprint);
				seahorse_pgp_subkey_set_flags (subkey, flags);
				seahorse_pgp_subkey_set_created (subkey, parse_hkp_date (v[1]));
				seahorse_pgp_subkey_set_length (subkey, strtol (v[0], NULL, 10));
				seahorse_pgp_subkey_set_algorithm (subkey, algo);
				subkeys = g_list_prepend (subkeys, subkey);

				/* And the UID if one was found */                
				if (has_uid) {
					SeahorsePgpUid *uid = seahorse_pgp_uid_new (key, v[2]);
					uids = g_list_prepend (uids, uid);
				}
			}
            
			g_strfreev (v);
            
		/* A UID for the key */
		} else if (key && g_ascii_strncasecmp (line, "    ", 4) == 0) {
            
			SeahorsePgpUid *uid;
			
			g_strstrip (line);
			uid = seahorse_pgp_uid_new (key, line);
			uids = g_list_prepend (uids, uid);
            
		/* Signatures */
		} else if (key && g_ascii_strncasecmp (line, "sig ", 4) == 0) {
            
			/* TODO: Implement signatures */
            
		} 
	}
    
	g_strfreev (lines);

	if (key) {
		seahorse_pgp_key_set_uids (SEAHORSE_PGP_KEY (key), g_list_reverse (uids));
		seahorse_object_list_free (uids);
		seahorse_pgp_key_set_subkeys (SEAHORSE_PGP_KEY (key), g_list_reverse (subkeys));
		seahorse_object_list_free (subkeys);
		seahorse_pgp_key_realize (SEAHORSE_PGP_KEY (key));
	}
	
	return keys; 
}

/**
* response: The server response
*
* Parses the response and extracts an error message
*
* Free the error message with g_free after usage
*
* Returns NULL if there was no error. The error message else
**/
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

/**
 * detect_key:
 * @text: The ASCII armoured key
 * @len: Length of the ASCII block or -1
 * @start: Returned, start of the key
 * @end: Returned, end of the key
 *
 * Finds a key in a char* block
 *
 * Returns: FALSE if no key is contained, TRUE else
 */
static gboolean
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

/* -----------------------------------------------------------------------------
 *  SEAHORSE HKP SOURCE
 */

G_DEFINE_TYPE (SeahorseHKPSource, seahorse_hkp_source, SEAHORSE_TYPE_SERVER_SOURCE);

static void 
seahorse_hkp_source_init (SeahorseHKPSource *hsrc)
{

}

static gboolean
hkp_message_propagate_error (SeahorseHKPSource *self,
                             SoupMessage *message,
                             GError **error)
{
	gchar *text, *server;

	if (!SOUP_MESSAGE_IS_ERROR (message))
		return FALSE;

	if (message->status_code == SOUP_STATUS_CANCELLED) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED,
		                     _("The operation was cancelled"));
		return TRUE;
	}

	g_object_get (self, "key-server", &server, NULL);

	/* Make the body lower case, and no tags */
	text = g_strndup (message->response_body->data, message->response_body->length);
	if (text != NULL) {
		dehtmlize (text);
		seahorse_util_string_lower (text);
	}

	if (text && strstr (text, "no keys")) {
		g_free (text);
		return FALSE; /* not found is not an error */
	} else if (text && strstr (text, "too many")) {
		g_set_error (error, HKP_ERROR_DOMAIN, 0,
		             _("Search was not specific enough. Server '%s' found too many keys."), server);
	} else {
		g_set_error (error, HKP_ERROR_DOMAIN, message->status_code,
		             _("Couldn't communicate with server '%s': %s"),
		             server, message->reason_phrase);
	}

	g_free (text);
	return TRUE;
}

static void
on_session_cancelled (GCancellable *cancellable,
                     gpointer user_data)
{
	SoupSession *session = user_data;
	soup_session_abort (session);
}

typedef struct {
	SeahorseHKPSource *source;
	GCancellable *cancellable;
	gulong cancelled_sig;
	SoupSession *session;
	gint requests;
	GcrSimpleCollection *results;
} source_search_closure;

static void
source_search_free (gpointer data)
{
	source_search_closure *closure = data;
	g_object_unref (closure->source);
	g_cancellable_disconnect (closure->cancellable, closure->cancelled_sig);
	g_clear_object (&closure->cancellable);
	g_object_unref (closure->session);
	g_clear_object (&closure->results);
	g_free (closure);
}

static void
on_search_message_complete (SoupSession *session,
                            SoupMessage *message,
                            gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_search_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;
	GList *keys, *l;

	seahorse_progress_end (closure->cancellable, message);

	if (hkp_message_propagate_error (closure->source, message, &error)) {
		g_simple_async_result_take_error (res, error);

	} else {
		keys = parse_hkp_index (message->response_body->data);
		for (l = keys; l; l = g_list_next (l)) {
			g_object_set (l->data, "place", closure->source, NULL);
			gcr_simple_collection_add (closure->results, l->data);
		}
		g_list_free_full (keys, g_object_unref);
	}

	g_simple_async_result_complete_in_idle (res);
	g_object_unref (res);
}

static gboolean
is_hex_keyid (const gchar *match)
{
	const gchar *text = match;

	if (strlen (match) != 8)
		return FALSE;

	while (*text != '\0') {
		if (!((*text >= 0x30 && *text <= 0x39) ||
		      (*text >= 0x41 && *text <= 0x46) ||
		      (*text >= 0x61 && *text <= 0x66)))
			return FALSE;
		text++;
	}

	return TRUE;
}
static void
seahorse_hkp_source_search_async (SeahorseServerSource *source,
                                  const gchar *match,
                                  GcrSimpleCollection *results,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	SeahorseHKPSource *self = SEAHORSE_HKP_SOURCE (source);
	source_search_closure *closure;
	GSimpleAsyncResult *res;
	SoupMessage *message;
	GHashTable *form;
	SoupURI *uri;
	gchar hexfpr[11];

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_hkp_source_search_async);
	closure = g_new0 (source_search_closure, 1);
	closure->source = g_object_ref (self);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	closure->session = create_hkp_soup_session ();
	closure->results = g_object_ref (results);
	g_simple_async_result_set_op_res_gpointer (res, closure, source_search_free);

	uri = get_http_server_uri (self, "/pks/lookup");
	g_return_if_fail (uri);

	form = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (form, "op", "index");

	if (is_hex_keyid (match)) {
		strncpy (hexfpr, "0x", 3);
		strncpy (hexfpr + 2, match, 9);
		g_hash_table_insert (form, "search", hexfpr);
	} else {
		g_hash_table_insert (form, "search", (char *)match);
	}

	soup_uri_set_query_from_form (uri, form);
	g_hash_table_destroy (form);

	message = soup_message_new_from_uri ("GET", uri);
	soup_session_queue_message (closure->session, message,
	                            on_search_message_complete, g_object_ref (res));

	seahorse_progress_prep_and_begin (cancellable, message, NULL);

	if (cancellable)
		closure->cancelled_sig = g_cancellable_connect (cancellable,
		                                                G_CALLBACK (on_session_cancelled),
		                                                closure->session, NULL);

	soup_uri_free (uri);
	g_object_unref (res);
}

static gboolean
seahorse_hkp_source_search_finish (SeahorseServerSource *source,
                                   GAsyncResult *result,
                                   GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_hkp_source_search_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

typedef struct {
	SeahorseHKPSource *source;
	GInputStream *input;
	GCancellable *cancellable;
	gulong cancelled_sig;
	SoupSession *session;
	gint requests;
} source_import_closure;

static void
source_import_free (gpointer data)
{
	source_import_closure *closure = data;
	g_object_unref (closure->source);
	g_object_unref (closure->input);
	g_cancellable_disconnect (closure->cancellable, closure->cancelled_sig);
	g_clear_object (&closure->cancellable);
	g_object_unref (closure->session);
	g_free (closure);
}

static void
on_import_message_complete (SoupSession *session,
                            SoupMessage *message,
                            gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_import_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;
	gchar *errmsg;

	g_assert (closure->requests > 0);
	seahorse_progress_end (closure->cancellable, GUINT_TO_POINTER (closure->requests));
	closure->requests--;

	if (hkp_message_propagate_error (closure->source, message, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);

	} else if ((errmsg = get_send_result (message->response_body->data)) != NULL) {
		g_set_error (&error, HKP_ERROR_DOMAIN, message->status_code, "%s", errmsg);
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);
		g_free (errmsg);

	/* A successful status from the server is all we want in this case */
	} else {
		if (closure->requests == 0)
			g_simple_async_result_complete_in_idle (res);
	}

	g_object_unref (res);
}

/**
* sksrc: The HKP source to use
* input: The input stream to add
*
* Imports a list of keys from the input stream to the keyserver
**/
static void
seahorse_hkp_source_import_async (SeahorseServerSource *source,
                                  GInputStream *input,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	SeahorseHKPSource *self = SEAHORSE_HKP_SOURCE (source);
	GSimpleAsyncResult *res;
	source_import_closure *closure;
	SoupMessage *message;
	GList *keydata = NULL;
	GString *buf = NULL;
	GHashTable *form;
	gchar *key;
	SoupURI *uri;
	GList *l;
	guint len;

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_hkp_source_import_async);
	closure = g_new0 (source_import_closure, 1);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	closure->input = g_object_ref (input);
	closure->source = g_object_ref (self);
	closure->session = create_hkp_soup_session ();
	g_simple_async_result_set_op_res_gpointer (res, closure, source_import_free);

	for (;;) {

		buf = g_string_sized_new (2048);
		len = seahorse_util_read_data_block (buf, input,
		                                     "-----BEGIN PGP PUBLIC KEY BLOCK-----",
		                                     "-----END PGP PUBLIC KEY BLOCK-----");

		if (len > 0) {
			keydata = g_list_prepend (keydata, g_string_free (buf, FALSE));
		} else {
			g_string_free (buf, TRUE);
			break;
		}
	}

	if (g_list_length (keydata) == 0) {
		g_simple_async_result_complete_in_idle (res);
		g_object_unref (res);
		return;
	}

	/* Figure out the URI we're sending to */
	uri = get_http_server_uri (self, "/pks/add");
	g_return_if_fail (uri);

	/* New operation and away we go */
	keydata = g_list_reverse (keydata);

	form = g_hash_table_new (g_str_hash, g_str_equal);
	for (l = keydata; l; l = g_list_next (l)) {
		g_assert (l->data != NULL);
		g_hash_table_remove_all (form);

		g_hash_table_insert (form, "keytext", l->data);
		key = soup_form_encode_urlencoded (form);

		message = soup_message_new_from_uri ("POST", uri);
		soup_message_set_request (message, "application/x-www-form-urlencoded",
		                          SOUP_MEMORY_TAKE, key, strlen (key));

		soup_session_queue_message (closure->session, message,
		                            on_import_message_complete, g_object_ref (res));

		closure->requests++;
		seahorse_progress_prep_and_begin (cancellable, GUINT_TO_POINTER (closure->requests), NULL);
	}
	g_hash_table_destroy (form);

	if (cancellable)
		closure->cancelled_sig = g_cancellable_connect (cancellable,
		                                                G_CALLBACK (on_session_cancelled),
		                                                closure->session, NULL);

	soup_uri_free (uri);

	for (l = keydata; l != NULL; l = g_list_next (l))
		g_free (l->data);
	g_list_free (keydata);

	g_object_unref (res);
}

static GList *
seahorse_hkp_source_import_finish (SeahorseServerSource *source,
                                   GAsyncResult *result,
                                   GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_hkp_source_import_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	/* We don't know which keys got imported, so just return NULL */
	return NULL;
}


typedef struct {
	SeahorseHKPSource *source;
	GString *data;
	GCancellable *cancellable;
	gulong cancelled_sig;
	SoupSession *session;
	gint requests;
} ExportClosure;

static void
export_closure_free (gpointer data)
{
	ExportClosure *closure = data;
	g_object_unref (closure->source);
	if (closure->data)
		g_string_free (closure->data, TRUE);
	g_cancellable_disconnect (closure->cancellable, closure->cancelled_sig);
	g_clear_object (&closure->cancellable);
	g_object_unref (closure->session);
	g_free (closure);
}

static void
on_export_message_complete (SoupSession *session,
                            SoupMessage *message,
                            gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	ExportClosure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;
	const gchar *start;
	const gchar *end;
	const gchar *text;
	guint len;

	seahorse_progress_end (closure->cancellable, message);

	if (hkp_message_propagate_error (closure->source, message, &error)) {
		g_simple_async_result_take_error (res, error);
	} else {
		end = text = message->response_body->data;
		len = message->response_body->length;

		for (;;) {

			len -= end - text;
			text = end;

			if (!detect_key (text, len, &start, &end))
				break;

			g_string_append_len (closure->data, start, end - start);
			g_string_append_c (closure->data, '\n');
		}
	}

	g_assert (closure->requests > 0);
	closure->requests--;

	if (closure->requests == 0)
		g_simple_async_result_complete_in_idle (res);

	g_object_unref (res);
}

/**
* sksrc: A HKP source
* keyids: the keyids to look up
* output: The output stream the data will end in
*
* Gets data from the keyserver, writes it to the output stream
**/
static void
seahorse_hkp_source_export_async (SeahorseServerSource *source,
                                  const gchar **keyids,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	SeahorseHKPSource *self = SEAHORSE_HKP_SOURCE (source);
	ExportClosure *closure;
	GSimpleAsyncResult *res;
	SoupMessage *message;
	SoupURI *uri;
	const gchar *fpr;
	gchar hexfpr[11];
	GHashTable *form;
	guint len;
	gint i;

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_hkp_source_export_async);
	closure = g_new0 (ExportClosure, 1);
	closure->source = g_object_ref (self);
	closure->data = g_string_sized_new (1024);
	closure->session = create_hkp_soup_session ();
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	g_simple_async_result_set_op_res_gpointer (res, closure, export_closure_free);

	if (!keyids || !keyids[0]) {
		g_simple_async_result_complete_in_idle (res);
		g_object_unref (res);
		return;
	}

	uri = get_http_server_uri (self, "/pks/lookup");
	g_return_if_fail (uri);

	/* prepend the hex prefix (0x) to make keyservers happy */
	strncpy (hexfpr, "0x", 3);

	form = g_hash_table_new (g_str_hash, g_str_equal);
	for (i = 0; keyids[i] != NULL; i++) {
		g_hash_table_remove_all (form);

		/* Get the key id and limit it to 8 characters */
		fpr = keyids[i];
		len = strlen (fpr);
		if (len > 8)
			fpr += (len - 8);
	
		strncpy (hexfpr + 2, fpr, 9);

		/* The get key URI */
		g_hash_table_insert (form, "op", "get");
		g_hash_table_insert (form, "search", (char *)hexfpr);
		soup_uri_set_query_from_form (uri, form);

		message = soup_message_new_from_uri ("GET", uri);

		soup_session_queue_message (closure->session, message,
		                            on_export_message_complete,
		                            g_object_ref (res));

		closure->requests++;
		seahorse_progress_prep_and_begin (cancellable, message, NULL);
	}

	if (cancellable)
		closure->cancelled_sig = g_cancellable_connect (cancellable,
		                                                G_CALLBACK (on_session_cancelled),
		                                                closure->session, NULL);

	g_hash_table_destroy (form);
	g_object_unref (res);
}

static gpointer
seahorse_hkp_source_export_finish (SeahorseServerSource *source,
                                   GAsyncResult *result,
                                   gsize *size,
                                   GError **error)
{
	ExportClosure *closure;
	gpointer output;

	g_return_val_if_fail (size != NULL, NULL);
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_hkp_source_export_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	closure = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
	*size = closure->data->len;
	output = g_string_free (closure->data, FALSE);
	closure->data = NULL;
	return output;
}

/**
* klass:
*
* Initialize the basic class stuff
*
**/
static void
seahorse_hkp_source_class_init (SeahorseHKPSourceClass *klass)
{
	SeahorseServerSourceClass *server_class = SEAHORSE_SERVER_SOURCE_CLASS (klass);

	server_class->search_async = seahorse_hkp_source_search_async;
	server_class->search_finish = seahorse_hkp_source_search_finish;
	server_class->export_async = seahorse_hkp_source_export_async;
	server_class->export_finish = seahorse_hkp_source_export_finish;
	server_class->import_async = seahorse_hkp_source_import_async;
	server_class->import_finish = seahorse_hkp_source_import_finish;

	seahorse_servers_register_type ("hkp", _("HTTP Key Server"), seahorse_hkp_is_valid_uri);
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
            soup->host && soup->host[0] && g_str_equal (soup->path ? soup->path : "/", "/"))
            ret = TRUE;
        soup_uri_free (soup);
    }

    return ret;
}

#endif /* WITH_HKP */
