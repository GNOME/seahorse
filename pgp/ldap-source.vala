/*
 * Seahorse
 *
 * Copyright (C) 2004 - 2006 Stefan Walter
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

public class Seahorse.LdapSource : ServerSource {
    /**
     * Amount of keys to load in a batch
     */
    public const int DEFAULT_LOAD_BATCH = 30;

    static construct {
        if (Config.WITH_LDAP)
            Seahorse.Server.register_type("ldap", _("LDAP Key Server"), seahorse_ldap_is_valid_uri);
    }

    /**
     * Creates a new key source for an LDAP PGP server.
     *
     * @param uri The server to connect to
     */
    public LDAPSource(string uri, string host) {
        GLib.Object(key_server: host, uri: uri);
    }

    /**
     * @param uri The uri to check
     *
     * Returns: Whether the passed uri is valid for an ldap key source
     */
    public static bool is_valid_uri (string uri) {
        if (uri == null || uri == "")
            return false;

        LDAPURLDesc *url;
        int r;

        r = ldap_url_parse (uri, &url);
        if (r == LDAP_URL_SUCCESS) {

            /* Some checks to make sure it's a simple URI */
            if (!(url.lud_host && url.lud_host[0]) ||
                (url.lud_dn && url.lud_dn[0]) ||
                (url.lud_attrs || url.lud_attrs))
                r = LDAP_URL_ERR_PARAM;

            ldap_free_urldesc (url);
        }

        return r == LDAP_URL_SUCCESS;
    }

    /* -----------------------------------------------------------------------------
     * SERVER INFO
     */

    typedef struct _LDAPServerInfo {
        string base_dn;             /* The base dn where PGP keys are found */
        string key_attr;            /* The attribute of PGP key data */
        uint version;              /* The version of the PGP server software */
    } LDAPServerInfo;

    static LDAPServerInfo* get_ldap_server_info(bool force) {
        LDAPServerInfo? sinfo = get_data("server-info");

        // When we're asked to force getting the data, we fill in some defaults
        if (sinfo == null && force) {
            sinfo = g_new0 (LDAPServerInfo, 1);
            sinfo.base_dn = "OU=ACTIVE,O=PGP KEYSPACE,C=US";
            sinfo.key_attr = "pgpKey";
            sinfo.version = 0;
            set_data_full("server-info", sinfo, free_ldap_server_info);
        }

        return sinfo;
    }


    /* -----------------------------------------------------------------------------
     *  LDAP HELPERS
     */

#define LDAP_ERROR_DOMAIN (get_ldap_error_domain())

    static string[] get_ldap_values(LDAP *ld, LDAPMessage *entry, string attribute) {
        struct berval **bv = ldap_get_values_len (ld, entry, attribute);
        if (!bv)
            return null;

        string[] array = new string[];
        int num = ldap_count_values_len(bv);
        for(int i = 0; i < num; i++) {
            string val = g_strndup (bv[i].bv_val, bv[i].bv_len);
            array += val;
        }

        return array;
    }

#if WITH_DEBUG
    static void dump_ldap_entry (LDAP *ld, LDAPMessage *res) {
        string dn = ldap_get_dn (ld, res);
        debug ("dn: %s\n", dn);

        BerElement *pos;
        for (string? t = ldap_first_attribute (ld, res, &pos); t != null; t = ldap_next_attribute (ld, res, pos)) {
            string[] values = get_ldap_values (ld, res, t);
            for (string val in values)
                g_debug ("%s: %s\n", t, v);

            g_strfreev (values);
            ldap_memfree (t);
        }

        ber_free (pos, 0);
    }
