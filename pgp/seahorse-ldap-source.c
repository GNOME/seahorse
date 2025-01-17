/*
 * Seahorse
 *
 * Copyright (C) 2004 - 2006 Stefan Walter
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

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <glib/gi18n.h>

#include "seahorse-ldap-source.h"

#include "seahorse-pgp-key.h"
#include "seahorse-pgp-subkey.h"
#include "seahorse-pgp-uid.h"

#include "seahorse-common.h"

#include "libseahorse/seahorse-util.h"

#include <ldap.h>

#ifdef WITH_LDAP

/* Amount of keys to load in a batch */
#define DEFAULT_LOAD_BATCH 30

struct _SeahorseLDAPSource {
    SeahorseServerSource parent;
};

/* -----------------------------------------------------------------------------
 * SERVER INFO
 */

typedef struct _LDAPServerInfo {
    char *base_dn;              /* The base dn where PGP keys are found */
    char *key_attr;             /* The attribute of PGP key data */
    guint version;              /* The version of the PGP server software */
} LDAPServerInfo;

static void
free_ldap_server_info (LDAPServerInfo *sinfo)
{
    if (sinfo) {
        g_free (sinfo->base_dn);
        g_free (sinfo->key_attr);
        g_free (sinfo);
    }
}

static void
set_ldap_server_info (SeahorseLDAPSource *lsrc, LDAPServerInfo *sinfo)
{
    g_object_set_data_full (G_OBJECT (lsrc), "server-info", sinfo,
                            (GDestroyNotify)free_ldap_server_info);
}

static LDAPServerInfo*
get_ldap_server_info (SeahorseLDAPSource *lsrc, gboolean force)
{
    LDAPServerInfo *sinfo;

    sinfo = g_object_get_data (G_OBJECT (lsrc), "server-info");

    /* When we're asked to force getting the data, we fill in
     * some defaults */
    if (!sinfo && force) {
        sinfo = g_new0 (LDAPServerInfo, 1);
        sinfo->base_dn = g_strdup ("OU=ACTIVE,O=PGP KEYSPACE,C=US");
        sinfo->key_attr = g_strdup ("pgpKey");
        sinfo->version = 0;
        set_ldap_server_info (lsrc, sinfo);
    }

    return sinfo;
}

static void
destroy_ldap (gpointer data)
{
    if (data)
        ldap_unbind_ext ((LDAP *) data, NULL, NULL);
}

/* -----------------------------------------------------------------------------
 *  LDAP HELPERS
 */

#define LDAP_ERROR_DOMAIN (get_ldap_error_domain())

static char**
get_ldap_values (LDAP *ld, LDAPMessage *entry, const char *attribute)
{
    GArray *array;
    struct berval **bv;
    char *value;
    int num, i;

    bv = ldap_get_values_len (ld, entry, attribute);
    if (!bv)
        return NULL;

    array = g_array_new (TRUE, TRUE, sizeof (char*));
    num = ldap_count_values_len (bv);
    for(i = 0; i < num; i++) {
        value = g_strndup (bv[i]->bv_val, bv[i]->bv_len);
        g_array_append_val(array, value);
    }

    return (char**)g_array_free (array, FALSE);
}

#ifdef WITH_DEBUG

static void
dump_ldap_entry (LDAP *ld, LDAPMessage *res)
{
    BerElement *pos;
    char *t;

    t = ldap_get_dn (ld, res);
    g_debug ("dn: %s", t);
    ldap_memfree (t);

    for (t = ldap_first_attribute (ld, res, &pos); t;
         t = ldap_next_attribute (ld, res, pos)) {
        g_auto(GStrv) values = NULL;

        values = get_ldap_values (ld, res, t);
        for (char **v = values; *v; v++)
            g_debug ("%s: %s", t, *v);

        ldap_memfree (t);
    }

    ber_free (pos, 0);
}

#endif /* WITH_DEBUG */

static GQuark
get_ldap_error_domain ()
{
    static GQuark q = 0;
    if(q == 0)
        q = g_quark_from_static_string ("seahorse-ldap-error");
    return q;
}

static char*
get_string_attribute (LDAP *ld, LDAPMessage *res, const char *attribute)
{
    g_auto(GStrv) vals = NULL;
    char *v;

    vals = get_ldap_values (ld, res, attribute);
    if (!vals)
        return NULL;
    v = vals[0] ? g_strdup (vals[0]) : NULL;
    return v;
}

static gboolean
get_boolean_attribute (LDAP* ld, LDAPMessage *res, const char *attribute)
{
    g_auto(GStrv) vals = NULL;
    gboolean b;

    vals = get_ldap_values (ld, res, attribute);
    if (!vals)
        return FALSE;
    b = vals[0] && atoi (vals[0]) == 1;
    return b;
}

static long int
get_int_attribute (LDAP* ld, LDAPMessage *res, const char *attribute)
{
    g_auto(GStrv) vals = NULL;
    long int d;

    vals = get_ldap_values (ld, res, attribute);
    if (!vals)
        return 0;
    d = vals[0] ? atoi (vals[0]) : 0;
    return d;
}

