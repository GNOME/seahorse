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

G_DEFINE_QUARK (seahorse-hkp-error, seahorse_hkp_error);

struct _SeahorseHKPSource {
    SeahorseServerSource parent;
};

G_DEFINE_TYPE (SeahorseHKPSource, seahorse_hkp_source, SEAHORSE_TYPE_SERVER_SOURCE);

/* Helper method */
static GUri *
get_http_server_uri (SeahorseHKPSource *self,
                     const char *path,
                     GHashTable *query_hash)
{
    g_autofree char *uri = NULL;
    gboolean parsed;
    g_autofree char *scheme = NULL;
    g_autofree char *host = NULL;
    int port;
    g_autoptr(GError) error = NULL;
    g_autofree char *query = NULL;

    /* Take the (user-)configured URI */
    uri = seahorse_place_get_uri (SEAHORSE_PLACE (self));
    g_return_val_if_fail (uri != NULL, NULL);

    /* NOTE: seahorse_hkp_is_valid_uri already checked certain fields */
    parsed = g_uri_split (uri, G_URI_FLAGS_NONE,
                          &scheme, NULL, &host, &port, NULL, NULL, NULL,
                          &error);
    if (!parsed) {
        g_warning ("Bad URL for HKP server '%s': %s", uri, error->message);
        return NULL;
    }

    /* Fixup the scheme if needed */
    if (g_strcmp0 (scheme, "hkp") == 0) {
        g_free (scheme);
        scheme = g_strdup ("http");
    } else if (g_strcmp0 (scheme, "hkps") == 0) {
        g_free (scheme);
        scheme = g_strdup ("https");
    }

    /* We assume people won't use a query (and validate that earlier also) */
    if (query_hash != NULL)
        query = soup_form_encode_hash (query_hash);

    return g_uri_build (G_URI_FLAGS_NONE,
                        scheme, NULL, host, port, path, query, NULL);
}

