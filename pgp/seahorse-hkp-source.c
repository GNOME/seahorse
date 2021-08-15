/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>

#include "seahorse-hkp-source.h"

#include "seahorse-pgp-key.h"
#include "seahorse-pgp-subkey.h"
#include "seahorse-pgp-uid.h"

#include "seahorse-common.h"

#include "libseahorse/seahorse-progress.h"
#include "libseahorse/seahorse-util.h"

#include <libsoup/soup.h>

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

G_DEFINE_QUARK (seahorse-hkp-error, seahorse_hkp_error);

struct _SeahorseHKPSource {
    SeahorseServerSource parent;
};

G_DEFINE_TYPE (SeahorseHKPSource, seahorse_hkp_source, SEAHORSE_TYPE_SERVER_SOURCE);

/**
 * self: The SeahorseSource to use as server for the uri
 * path: The path to add to the SOUP uri
 *
 * Returns: A #SoupUri with server, port and paths
 */
static SoupURI*
get_http_server_uri (SeahorseHKPSource *self, const char *path)
{
    g_autoptr(SoupURI) uri = NULL;
    g_autofree char *conf_uri = NULL;

    conf_uri = seahorse_place_get_uri (SEAHORSE_PLACE (self));
    g_return_val_if_fail (conf_uri != NULL, NULL);

    if (strncasecmp (conf_uri, "hkp:", 4) == 0) {
        g_autofree char *t = g_strdup_printf ("http:%s", conf_uri + 4);
        uri = soup_uri_new (t);
    } else if (strncasecmp (conf_uri, "hkps:", 5) == 0) {
        g_autofree char *t = g_strdup_printf ("https:%s", conf_uri + 5);
        uri = soup_uri_new (t);
    }

    soup_uri_set_path (uri, path);

    g_debug ("HTTP Server URI: %s", soup_uri_to_string(uri, FALSE));
    return g_steal_pointer (&uri);
}