static long int
get_date_attribute (LDAP* ld, LDAPMessage *res, const char *attribute)
{
    g_auto(GStrv) vals = NULL;
    struct tm t;
    long int d = 0;

    vals = get_ldap_values (ld, res, attribute);
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

static const char*
get_algo_attribute (LDAP* ld, LDAPMessage *res, const char *attribute)
{
    g_auto(GStrv) vals = NULL;
    const char *a = NULL;

    vals = get_ldap_values (ld, res, attribute);
    if (!vals)
        return 0;

    if (vals[0]) {
        if (g_ascii_strcasecmp (vals[0], "DH/DSS") == 0 ||
            g_ascii_strcasecmp (vals[0], "Elg") == 0 ||
            g_ascii_strcasecmp (vals[0], "Elgamal") == 0 ||
            g_ascii_strcasecmp (vals[0], "DSS/DH") == 0)
            a = "Elgamal";
        if (g_ascii_strcasecmp (vals[0], "RSA") == 0)
            a = "RSA";
        if (g_ascii_strcasecmp (vals[0], "DSA") == 0)
            a = "DSA";
    }

    return a;
}

/*
 * Escapes a value so it's safe to use in an LDAP filter. Also trims
 * any spaces which cause problems with some LDAP servers.
 */
static char*
escape_ldap_value (const char *v)
{
    GString *value;
    char* result;

    g_assert (v);
    value = g_string_sized_new (strlen(v));

    for ( ; *v; v++) {
        switch(*v) {
        case '#': case ',': case '+': case '\\':
        case '/': case '\"': case '<': case '>': case ';':
            value = g_string_append_c (value, '\\');
            value = g_string_append_c (value, *v);
            continue;
        };

        if(*v < 32 || *v > 126) {
            g_string_append_printf (value, "\\%02X", *v);
            continue;
        }

        value = g_string_append_c (value, *v);
    }

    result = g_string_free (value, FALSE);
    g_strstrip (result);
    return result;
}

typedef gboolean (*SeahorseLdapCallback)   (LDAPMessage *result,
                                            gpointer user_data);

typedef struct {
    GSource source;
    LDAP *ldap;
    int ldap_op;
    GCancellable *cancellable;
    gboolean cancelled;
    int cancelled_sig;
} SeahorseLdapGSource;

static gboolean
seahorse_ldap_gsource_prepare (GSource *gsource,
                               int *timeout)
{
    SeahorseLdapGSource *ldap_gsource = (SeahorseLdapGSource *)gsource;

    if (ldap_gsource->cancelled)
        return TRUE;

    /* No other way, but to poll */
    *timeout = 50;
    return FALSE;
}

static gboolean
seahorse_ldap_gsource_check (GSource *gsource)
{
    return TRUE;
}

static gboolean
seahorse_ldap_gsource_dispatch (GSource *gsource,
                                GSourceFunc callback,
                                gpointer user_data)
{
    SeahorseLdapGSource *ldap_gsource = (SeahorseLdapGSource *)gsource;
    struct timeval timeout;
    LDAPMessage *result;
    gboolean ret;
    int rc, i;

    if (ldap_gsource->cancelled) {
        ((SeahorseLdapCallback)callback) (NULL, user_data);
        return FALSE;
    }

    for (i = 0; i < DEFAULT_LOAD_BATCH; i++) {

        /* This effects a poll */
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;

        rc = ldap_result (ldap_gsource->ldap, ldap_gsource->ldap_op,
                          0, &timeout, &result);
        if (rc == -1) {
            g_warning ("ldap_result failed with rc = %d, errno = %s",
                       rc, g_strerror (errno));
            return G_SOURCE_REMOVE;
        }

        /* Timeout */
        if (rc == 0)
            return G_SOURCE_CONTINUE;

        ret = ((SeahorseLdapCallback)callback) (result, user_data);
        ldap_msgfree (result);

        if (!ret)
            return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

static void
seahorse_ldap_gsource_finalize (GSource *gsource)
{
    SeahorseLdapGSource *ldap_gsource = (SeahorseLdapGSource *)gsource;
    g_cancellable_disconnect (ldap_gsource->cancellable,
                              ldap_gsource->cancelled_sig);
    g_clear_object (&ldap_gsource->cancellable);
}

static GSourceFuncs seahorse_ldap_gsource_funcs = {
    seahorse_ldap_gsource_prepare,
    seahorse_ldap_gsource_check,
    seahorse_ldap_gsource_dispatch,
    seahorse_ldap_gsource_finalize,
};

static void
on_ldap_gsource_cancelled (GCancellable *cancellable,
                           gpointer user_data)
{
    SeahorseLdapGSource *ldap_gsource = user_data;
    ldap_gsource->cancelled = TRUE;
}

static GSource *
seahorse_ldap_gsource_new (LDAP *ldap,
                           int ldap_op,
                           GCancellable *cancellable)
{
    GSource *gsource;
    SeahorseLdapGSource *ldap_gsource;

    gsource = g_source_new (&seahorse_ldap_gsource_funcs,
                            sizeof (SeahorseLdapGSource));

    ldap_gsource = (SeahorseLdapGSource *)gsource;
    ldap_gsource->ldap = ldap;
    ldap_gsource->ldap_op = ldap_op;

    if (cancellable) {
        ldap_gsource->cancellable = g_object_ref (cancellable);
        ldap_gsource->cancelled_sig = g_cancellable_connect (cancellable,
                                                             G_CALLBACK (on_ldap_gsource_cancelled),
                                                             ldap_gsource, NULL);
    }

    return gsource;
}

static gboolean
seahorse_ldap_source_propagate_error (SeahorseLDAPSource *self,
                                      int rc, GError **error)
{
    g_autofree char *uri = NULL;

    if (rc == LDAP_SUCCESS)
        return FALSE;

    uri = seahorse_place_get_uri (SEAHORSE_PLACE (self));
    g_set_error (error, LDAP_ERROR_DOMAIN, rc, _("Couldn’t communicate with %s: %s"),
                 uri, ldap_err2string (rc));

    return TRUE;
}

typedef struct {
    LDAP *ldap;
} ConnectClosure;

static void
connect_closure_free (gpointer data)
{
    ConnectClosure *closure = data;
    if (closure->ldap)
        ldap_unbind_ext (closure->ldap, NULL, NULL);
    g_free (closure);
}

static gboolean
on_connect_server_info_completed (LDAPMessage *result,
                                  gpointer user_data)
{
    GTask *task = G_TASK (user_data);
    ConnectClosure *closure = g_task_get_task_data (task);
    GCancellable *cancellable = g_task_get_cancellable (task);
    SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (g_task_get_source_object (task));
    char *message;
    int code;
    int type;
    int rc;

    type = ldap_msgtype (result);
    g_return_val_if_fail (type == LDAP_RES_SEARCH_ENTRY || type == LDAP_RES_SEARCH_RESULT, FALSE);

    /* If we have results then fill in the server info */
    if (type == LDAP_RES_SEARCH_ENTRY) {
        LDAPServerInfo *sinfo;

        g_debug ("Server Info Result");
#ifdef WITH_DEBUG
        dump_ldap_entry (closure->ldap, result);
#endif

        /* NOTE: When adding attributes here make sure to add them to kServerAttributes */
        sinfo = g_new0 (LDAPServerInfo, 1);
        sinfo->version = get_int_attribute (closure->ldap, result, "version");
        sinfo->base_dn = get_string_attribute (closure->ldap, result, "basekeyspacedn");
        if (!sinfo->base_dn)
            sinfo->base_dn = get_string_attribute (closure->ldap, result, "pgpbasekeyspacedn");
        sinfo->key_attr = g_strdup (sinfo->version > 1 ? "pgpkeyv2" : "pgpkey");
        set_ldap_server_info (self, sinfo);

        return G_SOURCE_CONTINUE;
    }

    rc = ldap_parse_result (closure->ldap, result, &code, NULL, &message,
                            NULL, NULL, 0);
    g_return_val_if_fail (rc == LDAP_SUCCESS, FALSE);

    if (code != LDAP_SUCCESS)
        g_warning ("operation to get LDAP server info failed: %s", message);

    ldap_memfree (message);

    g_task_return_pointer (task, g_steal_pointer (&closure->ldap), destroy_ldap);
    return G_SOURCE_REMOVE;
}

static const char *SERVER_ATTRIBUTES[] = {
    "basekeyspacedn",
    "pgpbasekeyspacedn",
    "version",
    NULL
};

static gboolean
on_connect_bind_completed (LDAPMessage *result,
                           gpointer user_data)
{
    GTask *task = G_TASK (user_data);
    ConnectClosure *closure = g_task_get_task_data (task);
    GCancellable *cancellable = g_task_get_cancellable (task);
    SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (g_task_get_source_object (task));
    LDAPServerInfo *sinfo;
    g_autoptr(GError) error = NULL;
    char *message;
    int ldap_op;
    int code;
    int rc;
    g_autoptr(GSource) gsource = NULL;

    g_return_val_if_fail (ldap_msgtype (result) == LDAP_RES_BIND, FALSE);

    /* The result of the bind operation */
    rc = ldap_parse_result (closure->ldap, result, &code, NULL, &message,
                            NULL, NULL, 0);
    g_return_val_if_fail (rc == LDAP_SUCCESS, FALSE);
    ldap_memfree (message);

    if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return G_SOURCE_REMOVE;
    }

    /* Check if we need server info */
    sinfo = get_ldap_server_info (self, FALSE);
    if (sinfo != NULL) {
        g_task_return_pointer (task, g_steal_pointer (&closure->ldap), destroy_ldap);
        return G_SOURCE_REMOVE;
    }

    /* Retrieve the server info */
    rc = ldap_search_ext (closure->ldap, "cn=PGPServerInfo", LDAP_SCOPE_BASE,
                          "(objectclass=*)", (char **)SERVER_ATTRIBUTES, 0,
                          NULL, NULL, NULL, 0, &ldap_op);

    if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return G_SOURCE_REMOVE;
    }

    gsource = seahorse_ldap_gsource_new (closure->ldap, ldap_op, cancellable);
    g_source_set_callback (gsource,
                           G_SOURCE_FUNC (on_connect_server_info_completed),
                           g_object_ref (task), g_object_unref);
    g_source_attach (gsource, g_main_context_default ());

    return G_SOURCE_REMOVE;
}

static void
on_address_resolved (GObject      *src_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
    GSocketAddressEnumerator *enumer = G_SOCKET_ADDRESS_ENUMERATOR (src_object);
    g_autoptr(GTask) task = user_data;
    SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (g_task_get_source_object (task));
    ConnectClosure *closure = g_task_get_task_data (task);
    GCancellable *cancellable = g_task_get_cancellable (task);
    g_autofree char *uri = NULL;
    g_autoptr(GSocketAddress) addr = NULL;
    g_autoptr(GError) error = NULL;
    g_autofree char *addr_str = NULL;
    g_autofree char *resolved_url = NULL;
    int rc;
    struct berval cred;
    int ldap_op;
    g_autoptr(GSource) gsource = NULL;

    /* Note: this is the original (unresolved) URI */
    uri = seahorse_place_get_uri (SEAHORSE_PLACE (self));

    /* Get the resolved IP */
    addr = g_socket_address_enumerator_next_finish (enumer, res, &error);
    if (!addr) {
        g_task_return_new_error (task, SEAHORSE_ERROR, -1,
                                 _("Couldn’t resolve address %s"), uri);
        return;
    }

    /* Re-create the URL with the resolved IP */
    addr_str = g_socket_connectable_to_string (G_SOCKET_CONNECTABLE (addr));
    resolved_url = g_strdup_printf ("ldap://%s", addr_str);
    rc = ldap_initialize (&closure->ldap, resolved_url);

    if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    /* Start the bind operation */
    cred.bv_val = "";
    cred.bv_len = 0;

    rc = ldap_sasl_bind (closure->ldap, NULL, LDAP_SASL_SIMPLE, &cred, NULL,
                         NULL, &ldap_op);
    if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    gsource = seahorse_ldap_gsource_new (closure->ldap, ldap_op, cancellable);
    g_source_set_callback (gsource, G_SOURCE_FUNC (on_connect_bind_completed),
                           g_object_ref (task), g_object_unref);
    g_source_attach (gsource, g_main_context_default ());
}

static void
seahorse_ldap_source_connect_async (SeahorseLDAPSource *source,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
    g_autoptr(GTask) task = NULL;
    ConnectClosure *closure;
    g_autofree char *uri = NULL;
    g_autoptr(GSocketConnectable) addr = NULL;
    g_autoptr(GSocketAddressEnumerator) addr_enumer = NULL;
    g_autoptr(GError) error = NULL;

    task = g_task_new (source, cancellable, callback, user_data);
    g_task_set_source_tag (task, seahorse_ldap_source_connect_async);

    closure = g_new0 (ConnectClosure, 1);
    g_task_set_task_data (task, closure, connect_closure_free);

    /* Take the URI & turn it into a GNetworkAddress, to do address resolving */
    uri = seahorse_place_get_uri (SEAHORSE_PLACE (source));
    g_return_if_fail (uri && uri[0]);

    addr = g_network_address_parse_uri (uri, LDAP_PORT, &error);
    if (!addr) {
      g_task_return_new_error (task, SEAHORSE_ERROR, -1,
                               _("Invalid URI: %s"), uri);
      return;
    }

    /* Now get a GSocketAddressEnumerator to do the resolving */
    addr_enumer = g_socket_connectable_enumerate (addr);
    g_socket_address_enumerator_next_async (addr_enumer,
                                            cancellable,
                                            on_address_resolved,
                                            g_steal_pointer (&task));
}

static LDAP *
seahorse_ldap_source_connect_finish (SeahorseLDAPSource *source,
                                     GAsyncResult *result,
                                     GError **error)
{
    g_return_val_if_fail (g_task_is_valid (result, source), NULL);
    g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) ==
                          seahorse_ldap_source_connect_async, NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}

