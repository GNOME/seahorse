/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2017 Niels De Graef
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

namespace Seahorse {

/**
 * Implements the HKP (HTTP Keyserver protocol) Source object
 *
 * See: http://tools.ietf.org/html/draft-shaw-openpgp-hkp-00
 */
public class HkpSource : ServerSource {

    public const string PGP_KEY_BEGIN = "-----BEGIN PGP PUBLIC KEY BLOCK-----";
    public const string PGP_KEY_END = "-----END PGP PUBLIC KEY BLOCK-----";

    public const int DEFAULT_HKP_PORT = 11371;

#define SOUP_MESSAGE_IS_ERROR(msg) \
        (SOUP_STATUS_IS_TRANSPORT_ERROR((msg).status_code) || \
         SOUP_STATUS_IS_CLIENT_ERROR((msg).status_code) || \
         SOUP_STATUS_IS_SERVER_ERROR((msg).status_code))

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

    static construct {
        if (Config.WITH_HKP)
            Seahorse.Server.register_type("hkp", _("HTTP Key Server"), is_valid_uri);
    }

    /**
     * Creates a new key source for an HKP PGP server.
     *
     * @param uri The server to connect to
     */
    public HKPSource(string uri, string host) {
        GLib.Object(key_server: host);
    }

    /**
     * @param uri The uri to check
     *
     * @return Whether the passed uri is valid for an HKP key source
     */
    public private bool is_valid_uri(string? uri) {
        if(uri == null && uri == "")
            return false;

        gboolean ret = false;
        gchar *t;

        Soup.URI? soup;
        if (uri.has_prefix("hkp:")) {
            // Replace 'hkp' with 'http' at the beginning of the URI
            soup = "http:%s".printf(uri.substring(4));
        } else {
            soup = new Soup.URI(uri);
        }


        if (soup == null)
            return fase;

        /* Must be http or https, have a host. No querystring, user, path, passwd etc... */
        return (soup.scheme == "http" || soup.scheme == "https")
            && soup.host != null && soup.host != ""
            && (soup.path != null ? soup.path : "/") == "/"
            && soup.user == null && soup.password == null && soup.query == null && soup.fragment == null
    }

    /**
     * Returns a Soup uri with server, port and paths
     *
     * @param src The SeahorseSource to use as server for the uri
     * @param path The path to add to the SOUP uri
     */
    private Soup.URI? get_http_server_uri(string path) {
        if (this.server == null)
            return null;

        Soup.URI uri = new Soup.URI(null);
        uri.set_scheme("http");

        // If it already has a port then use that
        string server_port = this.server.split(":", 2);
        if (server_port.length == 2) {
            uri.set_host(server_port[0]);
            uri.set_port(int.parse(server_port[1]));
        } else {
            uri.set_host(server_port[0]);
            uri.set_port(DEFAULT_HKP_PORT);
        }

        uri.set_path(path);

        return uri;
    }

    private Soup.Session create_hkp_soup_session () {
        Soup.Session session = new Soup.SessionAsync.with_options(SOUP_SESSION_ADD_FEATURE_BY_TYPE,
                                                                  SOUP_TYPE_PROXY_RESOLVER_DEFAULT);

        if (Config.WITH_DEBUG) {
            string? env = Environment.get_variable("G_MESSAGES_DEBUG");
            if (env != null && ("seahorse" in env)) {
                Soup.Logger logger = new Soup.Logger(SOUP_LOGGER_LOG_BODY, -1);
                session.add_feature(logger);
            }
        }

        return session;
    }

    /**
     * Remove anything <between brackets> and de-urlencode in place.  Note
     * that this requires all brackets to be closed on the same line.  It
     * also means that the result is never larger than the input.
     *
     * @param line The line to remove the HTML from
     */
    private string? dehtmlize(string? line) {
        if (line == null)
            return null;

        StringBuilder parsed = new StringBuilder.sized(line.length);
        int i = 0;
        while (i < line.length) {
            if (line[i] == '<') {
                while (line[i] != '>' && line[i] != '\0')
                    i++;
                if (i < line.length)
                    i++;
                continue;
            }

            if (line[i] == '&') {
                if ((i + 4) <= line.length) {
                    if (line[i:i+4].down() == "&lt;")
                        parsed.append_c('<');
                    else if (line[i:i+4].down() == "&gt;")
                        parsed.append_c('>');

                    i += 4;
                    continue;
                }

                if ((i + 5) <= line.length && line[i:i+5].down() == "&amp;") {
                    parsed.append_c('&');
                    i += 5;
                    continue;
                }
            }
            // Fall through
            parsed.append_c(line[i]);
            i++;
        }

        // Chop off any trailing whitespace.  Note that the HKP servers have
        // \r\n as line endings, and the NAI HKP servers have just \n.
        if(parsedindex > 0) {
            parsedindex--;
            while (g_ascii_isspace(((unsigned char*)parsed)[parsedindex])) {
                parsed[parsedindex] = '\0';
                if(parsedindex == 0)
                    break;
                parsedindex--;
            }
        }

        return parsed.str;
    }