static SoupSession *
create_hkp_soup_session (void)
{
    SoupSession *session;
#ifdef WITH_DEBUG
    g_autoptr(SoupLogger) logger = NULL;
    const char *env;
#endif

    session = soup_session_new ();

#ifdef WITH_DEBUG
    env = g_getenv ("G_MESSAGES_DEBUG");
    if (env && strstr (env, "seahorse")) {
        logger = soup_logger_new (SOUP_LOGGER_LOG_BODY);
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
dehtmlize (char *line)
{
    int parsedindex = 0;
    char *parsed = line;

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
    /* Use The OpenPGP HTTP Keyserver Protocol (HKP) to search and get keys
     * https://tools.ietf.org/html/draft-shaw-openpgp-hkp-00#section-5 */
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
            flags = 0;
            if (columns[6])
                flags |= parse_hkp_flags (columns[6]);

            /* create key */
            g_debug("HKP Parse: found new key");
            key = seahorse_pgp_key_new ();
            seahorse_pgp_key_set_item_flags (key, flags);

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

    if (key_total != 0 && key_total != key_count) {
        g_warning ("HKP Parse: Could only parse %d keys out of %d", key_count, key_total);
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
    const char *t;

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

static void
on_session_cancelled (GCancellable *cancellable,
                     void *user_data)
{
    SoupSession *session = user_data;
    soup_session_abort (session);
}

typedef struct {
    SeahorseHKPSource *source;
    SoupSession *session;
    SoupMessage *message;
    GString *response;
    int requests;
    GListStore *results;
} SearchClosure;

static void
source_search_free (void *data)
{
    SearchClosure *closure = data;
    g_clear_object (&closure->source);
    g_clear_object (&closure->message);
    if (closure->response != NULL)
        g_string_free (closure->response, TRUE);
    g_clear_object (&closure->session);
    g_clear_object (&closure->results);
    g_free (closure);
}

static void
on_search_message_complete (GObject *object,
                            GAsyncResult *result,
                            void *user_data)
{
    SoupSession *session = SOUP_SESSION (object);
    g_autoptr(GTask) task = G_TASK (user_data);
    SearchClosure *closure = g_task_get_task_data (task);
    GCancellable *cancellable = g_task_get_cancellable (task);
    g_autoptr(GBytes) response = NULL;
    g_autoptr(GError) error = NULL;
    g_autolist(SeahorsePgpKey) keys = NULL;

    seahorse_progress_end (cancellable, closure->message);

    response = soup_session_send_and_read_finish (session, result, &error);
    if (response == NULL) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    closure->response = g_string_new_len (g_bytes_get_data (response, NULL),
                                          g_bytes_get_size (response));
    keys = seahorse_hkp_parse_lookup_response (closure->response->str);
    for (GList *l = keys; l; l = g_list_next (l)) {
        g_object_set (l->data, "place", closure->source, NULL);
        g_list_store_append (closure->results, l->data);
    }
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
                                  GListStore           *results,
                                  GCancellable         *cancellable,
                                  GAsyncReadyCallback   callback,
                                  void                 *user_data)
{
    SeahorseHKPSource *self = SEAHORSE_HKP_SOURCE (source);
    SearchClosure *closure;
    g_autoptr(GTask) task = NULL;
    g_autoptr(GHashTable) form = NULL;
    g_autoptr(GUri) uri = NULL;
    g_autofree char *uri_str = NULL;

    task = g_task_new (source, cancellable, callback, user_data);
    closure = g_new0 (SearchClosure, 1);
    closure->source = g_object_ref (self);
    closure->session = create_hkp_soup_session ();
    closure->results = g_object_ref (results);
    g_task_set_task_data (task, closure, source_search_free);

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

    uri = get_http_server_uri (self, "/pks/lookup", form);
    g_return_if_fail (uri);

    closure->message = soup_message_new_from_uri ("GET", uri);

    seahorse_progress_prep_and_begin (cancellable, closure->message, NULL);

    uri_str = g_uri_to_string_partial (uri, G_URI_HIDE_PASSWORD);
    g_debug ("Sending HKP search query to '%s'", uri_str);

    soup_session_send_and_read_async (closure->session,
                                      closure->message,
                                      G_PRIORITY_DEFAULT,
                                      cancellable,
                                      on_search_message_complete,
                                      g_steal_pointer (&task));

    if (cancellable)
        g_cancellable_connect (cancellable,
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
    SoupSession *session;
    SoupMessage *message;
    int requests;
} ImportClosure;

static void
source_import_free (void *data)
{
    ImportClosure *closure = data;
    g_object_unref (closure->source);
    g_object_unref (closure->input);
    g_object_unref (closure->message);
    g_object_unref (closure->session);
    g_free (closure);
}

static void
on_import_message_complete (GObject *object,
                            GAsyncResult *result,
                            void *user_data)
{
    SoupSession *session = SOUP_SESSION (object);
    g_autoptr(GTask) task = G_TASK (user_data);
    ImportClosure *closure = g_task_get_task_data (task);
    GCancellable *cancellable = g_task_get_cancellable (task);
    g_autoptr(GBytes) response = NULL;
    g_autoptr(GString) response_str = NULL;
    g_autoptr(GError) error = NULL;
    g_autofree char *errmsg = NULL;

    g_assert (closure->requests > 0);
    seahorse_progress_end (cancellable, GUINT_TO_POINTER (closure->requests));
    closure->requests--;

    response = soup_session_send_and_read_finish (session, result, &error);
    if (!response) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    response_str = g_string_new_len (g_bytes_get_data (response, NULL),
                                     g_bytes_get_size (response));
    if ((errmsg = get_send_result (response_str->str)) != NULL) {
        g_task_return_new_error (task, HKP_ERROR_DOMAIN,
                                 soup_message_get_status (closure->message),
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
                                  void *user_data)
{
    SeahorseHKPSource *self = SEAHORSE_HKP_SOURCE (source);
    g_autoptr(GTask) task = NULL;
    ImportClosure *closure;
    g_autoptr(GPtrArray) keydata = NULL;
    g_autoptr(GUri) uri = NULL;

    task = g_task_new (source, cancellable, callback, user_data);
    closure = g_new0 (ImportClosure, 1);
    closure->input = g_object_ref (input);
    closure->source = g_object_ref (self);
    closure->session = create_hkp_soup_session ();
    g_task_set_task_data (task, closure, source_import_free);

    keydata = g_ptr_array_new_with_free_func (g_free);
    for (;;) {
        g_autoptr(GString) buf = g_string_sized_new (2048);
        guint len;

        len = seahorse_util_read_data_block (buf, input,
                                             "-----BEGIN PGP PUBLIC KEY BLOCK-----",
                                             "-----END PGP PUBLIC KEY BLOCK-----");
        if (len <= 0)
            break;

        g_ptr_array_add (keydata, g_string_free (g_steal_pointer (&buf), FALSE));
    }

    if (keydata->len == 0) {
        g_task_return_pointer (task, NULL, NULL);
        return;
    }

    /* Figure out the URI we're sending to */
    uri = get_http_server_uri (self, "/pks/add", NULL);
    g_return_if_fail (uri);

    for (unsigned int i = 0; i < keydata->len; i++) {
        const char *keytext = g_ptr_array_index (keydata, i);
        g_autofree char *key = NULL;
        g_autoptr(GBytes) bytes = NULL;

        closure->message = soup_message_new_from_uri ("POST", uri);

        key = soup_form_encode ("keytext", keytext, NULL);
        bytes = g_bytes_new_static (key, strlen (key));
        soup_message_set_request_body_from_bytes (closure->message,
                                                  "application/x-www-form-urlencoded",
                                                  bytes);

        soup_session_send_and_read_async (closure->session,
                                          closure->message,
                                          G_PRIORITY_DEFAULT,
                                          cancellable,
                                          on_import_message_complete,
                                          g_steal_pointer (&task));

        closure->requests++;
        seahorse_progress_prep_and_begin (cancellable, GUINT_TO_POINTER (closure->requests), NULL);
    }

    if (cancellable)
        g_cancellable_connect (cancellable,
                               G_CALLBACK (on_session_cancelled),
                               closure->session, NULL);
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
    SoupSession *session;
    SoupMessage *message;
    int requests;
} ExportClosure;

static void
export_closure_free (void *data)
{
    ExportClosure *closure = data;
    g_clear_object (&closure->source);
    if (closure->data)
        g_string_free (closure->data, TRUE);
    g_clear_object (&closure->message);
    g_clear_object (&closure->session);
    g_free (closure);
}

static void
on_export_message_complete (GObject *object,
                            GAsyncResult *result,
                            void *user_data)
{
    SoupSession *session = SOUP_SESSION (object);
    g_autoptr(GTask) task = G_TASK (user_data);
    ExportClosure *closure = g_task_get_task_data (task);
    GCancellable *cancellable = g_task_get_cancellable (task);
    g_autoptr(GBytes) response = NULL;
    g_autoptr(GError) error = NULL;
    const char *start, *end, *text;
    size_t len;

    seahorse_progress_end (cancellable, closure->message);

    response = soup_session_send_and_read_finish (session, result, &error);
    if (response == NULL) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    end = text = g_bytes_get_data (response, &len);
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
        g_autoptr(GBytes) result = NULL;

        result = g_string_free_to_bytes (g_steal_pointer (&closure->data));
        g_task_return_pointer (task,
                               g_steal_pointer (&result),
                               (GDestroyNotify) g_bytes_unref);
    }
}

static void
seahorse_hkp_source_export_async (SeahorseServerSource *source,
                                  const char **keyids,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  void *user_data)
{
    SeahorseHKPSource *self = SEAHORSE_HKP_SOURCE (source);
    ExportClosure *closure;
    g_autoptr(GTask) task = NULL;

    task = g_task_new (self, cancellable, callback, user_data);
    closure = g_new0 (ExportClosure, 1);
    closure->source = g_object_ref (self);
    closure->data = g_string_sized_new (1024);
    closure->session = create_hkp_soup_session ();
    g_task_set_task_data (task, closure, export_closure_free);

    if (!keyids || !keyids[0]) {
        g_task_return_pointer (task, NULL, NULL);
        return;
    }


    for (int i = 0; keyids[i] != NULL; i++) {
        const char *fpr = keyids[i];
        size_t len;
        g_autofree char *hexfpr = NULL;
        g_autoptr(GHashTable) form = NULL;
        g_autoptr(GUri) uri = NULL;

        form = g_hash_table_new (g_str_hash, g_str_equal);

        /* Get the key id and limit it to 16 characters */
        len = strlen (fpr);
        if (len > 16)
            fpr += (len - 16);

        /* prepend the hex prefix (0x) to make keyservers happy */
        hexfpr = g_strdup_printf ("0x%s", fpr);

        /* The get key URI */
        g_hash_table_insert (form, "op", "get");
        g_hash_table_insert (form, "search", (char *)hexfpr);

        uri = get_http_server_uri (self, "/pks/lookup", form);
        g_return_if_fail (uri);

        closure->message = soup_message_new_from_uri ("GET", uri);

        soup_session_send_and_read_async (closure->session,
                                          closure->message,
                                          G_PRIORITY_DEFAULT,
                                          cancellable,
                                          on_export_message_complete,
                                          g_steal_pointer (&task));

        closure->requests++;
        seahorse_progress_prep_and_begin (cancellable, closure->message, NULL);
    }

    if (cancellable)
        g_cancellable_connect (cancellable,
                               G_CALLBACK (on_session_cancelled),
                               closure->session, NULL);
}

static GBytes *
seahorse_hkp_source_export_finish (SeahorseServerSource *source,
                                   GAsyncResult *result,
                                   GError **error)
{
    g_return_val_if_fail (g_task_is_valid (result, source), NULL);

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
seahorse_hkp_is_valid_uri (const char *uri)
{
    g_autoptr(GUri) parsed = NULL;
    const char *scheme;

    g_return_val_if_fail (uri && uri[0], FALSE);

    parsed = g_uri_parse (uri, G_URI_FLAGS_NONE, NULL);
    if (!parsed)
        return FALSE;

    scheme = g_uri_get_scheme (parsed);
    if (g_strcmp0 (scheme, "hkp") && g_strcmp0 (scheme, "hkps") &&
        g_strcmp0 (scheme, "http") && g_strcmp0 (scheme, "https"))
        return FALSE;

    if (g_uri_get_host (parsed) == NULL)
        return FALSE;
    if (g_uri_get_userinfo (parsed) != NULL && g_uri_get_userinfo (parsed)[0] != '\0')
        return FALSE;
    if (g_uri_get_path (parsed) != NULL && g_uri_get_path (parsed)[0] != '\0')
        return FALSE;
    if (g_uri_get_query (parsed) != NULL)
        return FALSE;

    return TRUE;
}

#endif /* WITH_HKP */