G_DEFINE_TYPE (SeahorseLDAPSource, seahorse_ldap_source, SEAHORSE_TYPE_SERVER_SOURCE);

static void
seahorse_ldap_source_init (SeahorseLDAPSource *self)
{
}

typedef struct {
    char *filter;
    LDAP *ldap;
    GListStore *results;
} SearchClosure;

static void
search_closure_free (gpointer data)
{
    SearchClosure *closure = data;
    g_clear_object (&closure->results);
    g_free (closure->filter);
    if (closure->ldap)
        ldap_unbind_ext (closure->ldap, NULL, NULL);
    g_free (closure);
}

static const char *PGP_ATTRIBUTES[] = {
    "pgpcertid",
    "pgpuserid",
    "pgprevoked",
    "pgpdisabled",
    "pgpkeycreatetime",
    "pgpkeyexpiretime"
    "pgpkeysize",
    "pgpkeytype",
    NULL
};

/* Add a key to the key source from an LDAP entry */
static void
search_parse_key_from_ldap_entry (SeahorseLDAPSource *self,
                                  GListStore         *results,
                                  LDAP               *ldap,
                                  LDAPMessage        *res)
{
    const char *algo;
    long int timestamp;
    long int expires;
    g_autofree char *fpr = NULL;
    g_autofree char *uidstr = NULL;
    gboolean revoked;
    gboolean disabled;
    int length;

    g_return_if_fail (ldap_msgtype (res) == LDAP_RES_SEARCH_ENTRY);

    fpr = get_string_attribute (ldap, res, "pgpcertid");
    uidstr = get_string_attribute (ldap, res, "pgpuserid");
    revoked = get_boolean_attribute (ldap, res, "pgprevoked");
    disabled = get_boolean_attribute (ldap, res, "pgpdisabled");
    timestamp = get_date_attribute (ldap, res, "pgpkeycreatetime");
    expires = get_date_attribute (ldap, res, "pgpkeyexpiretime");
    algo = get_algo_attribute (ldap, res, "pgpkeytype");
    length = get_int_attribute (ldap, res, "pgpkeysize");

    if (fpr && uidstr) {
        g_autoptr (SeahorsePgpSubkey) subkey = NULL;
        g_autoptr(SeahorsePgpKey) key = NULL;
        g_autofree char *fingerprint = NULL;
        g_autoptr(SeahorsePgpUid) uid = NULL;
        unsigned int flags = 0;

        /* Build up a subkey */
        subkey = seahorse_pgp_subkey_new ();
        seahorse_pgp_subkey_set_keyid (subkey, fpr);
        fingerprint = seahorse_pgp_subkey_calc_fingerprint (fpr);
        seahorse_pgp_subkey_set_fingerprint (subkey, fingerprint);
        seahorse_pgp_subkey_set_algorithm (subkey, algo);
        seahorse_pgp_subkey_set_length (subkey, length);

        if (timestamp > 0) {
            g_autoptr(GDateTime) created_date = NULL;
            created_date = g_date_time_new_from_unix_utc (timestamp);
            seahorse_pgp_subkey_set_created (subkey, created_date);
        }
        if (expires > 0) {
            g_autoptr(GDateTime) expires_date = NULL;
            expires_date = g_date_time_new_from_unix_utc (expires);
            seahorse_pgp_subkey_set_expires (subkey, expires_date);
        }

        if (revoked)
            flags |= SEAHORSE_FLAG_REVOKED;
        if (disabled)
            flags |= SEAHORSE_FLAG_DISABLED;
        seahorse_pgp_subkey_set_flags (subkey, flags);

        key = seahorse_pgp_key_new ();

        /* Build up a uid */
        uid = seahorse_pgp_uid_new (key, uidstr);
        if (revoked)
            seahorse_pgp_uid_set_validity (uid, SEAHORSE_VALIDITY_REVOKED);
        seahorse_pgp_key_add_uid (key, uid);

        /* Now build them into a key */
        seahorse_pgp_key_add_subkey (key, subkey);
        seahorse_item_set_place (SEAHORSE_ITEM (key), SEAHORSE_PLACE (self));
        seahorse_pgp_key_set_item_flags (key, flags);

        g_list_store_append (results, key);
    }
}