#endif /* WITH_DEBUG */

    static GQuark get_ldap_error_domain() {
        static GQuark q = 0;
        if(q == 0)
            q = g_quark_from_static_string ("seahorse-ldap-error");
        return q;
    }

    static string? get_string_attribute (LDAP *ld, LDAPMessage *res, string attribute) {
        string[] vals = get_ldap_values (ld, res, attribute);
        return (vals != null) vals[0] : null;
    }

    static bool get_boolean_attribute (LDAP* ld, LDAPMessage *res, string attribute) {
        string[] vals = get_ldap_values (ld, res, attribute);
        if (!vals)
            return false;
        bool b = vals[0] && atoi (vals[0]) == 1;
        return b;
    }

    static long int get_int_attribute (LDAP* ld, LDAPMessage *res, string attribute) {
        string[] vals = get_ldap_values (ld, res, attribute);
        if (!vals)
            return 0;
        long int d = vals[0] ? atoi (vals[0]) : 0;
        return d;
    }

    static long int get_date_attribute (LDAP* ld, LDAPMessage *res, string attribute) {
        struct tm t;
        long int d = 0;

        string[] vals = get_ldap_values (ld, res, attribute);
        if (!vals)
            return 0;

        if (vals[0]) {
            memset(&t, 0, sizeof (t));

            /* YYYYMMDDHHmmssZ */
            sscanf(vals[0], "%4d%2d%2d%2d%2d%2d",
                &t.tm_year, &t.tm_mon, &t.tm_mday,
                &t.tm_hour, &t.tm_min, &t.tm_sec);

            t.tm_year -= 1900;
            t.tm_isdst = -1;
            t.tm_mon--;

            d = mktime (&t);
        }

        return d;
    }

    static string? get_algo_attribute(LDAP* ld, LDAPMessage *res, string attribute) {
        string[] vals = get_ldap_values (ld, res, attribute);
        if (vals == null || vals.length < 1 || vals[0] == null)
            return null;

        if (g_ascii_strcasecmp (vals[0], "DH/DSS") == 0 ||
            g_ascii_strcasecmp (vals[0], "Elg") == 0 ||
            g_ascii_strcasecmp (vals[0], "Elgamal") == 0 ||
            g_ascii_strcasecmp (vals[0], "DSS/DH") == 0)
            return "Elgamal";
        if (g_ascii_strcasecmp (vals[0], "RSA") == 0)
            return "RSA";
        if (g_ascii_strcasecmp (vals[0], "DSA") == 0)
            return "DSA";

        return null;
    }

    /*
     * Escapes a value so it's safe to use in an LDAP filter. Also trims
     * any spaces which cause problems with some LDAP servers.
     */
    static string escape_ldap_value(string v) {
        assert(v != null);
        StringBuilder result = g_string_sized_new (strlen(v));

        for (int i = 0; i < v.length; v++) {
            switch(v[i]) {
            case '#': case ',': case '+': case '\\':
            case '/': case '\"': case '<': case '>': case ';':
                result.append_c('\\');
                result.append_c(v[i]);
                continue;
            };

            if(v[i] < 32 || v[i] > 126) {
                resulit.append_printf("\\%02X", *v);
                continue;
            }

            result.append_c(v[i]);
        }

        return result.str.strip();
    }

    typedef bool (*SeahorseLdapCallback)   (LDAPMessage *result, gpointer user_data);

    typedef struct {
        GSource source;
        LDAP *ldap;
        int ldap_op;
        GCancellable *cancellable;
        bool cancelled;
        gint cancelled_sig;
    } SeahorseLdapGSource;

    static bool seahorse_ldap_gsource_prepare (GSource *gsource, int *timeout) {
        SeahorseLdapGSource *ldap_gsource = (SeahorseLdapGSource *)gsource;

        if (ldap_gsource.cancelled)
            return true;

        /* No other way, but to poll */
        *timeout = 50;
        return false;
    }

    static bool seahorse_ldap_gsource_check (GSource *gsource) {
        return true;
    }

    static bool seahorse_ldap_gsource_dispatch(GSource *gsource, GSourceFunc callback, gpointer user_data) {
        SeahorseLdapGSource *ldap_gsource = (SeahorseLdapGSource *)gsource;

        if (ldap_gsource.cancelled) {
            ((SeahorseLdapCallback)callback) (null, user_data);
            return false;
        }

        for (int i = 0; i < DEFAULT_LOAD_BATCH; i++) {

            /* This effects a poll */
            timeout.tv_sec = 0;
            timeout.tv_usec = 0;

            struct timeval timeout; LDAPMessage *result;
            int rc = ldap_result (ldap_gsource.ldap, ldap_gsource.ldap_op,
                              0, &timeout, &result);
            if (rc == -1) {
                g_warning ("ldap_result failed with rc = %d, errno = %s",
                           rc, g_strerror (errno));
                return false;

            /* Timeout */
            } else if (rc == 0) {
                return true;
            }

            bool ret = ((SeahorseLdapCallback)callback) (result, user_data);
            ldap_msgfree (result);

            if (!ret)
                return false;
        }

        return true;
    }

    static void seahorse_ldap_gsource_finalize (GSource *gsource) {
        SeahorseLdapGSource *ldap_gsource = (SeahorseLdapGSource *)gsource;
        g_cancellable_disconnect (ldap_gsource.cancellable,
                                  ldap_gsource.cancelled_sig);
        g_clear_object (&ldap_gsource.cancellable);
    }

    static GSourceFuncs seahorse_ldap_gsource_funcs = {
        seahorse_ldap_gsource_prepare,
        seahorse_ldap_gsource_check,
        seahorse_ldap_gsource_dispatch,
        seahorse_ldap_gsource_finalize,
    };

    static void on_ldap_gsource_cancelled (GCancellable *cancellable, gpointer user_data) {
        SeahorseLdapGSource *ldap_gsource = user_data;
        ldap_gsource.cancelled = true;
    }

    static GSource * seahorse_ldap_gsource_new (LDAP *ldap, int ldap_op, GCancellable *cancellable) {
        SeahorseLdapGSource *ldap_gsource;

        GSource *source = g_source_new (&seahorse_ldap_gsource_funcs,
                                sizeof (SeahorseLdapGSource));

        ldap_gsource = (SeahorseLdapGSource *)gsource;
        ldap_gsource.ldap = ldap;
        ldap_gsource.ldap_op = ldap_op;

        if (cancellable) {
            ldap_gsource.cancellable = g_object_ref (cancellable);
            ldap_gsource.cancelled_sig = g_cancellable_connect (cancellable,
                                                                 G_CALLBACK (on_ldap_gsource_cancelled),
                                                                 ldap_gsource, null);
        }

        return gsource;
    }

    static bool seahorse_ldap_source_propagate_error(int rc, GError **error) {
        if (rc == LDAP_SUCCESS)
            return false;

        g_set_error(error, LDAP_ERROR_DOMAIN, rc, _("Couldn’t communicate with %s: %s"),
                     this.key_server, ldap_err2string (rc));

        return true;
    }

    typedef struct {
        GCancellable *cancellable;
        gchar *filter;
        LDAP *ldap;
        GcrSimpleCollection *results;
    } source_search_closure;

    static void source_search_free (gpointer data) {
        source_search_closure *closure = data;
        g_clear_object (&closure.cancellable);
        g_clear_object (&closure.results);
        g_free (closure.filter);
        if (closure.ldap)
            ldap_unbind_ext (closure.ldap, null, null);
        g_free (closure);
    }

    static string PGP_ATTRIBUTES[] = {
        "pgpcertid",
        "pgpuserid",
        "pgprevoked",
        "pgpdisabled",
        "pgpkeycreatetime",
        "pgpkeyexpiretime"
        "pgpkeysize",
        "pgpkeytype",
        null
    };

    public async bool search_async(string match, Gcr.SimpleCollection results, Cancellable cancellable) throws GLib.Error {
        source_search_closure *closure;

        string text = escape_ldap_value (match);
        closure.filter = "(pgpuserid=*%s*)".printf(text);

        Seahorse.Progress.prep_and_begin(closure.cancellable, res, null);

        connect_async(cancellable, on_search_connect_completed, g_object_ref (res));
    }

    static void on_search_connect_completed() {
        source_search_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
        GError *error = null;

        closure.ldap = connect_finish(result, &error);
        if (error != null) {
            g_simple_async_result_take_error (res, error);
            g_simple_async_result_complete (res);
            g_object_unref (res);
            return;
        }

        LDAPServerInfo sinfo = get_ldap_server_info(true);

        debug("Searching Server ... base: %s, filter: %s",
                 sinfo.base_dn, closure.filter);

        int ldap_op;
        int rc = ldap_search_ext (closure.ldap, sinfo.base_dn, LDAP_SCOPE_SUBTREE,
                              closure.filter, (char **)PGP_ATTRIBUTES, 0,
                              null, null, null, 0, &ldap_op);

        if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
            g_simple_async_result_take_error (res, error);
            g_simple_async_result_complete (res);

        } else {
            GSource *gsource = seahorse_ldap_gsource_new (closure.ldap, ldap_op,
                                                 closure.cancellable);
            g_source_set_callback (gsource, on_search_search_completed, g_object_ref (res), g_object_unref);
            g_source_attach (gsource, g_main_context_default ());
            g_source_unref (gsource);
        }
    }

    static bool on_search_search_completed(LDAPMessage *result) {
        if (result.type != LDAP_RES_SEARCH_ENTRY && result.type != LDAP_RES_SEARCH_RESULT)
            return false;

        source_search_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
        GError *error = null;

        /* An LDAP entry */
        if (result.type == LDAP_RES_SEARCH_ENTRY) {
            debug ("Retrieved Key Entry");
#ifdef WITH_DEBUG
            dump_ldap_entry (closure.ldap, result);
#endif

            search_parse_key_from_ldap_entry(closure.results, closure.ldap, result);
            return true; /* keep calling this callback */

        /* All entries done */
        } else {
            int code; char *message;
            int rc = ldap_parse_result (closure.ldap, result, &code, null, &message, null, null, 0);
            g_return_val_if_fail (rc == LDAP_SUCCESS, false);

            /* Error codes that we ignore */
            switch (code) {
            case LDAP_SIZELIMIT_EXCEEDED:
                code = LDAP_SUCCESS;
                break;
            };

            /* Failure */
            if (code != LDAP_SUCCESS)
                g_simple_async_result_set_error (res, LDAP_ERROR_DOMAIN, code, "%s", message);
            else if (seahorse_ldap_source_propagate_error (self, code, &error))
                g_simple_async_result_take_error (res, error);

            Seahorse.Progress.end(closure.cancellable, res);
            g_simple_async_result_complete (res);
            return false;
        }
    }

    /* Add a key to the key source from an LDAP entry */
    static void search_parse_key_from_ldap_entry(Gcr.SimpleCollection results, LDAP *ldap, LDAPMessage *res) {
        g_return_if_fail (ldap_msgtype (res) == LDAP_RES_SEARCH_ENTRY);

        string fpr = get_string_attribute (ldap, res, "pgpcertid");
        string uidstr = get_string_attribute (ldap, res, "pgpuserid");
        bool revoked = get_boolean_attribute (ldap, res, "pgprevoked");
        bool disabled = get_boolean_attribute (ldap, res, "pgpdisabled");
        long int timestamp = get_date_attribute (ldap, res, "pgpkeycreatetime");
        long int expires = get_date_attribute (ldap, res, "pgpkeyexpiretime");
        string algo = get_algo_attribute (ldap, res, "pgpkeytype");
        int length = get_int_attribute (ldap, res, "pgpkeysize");

        if (fpr && uidstr) {
            SeahorsePgpUid *uid;
            GList *list;

            /* Build up a subkey */
            Pgp.SubKey subkey = new Pgp.SubKey();
            subkey.keyid = fpr;
            subkey.fingerprint = Pgp.SubKey.calc_fingerprint(fpr);
            subkey.created = timestamp;
            subkey.expires = expires;
            subkey.algorithm = algo;
            subkey.length = length;

            Seahorse.Flags flags = Seahorse.Flags.EXPORTABLE;
            if (revoked)
                flags |= Seahorse.Flags.REVOKED;
            if (disabled)
                flags |= Seahorse.Flags.DISABLED;
            subkey.flags = flags;

            Pgp.SubKey key = new Pgp.SubKey();

            /* Build up a uid */
            uid = new Pgp.Uid(key, uidstr);
            if (revoked)
                uid.set_validity(Seahorse.Validity.REVOKED);

            /* Now build them into a key */
            list = g_list_prepend (null, uid);
            key.uids = list;
            list = g_list_prepend (null, subkey);
            key.subkeys = list;
            g_object_set (key,
                          "object-flags", flags,
                          "place", self,
                          null);

            key.realize();
            results.add(key);
        }
    }

    static bool search_finish(GAsyncResult *result, GError **error) {
        if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
            return false;

        return true;
    }

    typedef struct {
        GPtrArray *keydata;
        gint current_index;
        GCancellable *cancellable;
        LDAP *ldap;
    } source_import_closure;

    static void source_import_free (gpointer data) {
        source_import_closure *closure = data;
        g_ptr_array_free (closure.keydata, true);
        g_clear_object (&closure.cancellable);
        if (closure.ldap)
            ldap_unbind_ext (closure.ldap, null, null);
        g_free (closure);
    }

    public async List<Key> import_async(InputStream input, Cancellable *cancellable) {
        source_import_closure *closure;

        uint current_index = -1;

        string[] keydata = new string[];
        for (;;) {
            GString *buf = g_string_sized_new (2048);
            uint len = Seahorse.Util.read_data_block(buf, input, "-----BEGIN PGP PUBLIC KEY BLOCK-----",
                                                 "-----END PGP PUBLIC KEY BLOCK-----");
            if (len > 0) {
                keydata += buf.str;
                Seahorse.Progress.prep(closure.cancellable, keydata, null);
            } else {
                break;
            }
        }

        connect_async(cancellable, on_import_connect_completed, g_object_ref (res));
    }

    static void on_import_connect_completed() {
        GError *error = null;

        closure.ldap = connect_finish(result, &error);
        if (error != null) {
            g_simple_async_result_take_error (res, error);
            g_simple_async_result_complete (res);
        } else {
            import_send_key();
        }
    }

    static void import_send_key () {
        char *values[2];
        GSource *gsource;
        GError *error = null;

        if (closure.current_index >= 0) {
            keydata = closure.keydata.pdata[closure.current_index];
            Seahorse.Progress.end(closure.cancellable, keydata);
        }

        closure.current_index++;

        /* All done, complete operation */
        if (closure.current_index == (gint)closure.keydata.len) {
            g_simple_async_result_complete (res);
            return;
        }

        string keydata = closure.keydata[current_index];
        Seahorse.Progress.begin(closure.cancellable, keydata);
        values[0] = keydata;
        values[1] = null;

        LDAPServerInfo sinfo = get_ldap_server_info(true);
        LDAPMod mod;
        memset (&mod, 0, sizeof (mod));
        mod.mod_op = LDAP_MOD_ADD;
        mod.mod_type = sinfo.key_attr;
        mod.mod_values = values;

        LDAPMod *attrs[2];
        attrs[0] = &mod;
        attrs[1] = null;

        string base = "pgpCertid=virtual,%s".printf(sinfo.base_dn);
        int ldap_op;
        int rc = ldap_add_ext (closure.ldap, base, attrs, null, null, &ldap_op);

        if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
            g_simple_async_result_complete (res);
            return;
        }

        gsource = seahorse_ldap_gsource_new (closure.ldap, ldap_op, closure.cancellable);
        g_source_set_callback (gsource, on_import_add_completed);
        g_source_attach (gsource, g_main_context_default ());
        g_source_unref (gsource);
    }

    /* Called when results come in for a key send */
    static bool on_import_add_completed (LDAPMessage *result) {
        if (result.type != LDAP_RES_ADD)
            return false;

        GError *error = null;

        int code; string message;
        int rc = ldap_parse_result(closure.ldap, result, &code, null, &message, null, null, 0);
        g_return_val_if_fail (rc == LDAP_SUCCESS, false);

        /* TODO: Somehow communicate this to the user */
        if (code == LDAP_ALREADY_EXISTS)
            code = LDAP_SUCCESS;

        if (seahorse_ldap_source_propagate_error (self, code, &error)) {
            g_simple_async_result_take_error (res, error);
            g_simple_async_result_complete (res);
            return false; /* don't call for this source again */
        }

        import_send_key();
        return false; /* don't call for this source again */
    }

    static GList * import_finish() {
        if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
            return null;

        /* We don't know the keys that were imported, since this is a server */
        return null;
    }

    private struct ExportClosure {
        GPtrArray *fingerprints;
        gint current_index;
        GString *data;
        GCancellable *cancellable;
        LDAP *ldap;
    }

    static void export_closure_free (gpointer data) {
        ExportClosure *closure = data;
        g_ptr_array_free (closure.fingerprints, true);
        if (closure.data)
            g_string_free (closure.data, true);
        g_clear_object (&closure.cancellable);
        if (closure.ldap)
            ldap_unbind_ext (closure.ldap, null, null);
        g_free (closure);
    }

    public async uint8[] export_async(string[] keyids, Cancellable cancellable) throws GLib.Error {
        StringBuilder data = g_string_sized_new (1024);

        string[[ fingerprints = new string[keyids.length];
        foreach (string keyid in keyids) {
            fingerprints += keyid;
            Seahorse.Progress.prep(closure.cancellable, keyid, null);
        }

        int current_index = -1;

        yield connect_async(cancellable);

        closure.ldap = connect_finish(result, &error);
        if (error != null) {
            g_simple_async_result_take_error (res, error);
            g_simple_async_result_complete (res);
        } else {
            export_retrieve_key();
        }
    }

    static void export_retrieve_key() {
        GSource *gsource;
        GError *error = null;
        int ldap_op;

        string fingerprint;
        if (current_index > 0) {
            fingerprint = closure.fingerprints.pdata[closure.current_index];
            Seahorse.Progress.end(closure.cancellable, fingerprint);
        }

        closure.current_index++;

        /* All done, complete operation */
        if (closure.current_index == (gint)closure.fingerprints.len) {
            g_simple_async_result_complete (res);
            return;
        }

        fingerprint = closure.fingerprints.pdata[closure.current_index];
        Seahorse.Progress.begin(closure.cancellable, fingerprint);
        int length = fingerprint.length;
        if (length > 16)
            fingerprint += (length - 16);

        string filter = "(pgpcertid=%.16s)".printf(fingerprint);
        LDAPServerInfo *sinfo = get_ldap_server_info (self, true);

        char *attrs[2];
        attrs[0] = sinfo.key_attr;
        attrs[1] = null;

        int rc = ldap_search_ext(closure.ldap, sinfo.base_dn, LDAP_SCOPE_SUBTREE,
                                 filter, attrs, 0, null, null, null, 0, &ldap_op);

        if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
            g_simple_async_result_take_error (res, error);
            g_simple_async_result_complete (res);
            return;
        }

        gsource = seahorse_ldap_gsource_new (closure.ldap, ldap_op, closure.cancellable);
        g_source_set_callback (gsource, (GSourceFunc)on_export_search_completed, g_object_ref (res), g_object_unref);
        g_source_attach (gsource, g_main_context_default ());
    }

    static bool on_export_search_completed (LDAPMessage *result) {
        GError *error = null;
        int type;

        type = ldap_msgtype (result);
        g_return_val_if_fail (type == LDAP_RES_SEARCH_ENTRY || type == LDAP_RES_SEARCH_RESULT, false);
        LDAPServerInfo sinfo = get_ldap_server_info(true);

        /* An LDAP Entry */
        if (type == LDAP_RES_SEARCH_ENTRY) {
            debug("Server Info Result");
#ifdef WITH_DEBUG
            dump_ldap_entry (closure.ldap, result);
#endif

            string? key = get_string_attribute (closure.ldap, result, sinfo.key_attr);

            if (key == null) {
                warning("key server missing pgp key data");
                seahorse_ldap_source_propagate_error(LDAP_NO_SUCH_OBJECT, &error);
                g_simple_async_result_take_error (res, error);
                g_simple_async_result_complete (res);
                return false;
            }

            data.append( key);
            data.append_c('\n');

            return true;

        /* No more entries, result */
        } else {
            int code; string message;
            int rc = ldap_parse_result (closure.ldap, result, &code, null, &message, null, null, 0);
            g_return_val_if_fail (rc == LDAP_SUCCESS, false);

            if (seahorse_ldap_source_propagate_error (self, code, &error)) {
                g_simple_async_result_take_error (res, error);
                g_simple_async_result_complete (res);
                return false;
            }

            /* Process more keys if possible */
            export_retrieve_key(res);
            return false;
        }
    }

    static gpointer export_finish(GAsyncResult *result, gsize *size, GError **error) {
        ExportClosure *closure;
        gpointer output;

        g_return_val_if_fail (size != null, null);
        g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
                              seahorse_ldap_source_export_async), null);

        if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
            return null;

        closure = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
        *size = closure.data.len;
        output = g_string_free (closure.data, false);
        closure.data = null;
        return output;
    }

    typedef struct {
        GCancellable *cancellable;
        LDAP *ldap;
    } source_connect_closure;

    static void source_connect_free (gpointer data) {
        source_connect_closure *closure = data;
        g_clear_object (&closure.cancellable);
        if (closure.ldap)
            ldap_unbind_ext (closure.ldap, null, null);
        g_free (closure);
    }

    static string SERVER_ATTRIBUTES[] = {
        "basekeyspacedn",
        "pgpbasekeyspacedn",
        "version",
        null
    };

    private async void connect_async(GCancellable *cancellable) {
        if (this.key_server == null || this.key_server == "")
            return;

        source_connect_closure *closure;

        string pos;
        if ((pos = strchr (this.key_server, ':')) != null)
            *pos = 0;

        Seahorse.Progress.prep_and_begin(cancellable, res, null);

        /* If we have libsoup, try and resolve asynchronously */
#ifdef WITH_SOUP
        Soup.Address address = new Soup.Address(this.key_server, LDAP_PORT);
        Seahorse.Progress.update(cancellable, res, _("Resolving server address: %s"), this.key_server);

        address.resolve_async(null, cancellable, (addr, status) => {
            Seahorse.Progress.update(cancellable, res, _("Connecting to: %s"), this.key_server);

            /* DNS failed */
            if (status != Soup.Status.OK) {
                g_simple_async_result_set_error (res, SEAHORSE_ERROR, -1, _("Couldn’t resolve address: %s"), address.name);
                g_simple_async_result_complete_in_idle (res);

            /* Yay resolved */
            } else {
                once_resolved_start_connect (self, res, soup_address_get_physical (address));
            }
        });
#else /* !WITH_SOUP */
        once_resolved_start_connect();
#endif
    }

    static void once_resolved_start_connect() {
        // XXX we don't actually use this.key_server, but this.key_server *without port*

        if (this.key_server == null || this.key_server == "")
            return;

        source_connect_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
        GError *error = null;

        /* Now that we've resolved our address, connect via IP */
        string text;
        int port = LDAP_PORT;
        if ((text = strchr (this.key_server, ':')) != null) {
            *text = 0;
            text++;
            port = atoi (text);
            if (port <= 0 || port >= G_MAXUINT16) {
                warning("invalid port number: %s (using default)", text);
                port = LDAP_PORT;
            }
        }

        string url = "ldap://%s:%u".printf(this.key_server, port);
        int rc = ldap_initialize(&closure.ldap, url);

        if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
            g_simple_async_result_take_error (res, error);
            g_simple_async_result_complete_in_idle (res);

        /* Start the bind operation */
        } else {
            struct berval cred;
            cred.bv_val = "";
            cred.bv_len = 0;
            int ldap_op;
            rc = ldap_sasl_bind(closure.ldap, null, LDAP_SASL_SIMPLE, &cred, null, null, &ldap_op);
            if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
                g_simple_async_result_take_error (res, error);
                g_simple_async_result_complete_in_idle (res);

            } else {
                GSource *gsource = seahorse_ldap_gsource_new (closure.ldap, ldap_op, closure.cancellable);
                g_source_set_callback (gsource, on_connect_bind_completed, g_object_ref (res), g_object_unref);
                g_source_attach (gsource, g_main_context_default ());
            }
        }
    }

    static bool on_connect_bind_completed (LDAPMessage result) {
        if (result.type != LDAP_RES_BIND)
            return false;

        source_connect_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
        GError *error = null;

        /* The result of the bind operation */
        string message; int code;
        int rc = ldap_parse_result(closure.ldap, result, &code, null, &message, null, null, 0);
        g_return_val_if_fail (rc == LDAP_SUCCESS, false);

        if (seahorse_ldap_source_propagate_error(rc, &error)) {
            g_simple_async_result_take_error (res, error);
            g_simple_async_result_complete_in_idle (res);
            return false; /* don't call this callback again */
        }

        /* Check if we need server info */
        LDAPServerInfo *sinfo = get_ldap_server_info(false);
        if (sinfo != null) {
            g_simple_async_result_complete_in_idle (res);
            Seahorse.Progress.end(closure.cancellable, res);
            return false; /* don't call this callback again */
        }

        /* Retrieve the server info */
        int ldap_op;
        rc = ldap_search_ext (closure.ldap, "cn=PGPServerInfo", LDAP_SCOPE_BASE,
                              "(objectclass=*)", (char **)SERVER_ATTRIBUTES, 0,
                              null, null, null, 0, &ldap_op);

        if (seahorse_ldap_source_propagate_error(rc, &error)) {
            g_simple_async_result_take_error (res, error);
            g_simple_async_result_complete_in_idle (res);
            return false; /* don't call this callback again */

        } else {
            GSource *gsource = seahorse_ldap_gsource_new (closure.ldap, ldap_op, closure.cancellable);
            g_source_set_callback (gsource, on_connect_server_info_completed, g_object_ref (res), g_object_unref);
            g_source_attach (gsource, g_main_context_default ());
        }

        return false; /* don't call this callback again */
    }

    static bool on_connect_server_info_completed (LDAPMessage result) {
        if (result.type != LDAP_RES_SEARCH_ENTRY && result.type != LDAP_RES_SEARCH_RESULT)
            return false;

        source_connect_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
        // If we have results then fill in the server info
        if (result.type == LDAP_RES_SEARCH_ENTRY) {

            debug("Server Info Result");
#ifdef WITH_DEBUG
            dump_ldap_entry (closure.ldap, result);
#endif

            /* NOTE: When adding attributes here make sure to add them to kServerAttributes */
            LDAPServerInfo sinfo = g_new0 (LDAPServerInfo, 1);
            sinfo.version = get_int_attribute (closure.ldap, result, "version");
            sinfo.base_dn = get_string_attribute (closure.ldap, result, "basekeyspacedn");
            if (!sinfo.base_dn)
                sinfo.base_dn = get_string_attribute (closure.ldap, result, "pgpbasekeyspacedn");
            sinfo.key_attr = (sinfo.version > 1)? "pgpkeyv2" : "pgpkey";
            set_data_full("server-info", sinfo, free_ldap_server_info);

            return true; /* callback again */

        } else {
            string message;
            int code;
            int rc = ldap_parse_result (closure.ldap, result, out code, null, out message, null, null, 0);
            g_return_val_if_fail (rc == LDAP_SUCCESS, false);

            if (code != LDAP_SUCCESS)
                warning("operation to get LDAP server info failed: %s", message);

            g_simple_async_result_complete_in_idle (res);
            Seahorse.Progress.end(closure.cancellable, res);
            return false; /* don't callback again */
        }
    }

    static LDAP* seahorse_ldap_source_connect_finish(GError **error) {
        source_connect_closure *closure;

        if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
            return null;

        LDAP *ldap = closure.ldap;
        closure.ldap = null;
        return ldap;
    }
}