static SoupSession *
create_hkp_soup_session (void)
{
    SoupSession *session;
#ifdef WITH_DEBUG
    g_autoptr(SoupLogger) logger = NULL;
    const gchar *env;
#endif

    session = soup_session_new ();

#ifdef WITH_DEBUG
    env = g_getenv ("G_MESSAGES_DEBUG");
    if (env && strstr (env, "seahorse")) {
        logger = soup_logger_new (SOUP_LOGGER_LOG_BODY, -1);
        soup_session_add_feature (session, SOUP_SESSION_FEATURE (logger));
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
dehtmlize (gchar *line)
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

    if (parsedindex > 0) {
        parsedindex--;
        while (g_ascii_isspace (((unsigned char*)parsed)[parsedindex])) {
            parsed[parsedindex] = '\0';
            if (parsedindex == 0)
                break;
            parsedindex--;
        }
    }
}

/**
* flags: combintation of [rei] representing the key's status
*
* Parses the flags from the HKP output
*
* returns 0 on error or a combination of seahorse flags based on input
**/
static guint
parse_hkp_flags (char *flags)
{
    char flag = 0;

    g_return_val_if_fail (flags, 0);

    for (char *f = flags; *f; f++){
        switch (*f){
            case 'r':
                flag |= SEAHORSE_FLAG_REVOKED;
                break;
            case 'e':
                flag |= SEAHORSE_FLAG_EXPIRED;
                break;
            case 'd':
                flag |= SEAHORSE_FLAG_DISABLED;
                break;
        }
    }
    return flag;
}

/**
 * parse_hkp_index:
 * response: The HKP server response to parse
 *
 * Extracts the key data from the HKP server response
 *
 * Returns: (transfer full): The parsed list of keys
 */
GList *
seahorse_hkp_parse_lookup_response (const char *response)
{
    /*
     * Use The OpenPGP HTTP Keyserver Protocol (HKP) to search and get keys
     * https://tools.ietf.org/html/draft-shaw-openpgp-hkp-00#section-5
     */
    g_auto(GStrv) lines = NULL;
    SeahorsePgpKey *key = NULL;
    GList *keys = NULL;
    guint key_total = 0, key_count = 0;

    lines = g_strsplit (response, "\n", 0);
    for (char **l = lines; *l; l++) {
        char *line = *l;
        g_auto(GStrv) columns = NULL;

        if (!*line) {
          g_debug ("HKP Parser: skip empty line");
          continue;
        }

        g_debug ("%s", line);

        /* split the line using hkp delimiter */
        columns = g_strsplit_set (line, ":", 7);

        /* info header */
        /* info:<version>:<count> */
        if (g_ascii_strncasecmp (columns[0], "info", 4) == 0) {
            if (!columns[1] && !columns[2]){
                g_debug("HKP Parse: Invalid info line: %s", line);
            } else {
                key_total = strtol(columns[2], NULL, 10);
            }

        /* start a new key */
        /* pub:<keyid>:<algo>:<keylen>:<creationdate>:<expirationdate>:<flags> */
        } else if (g_ascii_strncasecmp (columns[0], "pub", 3) == 0) {
            const char *fpr;
            g_autofree char *fingerprint = NULL;
            const char *algo = NULL;
            g_autoptr (SeahorsePgpSubkey) subkey = NULL;
            long created = 0, expired = 0;
            g_autoptr(GDateTime) created_date = NULL;
            g_autoptr(GDateTime) expired_date = NULL;
            SeahorseFlags flags;

            key_count++;

            if (!columns[0] || !columns[1] || !columns[2] || !columns[3] || !columns[4]) {
                g_message ("Invalid key line from server: %s", line);
                continue;
            }

            /* Cut the length and fingerprint */
            fpr = columns[1];
            if (fpr == NULL)
                g_message ("couldn't find key fingerprint in line from server: %s", line);

            /* Check out the key type */
            switch (strtol(columns[2], NULL, 10)) {
                case 1:
                case 2:
                case 3:
                     algo = "RSA";
                    break;
                case 17:
                    algo = "DSA";
                    break;
                default:
                   break;
            }
            g_debug ("Algo: %s", algo);

            /* set dates */
            /* created */
            if (!columns[4]) {
                g_debug ("HKP Parse: No created date for key on line: %s", line);
            } else {
                created = strtol (columns[4], NULL, 10);
                if (created > 0)
                    created_date = g_date_time_new_from_unix_utc (created);
            }

            /* expires (optional) */
            if (columns[5]) {
                expired = strtol (columns[5], NULL, 10);
                if (expired > 0)
                    expired_date = g_date_time_new_from_unix_utc (expired);
            }

            /* set flags (optional) */
            flags = SEAHORSE_FLAG_EXPORTABLE;
            if (columns[6])
                flags |= parse_hkp_flags (columns[6]);

            /* create key */
            g_debug("HKP Parse: found new key");
            key = seahorse_pgp_key_new ();
            g_object_set (key, "object-flags", flags, NULL);

            /* Add all the info to the key */
            subkey = seahorse_pgp_subkey_new ();
            seahorse_pgp_subkey_set_keyid (subkey, fpr);

            fingerprint = seahorse_pgp_subkey_calc_fingerprint (fpr);
            seahorse_pgp_subkey_set_fingerprint (subkey, fingerprint);

            seahorse_pgp_subkey_set_flags (subkey, flags);
            seahorse_pgp_subkey_set_created (subkey, created_date);
            seahorse_pgp_subkey_set_expires (subkey, expired_date);
            seahorse_pgp_subkey_set_length (subkey, strtol (columns[3], NULL, 10));
            if (algo)
                seahorse_pgp_subkey_set_algorithm (subkey, algo);
            seahorse_pgp_key_add_subkey (key, subkey);

            /* Now add it to the list */
            keys = g_list_prepend (keys, key);

        /* A UID for the key */
        } else if (g_ascii_strncasecmp (columns[0], "uid", 3) == 0) {
            g_autoptr (SeahorsePgpUid) uid = NULL;
            g_autofree char *uid_string = NULL;

            if (!key) {
                g_debug("HKP Parse: Warning: seen uid line before keyline, skipping");
                continue;
            }

            g_debug("HKP Parse: handle uid");

            if (!columns[0] || !columns[1] || !columns[2]) {
                g_message ("HKP Parse: Invalid uid line from server: %s", line);
                continue;
            }

            uid_string = g_uri_unescape_string (columns[1], NULL);
            g_debug("HKP Parse: decoded uid string: %s", uid_string);

            uid = seahorse_pgp_uid_new (key, uid_string);
            seahorse_pgp_key_add_uid (key, uid);
        }
    }

    /* Make sure the keys are realized */
    for (GList *k = keys; k; k = g_list_next (k))
        seahorse_pgp_key_realize (SEAHORSE_PGP_KEY (k->data));

    if (key_total != 0 && key_total != key_count) {
        g_message ("HKP Parse; Warning: Issue during HKP parsing, only %d keys were parsed out of %d", key_count, key_total);
    } else {
        g_debug ("HKP Parse: %d keys parsed successfully", key_count);
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
static char*
get_send_result (const char *response)
{
    g_auto(GStrv) lines = NULL;
    char *last = NULL;
    gboolean is_error = FALSE;

    if (!*response)
        return g_strdup ("");

    lines = g_strsplit (response, "\n", 0);
    for (char **l = lines; *l; l++) {
        g_autofree char *t = NULL;

        dehtmlize (*l);
        g_strstrip (*l);

        if (!(*l)[0])
            continue;

        /* Look for the word 'error' */
        t = g_ascii_strdown (*l, -1);
        if (strstr (t, "error"))
            is_error = TRUE;

        if ((*l)[0])
            last = *l;
    }

    /* Use last line as the message */
    return is_error ? g_strdup (last) : NULL;
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
detect_key (const char *text, int len, const char **start, const char **end)
{
    const gchar *t;

    if (len == -1)
        len = strlen (text);

    /* Find the first of the headers */
    if ((t = g_strstr_len (text, len, PGP_KEY_BEGIN)) == NULL)
        return FALSE;
    if (start)
        *start = t;

    /* Find the end of that block */
    if ((t = g_strstr_len (t, len - (t - text), PGP_KEY_END)) == NULL)
        return FALSE;
    if (end)
        *end = t;

    return TRUE;
}

static gboolean
hkp_message_propagate_error (SeahorseHKPSource *self,
                             SoupMessage *message,
                             GError **error)
{
    g_autofree char *uri = NULL;
    g_autofree char *text = NULL;

    if (!SOUP_MESSAGE_IS_ERROR (message))
        return FALSE;

    if (message->status_code == SOUP_STATUS_CANCELLED) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                             _("The operation was cancelled"));
        return TRUE;
    }

    uri = seahorse_place_get_uri (SEAHORSE_PLACE (self));

    /* Make the body lower case, and no tags */
    text = g_strndup (message->response_body->data, message->response_body->length);
    if (text != NULL) {
        char *text_lower;

        dehtmlize (text);
        text_lower = g_ascii_strdown (text, -1);
        g_free (text);
        text = text_lower;
    }

    if (text && strstr (text, "no keys"))
        return FALSE; /* not found is not an error */

    if (text && strstr (text, "too many")) {
        g_set_error (error, HKP_ERROR_DOMAIN, 0,
                     _("Search was not specific enough. Server “%s” found too many keys."),
                     uri);
    } else {
        g_set_error (error, HKP_ERROR_DOMAIN, message->status_code,
                     _("Couldn’t communicate with server “%s”: %s"),
                     uri, message->reason_phrase);
    }

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
    int requests;
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
    g_autoptr(GTask) task = G_TASK (user_data);
    source_search_closure *closure = g_task_get_task_data (task);
    g_autoptr(GError) error = NULL;
    GList *keys, *l;

    seahorse_progress_end (closure->cancellable, message);

    if (hkp_message_propagate_error (closure->source, message, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    keys = seahorse_hkp_parse_lookup_response (message->response_body->data);
    for (l = keys; l; l = g_list_next (l)) {
        g_object_set (l->data, "place", closure->source, NULL);
        gcr_simple_collection_add (closure->results, l->data);
    }
    g_list_free_full (keys, g_object_unref);

    g_task_return_boolean (task, TRUE);
}

static gboolean
is_hex_keyid (const char *match)
{
    size_t match_len;

    /* See HKP draft, section 3.1.1.1 */
    match_len = strlen (match);
    if (match_len != 8 || match_len != 16 || match_len != 32 || match_len != 40)
        return FALSE;

    for (size_t i = 0; i < match_len; i++)
        if (!g_ascii_isxdigit (match[i]))
            return FALSE;

    return TRUE;
}

static void
seahorse_hkp_source_search_async (SeahorseServerSource *source,
                                  const char           *match,
                                  GcrSimpleCollection  *results,
                                  GCancellable         *cancellable,
                                  GAsyncReadyCallback   callback,
                                  void                 *user_data)
{
    SeahorseHKPSource *self = SEAHORSE_HKP_SOURCE (source);
    source_search_closure *closure;
    g_autoptr(GTask) task = NULL;
    g_autoptr(GHashTable) form = NULL;
    SoupMessage *message;
    g_autoptr(SoupURI) uri = NULL;
    g_autofree char *uri_str = NULL;

    task = g_task_new (source, cancellable, callback, user_data);
    closure = g_new0 (source_search_closure, 1);
    closure->source = g_object_ref (self);
    closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
    closure->session = create_hkp_soup_session ();
    closure->results = g_object_ref (results);
    g_task_set_task_data (task, closure, source_search_free);

    uri = get_http_server_uri (self, "/pks/lookup");
    g_return_if_fail (uri);

    form = g_hash_table_new (g_str_hash, g_str_equal);
    g_hash_table_insert (form, "op", "index");
    g_hash_table_insert (form, "options", "mr");

    if (is_hex_keyid (match)) {
        g_autofree char *hexfpr = NULL;

        hexfpr = g_strdup_printf ("0x%s", match);
        g_hash_table_insert (form, "search", hexfpr);
    } else {
        g_hash_table_insert (form, "search", (char *)match);
    }

    g_hash_table_insert (form, "fingerprint", "on");

    soup_uri_set_query_from_form (uri, form);
    message = soup_message_new_from_uri ("GET", uri);

    seahorse_progress_prep_and_begin (cancellable, message, NULL);

    uri_str = soup_uri_to_string (uri, TRUE);
    g_debug ("Sending HKP search query to '%s'", uri_str);

    soup_session_queue_message (closure->session, g_steal_pointer (&message),
                                on_search_message_complete,
                                g_steal_pointer (&task));

    if (cancellable)
        closure->cancelled_sig = g_cancellable_connect (cancellable,
                                                        G_CALLBACK (on_session_cancelled),
                                                        closure->session, NULL);
}

static gboolean
seahorse_hkp_source_search_finish (SeahorseServerSource *source,
                                   GAsyncResult *result,
                                   GError **error)
{
    g_return_val_if_fail (SEAHORSE_IS_HKP_SOURCE (source), FALSE);
    g_return_val_if_fail (g_task_is_valid (result, source), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

typedef struct {
    SeahorseHKPSource *source;
    GInputStream *input;
    GCancellable *cancellable;
    gulong cancelled_sig;
    SoupSession *session;
    int requests;
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
    g_autoptr(GTask) task = G_TASK (user_data);
    source_import_closure *closure = g_task_get_task_data (task);
    g_autoptr(GError) error = NULL;
    g_autofree gchar *errmsg = NULL;

    g_assert (closure->requests > 0);
    seahorse_progress_end (closure->cancellable, GUINT_TO_POINTER (closure->requests));
    closure->requests--;

    if (hkp_message_propagate_error (closure->source, message, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    if ((errmsg = get_send_result (message->response_body->data)) != NULL) {
        g_task_return_new_error (task, HKP_ERROR_DOMAIN, message->status_code,
                                 "%s", errmsg);
        return;
    }

    /* A successful status from the server is all we want in this case */
    if (closure->requests == 0) {
        /* We don't know which keys got imported, so just return NULL */
        g_task_return_pointer (task, NULL, NULL);
    }
}

static void
seahorse_hkp_source_import_async (SeahorseServerSource *source,
                                  GInputStream *input,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
    SeahorseHKPSource *self = SEAHORSE_HKP_SOURCE (source);
    g_autoptr(GTask) task = NULL;
    source_import_closure *closure;
    g_autoptr(GList) keydata = NULL;
    g_autoptr(GHashTable) form = NULL;
    g_autoptr(SoupURI) uri = NULL;
    GList *l;

    task = g_task_new (source, cancellable, callback, user_data);
    closure = g_new0 (source_import_closure, 1);
    closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
    closure->input = g_object_ref (input);
    closure->source = g_object_ref (self);
    closure->session = create_hkp_soup_session ();
    g_task_set_task_data (task, closure, source_import_free);

    for (;;) {
        g_autoptr(GString) buf = g_string_sized_new (2048);
        guint len;

        len = seahorse_util_read_data_block (buf, input,
                                             "-----BEGIN PGP PUBLIC KEY BLOCK-----",
                                             "-----END PGP PUBLIC KEY BLOCK-----");
        if (len <= 0)
            break;

        keydata = g_list_prepend (keydata,
                                  g_string_free (g_steal_pointer (&buf), FALSE));
    }

    if (g_list_length (keydata) == 0) {
        g_task_return_pointer (task, NULL, NULL);
        return;
    }

    /* Figure out the URI we're sending to */
    uri = get_http_server_uri (self, "/pks/add");
    g_return_if_fail (uri);

    /* New operation and away we go */
    keydata = g_list_reverse (keydata);

    form = g_hash_table_new (g_str_hash, g_str_equal);
    for (l = keydata; l; l = g_list_next (l)) {
        g_autoptr(SoupMessage) message = NULL;
        gchar *key;

        g_assert (l->data != NULL);
        g_hash_table_remove_all (form);

        g_hash_table_insert (form, "keytext", l->data);
        key = soup_form_encode_urlencoded (form);

        message = soup_message_new_from_uri ("POST", uri);
        soup_message_set_request (message, "application/x-www-form-urlencoded",
                                  SOUP_MEMORY_TAKE, key, strlen (key));

        soup_session_queue_message (closure->session,
                                    g_steal_pointer (&message),
                                    on_import_message_complete,
                                    g_object_ref (task));

        closure->requests++;
        seahorse_progress_prep_and_begin (cancellable, GUINT_TO_POINTER (closure->requests), NULL);
    }

    if (cancellable)
        closure->cancelled_sig = g_cancellable_connect (cancellable,
                                                        G_CALLBACK (on_session_cancelled),
                                                        closure->session, NULL);

    for (l = keydata; l != NULL; l = g_list_next (l))
        g_free (l->data);
}

static GList *
seahorse_hkp_source_import_finish (SeahorseServerSource *source,
                                   GAsyncResult *result,
                                   GError **error)
{
    g_return_val_if_fail (g_task_is_valid (result, source), NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}


typedef struct {
    SeahorseHKPSource *source;
    GString *data;
    gsize data_len;
    GCancellable *cancellable;
    gulong cancelled_sig;
    SoupSession *session;
    int requests;
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
    g_autoptr(GTask) task = G_TASK (user_data);
    ExportClosure *closure = g_task_get_task_data (task);
    g_autoptr(GError) error = NULL;
    const gchar *start, *end, *text;
    guint len;

    seahorse_progress_end (closure->cancellable, message);

    if (hkp_message_propagate_error (closure->source, message, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

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

    g_assert (closure->requests > 0);
    closure->requests--;

    if (closure->requests == 0) {
        closure->data_len = closure->data->len;
        g_task_return_pointer (task,
                               g_string_free (g_steal_pointer (&closure->data), FALSE),
                               g_free);
    }
}

static void
seahorse_hkp_source_export_async (SeahorseServerSource *source,
                                  const gchar **keyids,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
    SeahorseHKPSource *self = SEAHORSE_HKP_SOURCE (source);
    ExportClosure *closure;
    g_autoptr(GTask) task = NULL;
    SoupURI *uri;
    g_autoptr(GHashTable) form = NULL;

    task = g_task_new (self, cancellable, callback, user_data);
    closure = g_new0 (ExportClosure, 1);
    closure->source = g_object_ref (self);
    closure->data = g_string_sized_new (1024);
    closure->session = create_hkp_soup_session ();
    closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
    g_task_set_task_data (task, closure, export_closure_free);

    if (!keyids || !keyids[0]) {
        g_task_return_pointer (task, NULL, NULL);
        return;
    }

    uri = get_http_server_uri (self, "/pks/lookup");
    g_return_if_fail (uri);

    form = g_hash_table_new (g_str_hash, g_str_equal);
    for (int i = 0; keyids[i] != NULL; i++) {
        const char *fpr = keyids[i];
        size_t len;
        g_autofree char *hexfpr = NULL;
        SoupMessage *message;

        g_hash_table_remove_all (form);

        /* Get the key id and limit it to 16 characters */
        len = strlen (fpr);
        if (len > 16)
            fpr += (len - 16);

        /* prepend the hex prefix (0x) to make keyservers happy */
        hexfpr = g_strdup_printf ("0x%s", fpr);

        /* The get key URI */
        g_hash_table_insert (form, "op", "get");
        g_hash_table_insert (form, "search", (char *)hexfpr);
        soup_uri_set_query_from_form (uri, form);

        message = soup_message_new_from_uri ("GET", uri);

        soup_session_queue_message (closure->session, message,
                                    on_export_message_complete,
                                    g_object_ref (task));

        closure->requests++;
        seahorse_progress_prep_and_begin (cancellable, message, NULL);
    }

    if (cancellable)
        closure->cancelled_sig = g_cancellable_connect (cancellable,
                                                        G_CALLBACK (on_session_cancelled),
                                                        closure->session, NULL);
}

static gpointer
seahorse_hkp_source_export_finish (SeahorseServerSource *source,
                                   GAsyncResult *result,
                                   gsize *size,
                                   GError **error)
{
    ExportClosure *closure;

    g_return_val_if_fail (size != NULL, NULL);
    g_return_val_if_fail (g_task_is_valid (result, source), NULL);

    closure = g_task_get_task_data (G_TASK (result));
    *size = closure->data_len;
    return g_task_propagate_pointer (G_TASK (result), error);
}

static void
seahorse_hkp_source_init (SeahorseHKPSource *hsrc)
{
}

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
}

/**
 * seahorse_hkp_source_new:
 * @uri: The server to connect to
 *
 * Creates a new key source for an HKP PGP server.
 *
 * Returns: A new HKP Key Source
 */
SeahorseHKPSource*
seahorse_hkp_source_new (const char *uri)
{
    g_return_val_if_fail (seahorse_hkp_is_valid_uri (uri), NULL);

    return g_object_new (SEAHORSE_TYPE_HKP_SOURCE, "uri", uri, NULL);
}

/**
 * seahorse_hkp_is_valid_uri:
 * @uri: The uri to check
 *
 * Returns: Whether the passed uri is valid for an HKP key source
 */
gboolean
seahorse_hkp_is_valid_uri (const gchar *uri)
{
    SoupURI *soup;
    gboolean ret = FALSE;

    g_return_val_if_fail (uri && uri[0], FALSE);

    /* Replace 'hkp' with 'http' at the beginning of the URI */
    if (strncasecmp (uri, "hkp:", 4) == 0) {
        g_autofree char *t = g_strdup_printf ("http:%s", uri + 4);
        soup = soup_uri_new (t);
    /* Not 'hkp', but maybe 'http' */
    } else if (strncasecmp (uri, "hkps:", 5) == 0) {
        g_autofree char *t = g_strdup_printf ("https:%s", uri + 5);
        soup = soup_uri_new (t);
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