static gboolean
on_search_search_completed (LDAPMessage *result,
                            gpointer user_data)
{
    GTask *task = G_TASK (user_data);
    SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (g_task_get_source_object (task));
    SearchClosure *closure = g_task_get_task_data (task);
    GCancellable *cancellable = g_task_get_cancellable (task);
    g_autoptr(GError) error = NULL;
    int type;
    int rc;
    int code;
    char *message;

    type = ldap_msgtype (result);
    g_return_val_if_fail (type == LDAP_RES_SEARCH_ENTRY || type == LDAP_RES_SEARCH_RESULT, FALSE);

    /* An LDAP entry */
    if (type == LDAP_RES_SEARCH_ENTRY) {
        g_debug ("Retrieved Key Entry");
#ifdef WITH_DEBUG
        dump_ldap_entry (closure->ldap, result);
#endif

        search_parse_key_from_ldap_entry (self, closure->results,
                                          closure->ldap, result);
        return G_SOURCE_CONTINUE;
    }

    /* All entries done */
    rc = ldap_parse_result (closure->ldap, result, &code, NULL,
                            &message, NULL, NULL, 0);
    g_return_val_if_fail (rc == LDAP_SUCCESS, FALSE);

    /* Error codes that we ignore */
    switch (code) {
    case LDAP_SIZELIMIT_EXCEEDED:
        code = LDAP_SUCCESS;
        break;
    };

    if (code != LDAP_SUCCESS)
        g_task_return_new_error (task, LDAP_ERROR_DOMAIN, code, "%s", message);
    else if (seahorse_ldap_source_propagate_error (self, code, &error))
        g_task_return_error (task, g_steal_pointer (&error));
    else
        g_task_return_boolean (task, TRUE);

    ldap_memfree (message);

    return G_SOURCE_REMOVE;
}