    /**
     * @text: The date string to parse, YYYY-MM-DD
     *
     * Returns: 0 on error or the timestamp
     */
    static uint parse_hkp_date(string text) {
        if (text.length != 10 || text[4] != '-' || text[7] != '-')
            return 0;

        // YYYY-MM-DD
        int year, month, day;
        text.scanf("%4d-%2d-%2d", out year, month, day);

        // some basic checks
        if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31)
            return 0;

        struct tm tmbuf;
        time_t stamp;

        memset (&tmbuf, 0, sizeof tmbuf);
        tmbuf.tm_mday = day;
        tmbuf.tm_mon = month - 1;
        tmbuf.tm_year = year - 1900;
        tmbuf.tm_isdst = -1;

        stamp = mktime (&tmbuf);
        return stamp == (time_t)-1 ? 0 : stamp;
    }

    static string get_fingerprint_string(string line) {
        if (line == null)
            return null;

        string chugged = line.chug();
        string FINGERPRINT_KEY = "fingerprint=";
        if (chugged.down().has_prefix(FINGERPRINT_KEY))
            return chugged.substring(FINGERPRINT_KEY.length);

        return null;
    }

    /**
     * Extracts the key data from the HKP server response
     *
     * @param response The HKP server response to parse
     *
     * @return A List of keys
     */
    private List<Pgp.Key> parse_hkp_index (string response) {
        // Luckily enough, both the HKP server and NAI HKP interface to their
        // LDAP server are close enough in output so the same function can
        // parse them both.

        /* pub  2048/<a href="/pks/lookup?op=get&search=0x3CB3B415">3CB3B415</a> 1998/04/03 David M. Shaw &lt;<a href="/pks/lookup?op=get&search=0x3CB3B415">dshaw@jabberwocky.com</a>&gt; */

        SeahorsePgpKey *key = null;
        SeahorsePgpSubkey *subkey_with_id = null;
        GList *keys = null;
        GList *subkeys = null;
        GList *uids = null;

        string[] lines = response.split("\n");
        foreach (string line in lines) {
            line = dehtmlize(line);

            debug("%s", line);

            // Start a new key
            if (line.ascii_ncasecmp("pub ", 4) == 0) {
                string t = line.substring(4).chug();

                string[] v = t.split_set(" ", 3);
                if (v.length != 3) {
                    message("Invalid key line from server: %s", line);
                    return null;
                }

                // Cut the length and fingerprint
                string fpr = strchr (v[0], '/');
                if (fpr == null) {
                    message("couldn't find key fingerprint in line from server: %s", line);
                    fpr = "";
                } else {
                    *(fpr++) = 0;
                }

                // Check out the key type
                string algo;
                switch ((v[0][strlen(v[0]) - 1]).toupper()) {
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

                // Format the date for our parse function
                g_strdelimit (v[1], "/", '-');

                // Cleanup the UID
                g_strstrip (v[2]);

                bool has_uid = true;
                Seahorse.Flags flags = Seahorse.Flags.EXPORTABLE;

                if (v[2].ascii_strcasecmp("*** KEY REVOKED ***") == 0) {
                    flags |= Seahorse.Flags.REVOKED;
                    has_uid = false;
                }

                if (key) {
                    key.uids = uids;
                    key.subkeys = subkeys;
                    key.realize();
                    uids = subkeys = null;
                    subkey_with_id = null;
                    key = null;
                }

                key = new Pgp.Key();
                keys.prepend(key);
                key.flags = flags;

                // Add all the info to the key
                Pgp.Subkey subkey = new Pgp.SubKey();
                subkey.keyid = fpr;
                subkey_with_id = subkey;

                subkey.fingerprint = seahorse_pgp_subkey_calc_fingerprint (fpr);
                subkey.flags = flags;
                subkey.length = long.parse(v[0]);
                subkey.created = parse_hkp_date(v[1]);
                subkey.algorithm = algo;
                subkeys.prepend(subkey);

                // And the UID if one was found
                if (has_uid) {
                    Pgp.Uid uid = new Pgp.Uid(key, v[2]);
                    uids.prepend(uid);
                }

            // A UID for the key
            } else if (key && g_ascii_strncasecmp (line, "    ", 4) == 0) {
                Pgp.Uid uid = new Pgp.Uid(key, line.strip());
                uids.prepend(uid);

            // Signatures
            } else if (key && g_ascii_strncasecmp (line, "sig ", 4) == 0) {
                // TODO: Implement signatures

            } else if (key && subkey_with_id) {
                string fingerprint_str = get_fingerprint_string (line);

                if (fingerprint_str != null) {
                    string pretty_fingerprint = seahorse_pgp_subkey_calc_fingerprint (fingerprint_str);

                    // FIXME: we don't check that the fingerprint actually matches the key's ID.
                    // We also don't validate the fingerprint at all; the keyserver may have returned
                    // some garbage and we don't notice.

                    if (pretty_fingerprint != "")
                        subkey_with_id.fingerprint = pretty_fingerprint;
                }
            }
        }

        if (key != null) {
            key.uids = uids.reverse();
            key.subkeys = subkeys.reverse();
            key.realize();
        }

        return keys;
    }

    /**
     * Parses the response and extracts an error message
     *
     * @param response The server response
     *
     * Returns null if there was no error. The error message else
     **/
    private string get_send_result (string response) {
        if (response == null || response == "")
            return "";

        string[] lines = response.split("\n");
        string? last = null;
        foreach (string line in lines) {
            string l = dehtmlize(line).strip();

            if (l == "")
                continue;

            // Look for the word 'error'
            if ("error" in l.down())
                last = l;
        }

        // Use last line as the message
        return last;
    }

    /**
     * Finds a key in a string
     *
     * @param text The ASCII armoured key
     * @param start Returned, start of the key
     * @param end Returned, end of the key
     *
     * @return false if no key is contained, true otherwise
     */
    private bool detect_key(string text, out string? start = null, out string? end = null) {
        // Find the first of the headers
        int start_pos = text.index_of(PGP_KEY_BEGIN);
        if (start_pos == -1)
            return false;

        if (start != null)
            start = text[start_pos:(start_pos+PGP_KEY_BEGIN.length)];

        /* Find the end of that block */
        int end_pos = text.index_of(PGP_KEY_END);
        if (end_pos == -1)
            return false;

        if (end != null)
            end = text[end_pos:(end_pos+PGP_KEY_END.length)];

        return true;
    }

    /* -----------------------------------------------------------------------------
     *  SEAHORSE HKP SOURCE
     */

    private bool hkp_message_propagate_error(Soup.Message *message) throws GLib.Error {
        gchar *text, *server;

        if (!SOUP_MESSAGE_IS_ERROR (message))
            return false;

        if (message.status_code == Soup.STATUS_CANCELLED) {
            g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                                 _("The operation was cancelled"));
            return true;
        }

        g_object_get (self, "key-server", &server, null);

        // Make the body lower case, and no tags
        text = g_strndup (message.response_body.data, message.response_body.length);
        if (text != null) {
            dehtmlize (text);
            seahorse_util_string_lower (text);
        }

        if (text && strstr (text, "no keys")) {
            return false; /* not found is not an error */
        } else if (text && strstr (text, "too many")) {
            g_set_error (error, HKP_ERROR_DOMAIN, 0,
                         _("Search was not specific enough. Server “%s” found too many keys."), server);
        } else {
            g_set_error (error, HKP_ERROR_DOMAIN, message.status_code,
                         _("Couldn’t communicate with server “%s”: %s"),
                         server, message.reason_phrase);
        }

        return true;
    }