static void
on_search_connect_completed (GObject *source,
                             GAsyncResult *result,
                             gpointer user_data)
{
    SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (source);
    g_autoptr(GTask) task = G_TASK (user_data);
    SearchClosure *closure = g_task_get_task_data (task);
    GCancellable *cancellable = g_task_get_cancellable (task);
    g_autoptr(GError) error = NULL;
    LDAPServerInfo *sinfo;
    int ldap_op;
    int rc;
    g_autoptr(GSource) gsource = NULL;

    closure->ldap = seahorse_ldap_source_connect_finish (self, result, &error);
    if (error != NULL) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    sinfo = get_ldap_server_info (self, TRUE);

    g_debug ("Searching Server ... base: %s, filter: %s",
             sinfo->base_dn, closure->filter);

    rc = ldap_search_ext (closure->ldap, sinfo->base_dn, LDAP_SCOPE_SUBTREE,
                          closure->filter, (char **)PGP_ATTRIBUTES, 0,
                          NULL, NULL, NULL, 0, &ldap_op);

    if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    gsource = seahorse_ldap_gsource_new (closure->ldap, ldap_op, cancellable);
    g_source_set_callback (gsource, G_SOURCE_FUNC (on_search_search_completed),
                           g_steal_pointer (&task), g_object_unref);
    g_source_attach (gsource, g_main_context_default ());
}