    private void on_session_cancelled(Cancellable cancellable, Soup.Session session) {
        session.abort();
    }

    public async void search_async(string match, Gcr.SimpleCollection results,
                                   Cancellable cancellable) throws GLib.Error {
        Soup.Session session = create_hkp_soup_session ();

        Soup.URI? uri = get_http_server_uri("/pks/lookup");
        if (uri == null)
            return false;

        HashTable<string, string> form = new HashTable<string, string>();
        form.insert("op", "index");

        if (is_hex_keyid (match)) {
            gchar hexfpr[11];
            strncpy (hexfpr, "0x", 3);
            strncpy (hexfpr + 2, match, 9);
            form.insert("search", hexfpr);
        } else {
            form.insert("search", match);
        }

        form.insert("fingerprint", "on");

        uri.set_query_from_form(form);

        Soup.Message message = new Soup.Message("GET", uri);
        session.queue_message(message, on_search_message_complete);

        Seahorse.Progress.prep_and_begin (cancellable, message, null);

        if (cancellable != null)
            closure.cancelled_sig = cancellable.connect(on_session_cancelled, closure.session);
    }

    private void on_search_message_complete(Soup.Session session, Soup.Message message) {
        GError *error = null;
        Seahorse.Progress.end(cancellable, message);

        if (hkp_message_propagate_error(message, &error)) {
            g_simple_async_result_take_error (res, error);

        } else {
            List<Pgp.Key> keys = parse_hkp_index (message.response_body.data);
            foreach (Pgp.Key key in keys) {
                key.place = this;
                results.add(key);
            }
        }
    }