static void
seahorse_ldap_source_search_async (SeahorseServerSource *source,
                                   const char           *match,
                                   GListStore           *results,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   void                 *user_data)
{
    SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (source);
    SearchClosure *closure;
    g_autoptr(GTask) task = NULL;
    g_autofree char *text = NULL;

    task = g_task_new (source, cancellable, callback, user_data);
    g_task_set_source_tag (task, seahorse_ldap_source_search_async);
    closure = g_new0 (SearchClosure, 1);
    closure->results = g_object_ref (results);
    text = escape_ldap_value (match);
    closure->filter = g_strdup_printf ("(pgpuserid=*%s*)", text);
    g_task_set_task_data (task, closure, search_closure_free);

    seahorse_ldap_source_connect_async (self, cancellable,
                                        on_search_connect_completed,
                                        g_steal_pointer (&task));
}

static gboolean
seahorse_ldap_source_search_finish (SeahorseServerSource *source,
                                    GAsyncResult *result,
                                    GError **error)
{
    g_return_val_if_fail (SEAHORSE_IS_LDAP_SOURCE (source), FALSE);
    g_return_val_if_fail (g_task_is_valid (result, source), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

typedef struct {
    GPtrArray *keydatas;
    int current_index;
    LDAP *ldap;
} ImportClosure;

static void
import_closure_free (gpointer data)
{
    ImportClosure *closure = data;
    g_ptr_array_free (closure->keydatas, TRUE);
    if (closure->ldap)
        ldap_unbind_ext (closure->ldap, NULL, NULL);
    g_free (closure);
}

static void import_send_key (SeahorseLDAPSource *self, GTask *task);

/* Called when results come in for a key send */
static gboolean
on_import_add_completed (LDAPMessage *result,
                         gpointer user_data)
{
    GTask *task = G_TASK (user_data);
    ImportClosure *closure = g_task_get_task_data (task);
    SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (g_task_get_source_object (task));
    GError *error = NULL;
    char *message;
    int code;
    int rc;

    g_return_val_if_fail (ldap_msgtype (result) == LDAP_RES_ADD, FALSE);

    rc = ldap_parse_result (closure->ldap, result, &code, NULL,
                            &message, NULL, NULL, 0);
    g_return_val_if_fail (rc == LDAP_SUCCESS, FALSE);

    /* TODO: Somehow communicate this to the user */
    if (code == LDAP_ALREADY_EXISTS)
        code = LDAP_SUCCESS;

    ldap_memfree (message);

    if (seahorse_ldap_source_propagate_error (self, code, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return G_SOURCE_REMOVE;
    }

    import_send_key (self, task);
    return G_SOURCE_REMOVE;
}

static void
import_send_key (SeahorseLDAPSource *self, GTask *task)
{
    ImportClosure *closure = g_task_get_task_data (task);
    GCancellable *cancellable = g_task_get_cancellable (task);
    LDAPServerInfo *sinfo;
    g_autofree char *base = NULL;
    LDAPMod mod;
    LDAPMod *attrs[2];
    char *values[2];
    g_autoptr(GSource) gsource = NULL;
    GError *error = NULL;
    char *keydata;
    int ldap_op;
    int rc;

    if (closure->current_index >= 0) {
        keydata = g_ptr_array_index (closure->keydatas, closure->current_index);
    }

    closure->current_index++;

    /* All done, complete operation */
    if (closure->current_index == (int) closure->keydatas->len) {
        g_task_return_boolean (task, TRUE);
        return;
    }

    keydata = g_ptr_array_index (closure->keydatas, closure->current_index);
    values[0] = keydata;
    values[1] = NULL;

    sinfo = get_ldap_server_info (self, TRUE);
    memset (&mod, 0, sizeof (mod));
    mod.mod_op = LDAP_MOD_ADD;
    mod.mod_type = sinfo->key_attr;
    mod.mod_values = values;

    attrs[0] = &mod;
    attrs[1] = NULL;

    base = g_strdup_printf ("pgpCertid=virtual,%s", sinfo->base_dn);
    rc = ldap_add_ext (closure->ldap, base, attrs, NULL, NULL, &ldap_op);

    if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    gsource = seahorse_ldap_gsource_new (closure->ldap, ldap_op, cancellable);
    g_source_set_callback (gsource, G_SOURCE_FUNC (on_import_add_completed),
                           g_object_ref (task), g_object_unref);
    g_source_attach (gsource, g_main_context_default ());
}

static void
on_import_connect_completed (GObject *source,
                             GAsyncResult *result,
                             gpointer user_data)
{
    g_autoptr(GTask) task = G_TASK (user_data);
    ImportClosure *closure = g_task_get_task_data (task);
    SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (source);
    g_autoptr(GError) error = NULL;

    closure->ldap = seahorse_ldap_source_connect_finish (self, result, &error);
    if (error != NULL) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    import_send_key (self, task);
}

static void
seahorse_ldap_source_import_async (SeahorseServerSource *source,
                                   GInputStream *input,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
    SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (source);
    g_autoptr(GTask) task = NULL;
    ImportClosure *closure;

    task = g_task_new (source, cancellable, callback, user_data);
    g_task_set_source_tag (task, seahorse_ldap_source_import_async);

    closure = g_new0 (ImportClosure, 1);
    closure->current_index = -1;
    g_task_set_task_data (task, closure, import_closure_free);

    closure->keydatas = g_ptr_array_new_with_free_func (g_free);
    for (;;) {
        g_autoptr(GString) buf = g_string_sized_new (2048);
        guint len;
        g_autofree char *keydata = NULL;

        len = seahorse_util_read_data_block (buf, input, "-----BEGIN PGP PUBLIC KEY BLOCK-----",
                                             "-----END PGP PUBLIC KEY BLOCK-----");
        if (len <= 0)
            break;

        keydata = g_string_free (g_steal_pointer (&buf), FALSE);
        g_ptr_array_add (closure->keydatas, g_steal_pointer (&keydata));
    }

    seahorse_ldap_source_connect_async (self, cancellable,
                                        on_import_connect_completed,
                                        g_steal_pointer (&task));
}

static GList *
seahorse_ldap_source_import_finish (SeahorseServerSource *source,
                                    GAsyncResult *result,
                                    GError **error)
{
    g_return_val_if_fail (g_task_is_valid (result, source), NULL);
    g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) ==
                          seahorse_ldap_source_import_async, NULL);

    if (!g_task_propagate_boolean (G_TASK (result), error))
        return NULL;

    /* We don't know the keys that were imported, since this is a server */
    return NULL;
}

typedef struct {
    GPtrArray *fingerprints;
    int current_index;
    GString *data;
    LDAP *ldap;
} ExportClosure;

static void
export_closure_free (gpointer data)
{
    ExportClosure *closure = data;
    g_ptr_array_free (closure->fingerprints, TRUE);
    if (closure->data)
        g_string_free (closure->data, TRUE);
    if (closure->ldap)
        ldap_unbind_ext (closure->ldap, NULL, NULL);
    g_free (closure);
}

static void     export_retrieve_key     (SeahorseLDAPSource *self,
                                         GTask *task);

static gboolean
on_export_search_completed (LDAPMessage *result,
                            gpointer user_data)
{
    GTask *task = G_TASK (user_data);
    ExportClosure *closure = g_task_get_task_data (task);
    SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (g_task_get_source_object (task));
    LDAPServerInfo *sinfo;
    char *message;
    GError *error = NULL;
    int code;
    int type;
    int rc;

    type = ldap_msgtype (result);
    g_return_val_if_fail (type == LDAP_RES_SEARCH_ENTRY || type == LDAP_RES_SEARCH_RESULT, FALSE);
    sinfo = get_ldap_server_info (self, TRUE);

    /* An LDAP Entry */
    if (type == LDAP_RES_SEARCH_ENTRY) {
        g_autofree char *key = NULL;

        g_debug ("Server Info Result");
#ifdef WITH_DEBUG
        dump_ldap_entry (closure->ldap, result);
#endif

        key = get_string_attribute (closure->ldap, result, sinfo->key_attr);
        if (key == NULL) {
            g_warning ("key server missing pgp key data");
            seahorse_ldap_source_propagate_error (self, LDAP_NO_SUCH_OBJECT, &error);
            g_task_return_error (task, g_steal_pointer (&error));
            return G_SOURCE_REMOVE;
        }

        g_string_append (closure->data, key);
        g_string_append_c (closure->data, '\n');

        return G_SOURCE_CONTINUE;
    }

    /* No more entries, result */
    rc = ldap_parse_result (closure->ldap, result, &code, NULL,
                            &message, NULL, NULL, 0);
    g_return_val_if_fail (rc == LDAP_SUCCESS, FALSE);

    if (seahorse_ldap_source_propagate_error (self, code, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return G_SOURCE_REMOVE;
    }

    ldap_memfree (message);

    /* Process more keys if possible */
    export_retrieve_key (self, task);
    return G_SOURCE_REMOVE;
}

static void
export_retrieve_key (SeahorseLDAPSource *self,
                     GTask *task)
{
    ExportClosure *closure = g_task_get_task_data (task);
    GCancellable *cancellable = g_task_get_cancellable (task);
    LDAPServerInfo *sinfo;
    g_autofree char *filter = NULL;
    char *attrs[2];
    g_autoptr(GSource) gsource = NULL;
    const char *fingerprint;
    g_autoptr(GError) error = NULL;
    int length, rc;
    int ldap_op;

    if (closure->current_index > 0) {
        fingerprint = g_ptr_array_index (closure->fingerprints,
                                         closure->current_index);
    }

    closure->current_index++;

    /* All done, complete operation */
    if (closure->current_index == (int) closure->fingerprints->len) {
        g_task_return_boolean (task, TRUE);
        return;
    }

    fingerprint = g_ptr_array_index (closure->fingerprints,
                                     closure->current_index);
    length = strlen (fingerprint);
    if (length > 16)
        fingerprint += (length - 16);

    filter = g_strdup_printf ("(pgpcertid=%.16s)", fingerprint);
    sinfo = get_ldap_server_info (self, TRUE);

    attrs[0] = sinfo->key_attr;
    attrs[1] = NULL;

    rc = ldap_search_ext (closure->ldap, sinfo->base_dn, LDAP_SCOPE_SUBTREE,
                          filter, attrs, 0,
                          NULL, NULL, NULL, 0, &ldap_op);

    if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    gsource = seahorse_ldap_gsource_new (closure->ldap, ldap_op, cancellable);
    g_source_set_callback (gsource, (GSourceFunc)on_export_search_completed,
                           g_object_ref (task), g_object_unref);
    g_source_attach (gsource, g_main_context_default ());
}

static void
on_export_connect_completed (GObject *source,
                             GAsyncResult *result,
                             gpointer user_data)
{
    SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (source);
    g_autoptr(GTask) task = G_TASK (user_data);
    ExportClosure *closure = g_task_get_task_data (task);
    g_autoptr(GError) error = NULL;

    closure->ldap = seahorse_ldap_source_connect_finish (self, result, &error);
    if (error != NULL) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    export_retrieve_key (self, task);
}

static void
seahorse_ldap_source_export_async (SeahorseServerSource  *source,
                                   const char           **keyids,
                                   GCancellable          *cancellable,
                                   GAsyncReadyCallback    callback,
                                   void                  *user_data)
{
    SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (source);
    ExportClosure *closure;
    g_autoptr(GTask) task = NULL;

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, seahorse_ldap_source_export_async);

    closure = g_new0 (ExportClosure, 1);
    closure->data = g_string_sized_new (1024);
    closure->fingerprints = g_ptr_array_new_with_free_func (g_free);
    for (int i = 0; keyids[i] != NULL; i++) {
        char *fingerprint = g_strdup (keyids[i]);

        g_ptr_array_add (closure->fingerprints, fingerprint);
    }
    closure->current_index = -1;
    g_task_set_task_data (task, closure, export_closure_free);

    seahorse_ldap_source_connect_async (self, cancellable,
                                        on_export_connect_completed,
                                        g_steal_pointer (&task));
}

static GBytes *
seahorse_ldap_source_export_finish (SeahorseServerSource *source,
                                    GAsyncResult         *result,
                                    GError              **error)
{
    ExportClosure *closure;

    g_return_val_if_fail (g_task_is_valid (result, source), NULL);

    if (!g_task_propagate_boolean (G_TASK (result), error))
        return NULL;

    closure = g_task_get_task_data (G_TASK (result));
    return g_string_free_to_bytes (g_steal_pointer (&closure->data));
}

/* Initialize the basic class stuff */
static void
seahorse_ldap_source_class_init (SeahorseLDAPSourceClass *klass)
{
    SeahorseServerSourceClass *server_class = SEAHORSE_SERVER_SOURCE_CLASS (klass);

    server_class->search_async = seahorse_ldap_source_search_async;
    server_class->search_finish = seahorse_ldap_source_search_finish;
    server_class->export_async = seahorse_ldap_source_export_async;
    server_class->export_finish = seahorse_ldap_source_export_finish;
    server_class->import_async = seahorse_ldap_source_import_async;
    server_class->import_finish = seahorse_ldap_source_import_finish;
}

/**
 * seahorse_ldap_source_new
 * @uri: The server to connect to
 *
 * Creates a new key source for an LDAP PGP server.
 *
 * Returns: A new LDAP Key Source
 */
SeahorseLDAPSource *
seahorse_ldap_source_new (const char *uri)
{
    g_return_val_if_fail (seahorse_ldap_is_valid_uri (uri), NULL);

    return g_object_new (SEAHORSE_TYPE_LDAP_SOURCE, "uri", uri, NULL);
}

/**
 * seahorse_ldap_is_valid_uri
 * @uri: The uri to check
 *
 * Returns: Whether the passed uri is valid for an ldap key source
 */
gboolean
seahorse_ldap_is_valid_uri (const char *uri)
{
    LDAPURLDesc *url;
    int r;

    g_return_val_if_fail (uri && *uri, FALSE);

    r = ldap_url_parse (uri, &url);
    if (r == LDAP_URL_SUCCESS) {
        /* Some checks to make sure it's a simple URI */
        if (!(url->lud_host && url->lud_host[0]) ||
            (url->lud_dn && url->lud_dn[0]) ||
            (url->lud_attrs || url->lud_attrs))
            r = LDAP_URL_ERR_PARAM;

        ldap_free_urldesc (url);
    }

    return r == LDAP_URL_SUCCESS;
}

#endif /* WITH_LDAP */