    private bool is_hex_keyid(string? text) {
        if (text == null || text.length != 8)
            return false;

        foreach (uint8 byt in text.data) {
            if (!((byt >= 0x30 && byt <= 0x39) ||
                  (byt >= 0x41 && byt <= 0x46) ||
                  (byt >= 0x61 && byt <= 0x66)))
                return false;
        }

        return true;
    }

    /**
     * Imports a list of keys from the input stream to the keyserver
     *
     * @param input The input stream to add
     **/
    public List? import_async(InputStream input, Cancellable cancellable) throws GLib.Error {
        Soup.Session session = create_hkp_soup_session ();

        List<string> keydata = new List<string>();
        for (;;) {
            StringBuilder buf = new StringBuilder(2048);
            uint len = Seahorse.Util.read_data_block(buf, input, PGP_KEY_BEGIN, PGP_KEY_END);

            if (len > 0)
                keydata.append(buf.str);
            else
                break;
        }

        if (keydata.length == 0)
            return null;

        // Figure out the URI we're sending to
        Soup.URI? uri = get_http_server_uri("/pks/add");
        if (uri == null)
            return null;

        // New operation and away we go
        int requests = 0;
        foreach (string keytext) {
            assert(keytext != null);

            HashTable<string, string> form = new HashTable<string, string>();

            form.insert("keytext", keytext);
            string key = form.encode_urlencoded();

            Soup.Message message = new Soup.Message("POST", uri);
            message.set_request("application/x-www-form-urlencoded", Soup.MemoryUse.TAKE, key);

            session.queue_message(message, on_import_message_complete);

            requests++;
            Seahorse.Progress.prep_and_begin(cancellable, requests, null);
        }

        if (cancellable)
            closure.cancelled_sig = cancellable.connect(on_session_cancelled);

        // We don't know which keys got imported, so just return null
        return null;
    }

    private void on_import_message_complete (Soup.Session session, Soup.Message message) {
        GError *error = null;
        gchar *errmsg;

        assert (closure.requests > 0);
        seahorse_progress_end (closure.cancellable, GUINT_TO_POINTER (closure.requests));
        closure.requests--;

        if (hkp_message_propagate_error (closure.source, message, &error)) {
            g_simple_async_result_take_error (res, error);

        } else if ((errmsg = get_send_result (message.response_body.data)) != null) {
            g_set_error (&error, HKP_ERROR_DOMAIN, message.status_code, "%s", errmsg);
            g_simple_async_result_take_error (res, error);

        // A successful status from the server is all we want in this case
        } else {
            if (closure.requests == 0)
                g_simple_async_result_complete_in_idle (res);
        }
    }

    /**
     * Gets data from the keyserver, writes it to the output stream
     *
     * @param keyids the keyids to look up
     *
     * @return The output stream the data will end in
     */
    private void* export_async(string[] keyids, Cancellable cancellable) throws GLib.Error {
        if (keyids == null || keyids.length = 0)
            return null;

        StringBuilder data = new StringBuilder.sized(1024);
        Soup.Session session = create_hkp_soup_session ();

        Soup.URI? uri = get_http_server_uri (self, "/pks/lookup");
        if (uri == null)
            return null;

        // prepend the hex prefix (0x) to make keyservers happy
        strncpy (hexfpr, "0x", 3);

        int requests = 0;
        foreach (string fpr in keyids) {
            HashTable<string, string> form = new HashTable<string, string>();

            // Get the key id and limit it to 8 characters
            if (fpr.length > 8)
                fpr += (len - 8);

            gchar hexfpr[11];
            strncpy (hexfpr + 2, fpr, 9);

            // The get key URI
            form.insert("op", "get");
            form.insert("search", hexfpr);
            uri.set_query_from_form(form);

            Soup.Message message = new Soup.Message.from_uri("GET", uri);
            session.queue_message(message, on_export_message_complete);

            requests++;
            Seahorse.Progress.prep_and_begin(cancellable, message, null);
        }

        if (cancellable)
            closure.cancelled_sig = cancellable.connect(on_session_cancelled, closure.session, null);

        // XXX export_async_finish(size_t size);
        if (size == null)
            return null;

        *size = closure.data.len;
        return data.str;
    }

    private void on_export_message_complete(Soup.Session session, Soup.Message message) {
        GError *error = null;

        seahorse_progress_end (closure.cancellable, message);

        if (hkp_message_propagate_error (closure.source, message, &error)) {
            g_simple_async_result_take_error (res, error);
        } else {
            string end, text;
            end = text = message.response_body.data;
            uint len = message.response_body.length;

            for (;;) {
                len -= end - text;
                text = end;

                string start;
                if (!detect_key (text, len, &start, &end))
                    break;

                data.append_len(start, end - start);
                data.append_c('\n');
            }
        }

        assert (closure.requests > 0);
        closure.requests--;

        if (closure.requests == 0)
            g_simple_async_result_complete_in_idle (res);
    }
}

}
