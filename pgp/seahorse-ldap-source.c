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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
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
#include "seahorse-object-list.h"
#include "seahorse-progress.h"
#include "seahorse-servers.h"
#include "seahorse-util.h"

#include <ldap.h>

#ifdef WITH_SOUP
#include <libsoup/soup-address.h>
#endif

#ifdef WITH_LDAP

#define DEBUG_FLAG SEAHORSE_DEBUG_LDAP
#include "seahorse-debug.h"

/* Amount of keys to load in a batch */
#define DEFAULT_LOAD_BATCH 30

/* -----------------------------------------------------------------------------
 * SERVER INFO
 */
 
typedef struct _LDAPServerInfo {
    gchar *base_dn;             /* The base dn where PGP keys are found */
    gchar *key_attr;            /* The attribute of PGP key data */
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
                                                 

/* -----------------------------------------------------------------------------
 *  LDAP HELPERS
 */
 
#define LDAP_ERROR_DOMAIN (get_ldap_error_domain())

static gchar**
get_ldap_values (LDAP *ld, LDAPMessage *entry, const char *attribute)
{
    GArray *array;
    struct berval **bv;
    gchar *value;
    int num, i;

    bv = ldap_get_values_len (ld, entry, attribute);
    if (!bv)
        return NULL;

    array = g_array_new (TRUE, TRUE, sizeof (gchar*));
    num = ldap_count_values_len (bv);
    for(i = 0; i < num; i++) {
        value = g_strndup (bv[i]->bv_val, bv[i]->bv_len);
        g_array_append_val(array, value);
    }

    return (gchar**)g_array_free (array, FALSE);
}

#if WITH_DEBUG

static void
dump_ldap_entry (LDAP *ld, LDAPMessage *res)
{
    BerElement *pos;
    gchar **values;
    gchar **v;
    char *t;
    
    t = ldap_get_dn (ld, res);
    g_printerr ("dn: %s\n", t);
    ldap_memfree (t);
    
    for (t = ldap_first_attribute (ld, res, &pos); t; 
         t = ldap_next_attribute (ld, res, pos)) {
             
        values = get_ldap_values (ld, res, t);
        for (v = values; *v; v++) 
            g_printerr ("%s: %s\n", t, *v);
             
        g_strfreev (values);
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

static gchar*
get_string_attribute (LDAP *ld, LDAPMessage *res, const char *attribute)
{
    gchar **vals;
    gchar *v;
    
    vals = get_ldap_values (ld, res, attribute);
    if (!vals)
        return NULL; 
    v = vals[0] ? g_strdup (vals[0]) : NULL;
    g_strfreev (vals);
    return v;
}

static gboolean
get_boolean_attribute (LDAP* ld, LDAPMessage *res, const char *attribute)
{
    gchar **vals;
    gboolean b;
    
    vals = get_ldap_values (ld, res, attribute);
    if (!vals)
        return FALSE;
    b = vals[0] && atoi (vals[0]) == 1;
    g_strfreev (vals);
    return b;
}

static long int
get_int_attribute (LDAP* ld, LDAPMessage *res, const char *attribute)
{
    gchar **vals;
    long int d;
    
    vals = get_ldap_values (ld, res, attribute);
    if (!vals)
        return 0;
    d = vals[0] ? atoi (vals[0]) : 0;
    g_strfreev (vals);
    return d;         
}

static long int
get_date_attribute (LDAP* ld, LDAPMessage *res, const char *attribute)
{
    struct tm t;
    gchar **vals;
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

    g_strfreev (vals);
    return d;         
}

static const gchar*
get_algo_attribute (LDAP* ld, LDAPMessage *res, const char *attribute)
{
	const gchar *a = NULL;
	gchar **vals;
    
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
    
	g_strfreev (vals);
	return a;
}

/* 
 * Escapes a value so it's safe to use in an LDAP filter. Also trims
 * any spaces which cause problems with some LDAP servers.
 */
static gchar*
escape_ldap_value (const gchar *v)
{
    GString *value;
    gchar* result;
    
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
	gint cancelled_sig;
} SeahorseLdapGSource;

static gboolean
seahorse_ldap_gsource_prepare (GSource *gsource,
                               gint *timeout)
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
			return FALSE;

		/* Timeout */
		} else if (rc == 0) {
			return TRUE;
		}

		ret = ((SeahorseLdapCallback)callback) (result, user_data);
		ldap_msgfree (result);

		if (!ret)
			return FALSE;
	}

	return TRUE;
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
	gchar *server;

	if (rc == LDAP_SUCCESS)
		return FALSE;

	g_object_get (self, "key-server", &server, NULL);
	g_set_error (error, LDAP_ERROR_DOMAIN, rc, _("Couldn't communicate with '%s': %s"),
	             server, ldap_err2string (rc));
	g_free (server);

	return TRUE;
}

typedef struct {
	GCancellable *cancellable;
	LDAP *ldap;
} source_connect_closure;

static void
source_connect_free (gpointer data)
{
	source_connect_closure *closure = data;
	g_clear_object (&closure->cancellable);
	if (closure->ldap)
		ldap_unbind_ext (closure->ldap, NULL, NULL);
	g_free (closure);
}

static gboolean
on_connect_server_info_completed (LDAPMessage *result,
                                  gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_connect_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (g_async_result_get_source_object (user_data));
	LDAPServerInfo *sinfo;
	char *message;
	int code;
	int type;
	int rc;

	type = ldap_msgtype (result);
	g_return_val_if_fail (type == LDAP_RES_SEARCH_ENTRY || type == LDAP_RES_SEARCH_RESULT, FALSE);

	/* If we have results then fill in the server info */
	if (type == LDAP_RES_SEARCH_ENTRY) {

		seahorse_debug ("Server Info Result:");
#ifdef WITH_DEBUG
		if (seahorse_debugging)
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

		return TRUE; /* callback again */

	} else {
		rc = ldap_parse_result (closure->ldap, result, &code, NULL,
		                        &message, NULL, NULL, 0);
		g_return_val_if_fail (rc == LDAP_SUCCESS, FALSE);

		if (code != LDAP_SUCCESS)
			g_warning ("operation to get LDAP server info failed: %s", message);

		ldap_memfree (message);

		g_simple_async_result_complete_in_idle (res);
		seahorse_progress_end (closure->cancellable, res);
		return FALSE; /* don't callback again */
	}
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
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_connect_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (g_async_result_get_source_object (user_data));
	LDAPServerInfo *sinfo;
	GError *error = NULL;
	char *message;
	int ldap_op;
	int code;
	int rc;

	g_return_val_if_fail (ldap_msgtype (result) == LDAP_RES_BIND, FALSE);

	/* The result of the bind operation */
	rc = ldap_parse_result (closure->ldap, result, &code, NULL, &message, NULL, NULL, 0);
	g_return_val_if_fail (rc == LDAP_SUCCESS, FALSE);
	ldap_memfree (message);

	if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);
		return FALSE; /* don't call this callback again */
	}

	/* Check if we need server info */
	sinfo = get_ldap_server_info (self, FALSE);
	if (sinfo != NULL) {
		g_simple_async_result_complete_in_idle (res);
		seahorse_progress_end (closure->cancellable, res);
		return FALSE; /* don't call this callback again */
	}

	/* Retrieve the server info */
	rc = ldap_search_ext (closure->ldap, "cn=PGPServerInfo", LDAP_SCOPE_BASE,
	                      "(objectclass=*)", (char **)SERVER_ATTRIBUTES, 0,
	                      NULL, NULL, NULL, 0, &ldap_op);

	if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);
		return FALSE; /* don't call this callback again */

	} else {
		GSource *gsource = seahorse_ldap_gsource_new (closure->ldap, ldap_op,
		                                              closure->cancellable);
		g_source_set_callback (gsource, (GSourceFunc)on_connect_server_info_completed,
		                       g_object_ref (res), g_object_unref);
		g_source_attach (gsource, g_main_context_default ());
		g_source_unref (gsource);
	}

	return FALSE; /* don't call this callback again */
}

static void
once_resolved_start_connect (SeahorseLDAPSource *self,
                             GSimpleAsyncResult *res,
                             const gchar *address)
{
	source_connect_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	gchar *server = NULL;
	GError *error = NULL;
	gint port = LDAP_PORT;
	struct berval cred;
	int ldap_op;
	gchar *url;
	gchar *text;
	int rc;

	/* Now that we've resolved our address, connect via IP */
	g_object_get (self, "key-server", &server, NULL);
	g_return_if_fail (server && server[0]);

	if ((text = strchr (server, ':')) != NULL) {
		*text = 0;
		text++;
		port = atoi (text);
		if (port <= 0 || port >= G_MAXUINT16) {
			g_warning ("invalid port number: %s (using default)", text);
			port = LDAP_PORT;
		}
	}

	url = g_strdup_printf ("ldap://%s:%u", address, port);
	rc = ldap_initialize (&closure->ldap, url);
	g_free (url);

	if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);

	/* Start the bind operation */
	} else {
		cred.bv_val = "";
		cred.bv_len = 0;

		rc = ldap_sasl_bind (closure->ldap, NULL, LDAP_SASL_SIMPLE, &cred,
		                     NULL, NULL, &ldap_op);
		if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
			g_simple_async_result_take_error (res, error);
			g_simple_async_result_complete_in_idle (res);

		} else {
			GSource *gsource = seahorse_ldap_gsource_new (closure->ldap, ldap_op,
			                                     closure->cancellable);
			g_source_set_callback (gsource, (GSourceFunc)on_connect_bind_completed,
			                       g_object_ref (res), g_object_unref);
			g_source_attach (gsource, g_main_context_default ());
			g_source_unref (gsource);
		}
	}
}

#ifdef WITH_SOUP

static void
on_address_resolved_complete (SoupAddress *address,
                              guint status,
                              gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (g_async_result_get_source_object (user_data));
	source_connect_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	gchar *server;

	g_object_get (self, "key-server", &server, NULL);
	g_return_if_fail (server && server[0]);
	seahorse_progress_update (closure->cancellable, res, _("Connecting to: %s"), server);
	g_free (server);

	/* DNS failed */
	if (!SOUP_STATUS_IS_SUCCESSFUL (status)) {
		g_simple_async_result_set_error (res, SEAHORSE_ERROR, -1,
		                                 _("Couldn't resolve address: %s"),
		                                 soup_address_get_name (address));
		g_simple_async_result_complete_in_idle (res);

	/* Yay resolved */
	} else {
		once_resolved_start_connect (self, res, soup_address_get_physical (address));
	}

	g_object_unref (res);
}

#endif /* WITH_SOUP */

static void
seahorse_ldap_source_connect_async (SeahorseLDAPSource *source,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
	GSimpleAsyncResult *res;
	source_connect_closure *closure;
	gchar *server = NULL;
	gchar *pos;
#ifdef WITH_SOUP
	SoupAddress *address;
#endif

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_ldap_source_connect_async);
	closure = g_new0 (source_connect_closure, 1);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	g_simple_async_result_set_op_res_gpointer (res, closure, source_connect_free);

	g_object_get (source, "key-server", &server, NULL);
	g_return_if_fail (server && server[0]);
	if ((pos = strchr (server, ':')) != NULL)
		*pos = 0;

	seahorse_progress_prep_and_begin (cancellable, res, NULL);

	/* If we have libsoup, try and resolve asynchronously */
#ifdef WITH_SOUP
	address = soup_address_new (server, LDAP_PORT);
	seahorse_progress_update (cancellable, res, _("Resolving server address: %s"), server);

	soup_address_resolve_async (address, NULL, cancellable,
	                            on_address_resolved_complete,
	                            g_object_ref (res));
	g_object_unref (address);

#else /* !WITH_SOUP */

	once_resolved_start_connect (source, res, server);

#endif

	g_free (server);
	g_object_unref (res);
}

static LDAP *
seahorse_ldap_source_connect_finish (SeahorseLDAPSource *source,
                                     GAsyncResult *result,
                                     GError **error)
{
	source_connect_closure *closure;
	LDAP *ldap;

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_ldap_source_connect_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	closure = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
	ldap = closure->ldap;
	closure->ldap = NULL;
	return ldap;
}

G_DEFINE_TYPE (SeahorseLDAPSource, seahorse_ldap_source, SEAHORSE_TYPE_SERVER_SOURCE);

static void 
seahorse_ldap_source_init (SeahorseLDAPSource *self)
{

}

typedef struct {
	GCancellable *cancellable;
	gchar *filter;
	LDAP *ldap;
	GcrSimpleCollection *results;
} source_search_closure;

static void
source_search_free (gpointer data)
{
	source_search_closure *closure = data;
	g_clear_object (&closure->cancellable);
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
                                  GcrSimpleCollection *results,
                                  LDAP *ldap,
                                  LDAPMessage *res)
{
	const gchar *algo;
	long int timestamp;
	long int expires;
	gchar *fpr, *fingerprint;
	gchar *uidstr;
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
		SeahorsePgpSubkey *subkey;
		SeahorsePgpKey *key;
		SeahorsePgpUid *uid;
		GList *list;
		guint flags;

		/* Build up a subkey */
		subkey = seahorse_pgp_subkey_new ();
		seahorse_pgp_subkey_set_keyid (subkey, fpr);
		fingerprint = seahorse_pgp_subkey_calc_fingerprint (fpr);
		seahorse_pgp_subkey_set_fingerprint (subkey, fingerprint);
		g_free (fingerprint);
		seahorse_pgp_subkey_set_created (subkey, timestamp);
		seahorse_pgp_subkey_set_expires (subkey, expires);
		seahorse_pgp_subkey_set_algorithm (subkey, algo);
		seahorse_pgp_subkey_set_length (subkey, length);

		flags = SEAHORSE_FLAG_EXPORTABLE;
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

		/* Now build them into a key */
		list = g_list_prepend (NULL, uid);
		seahorse_pgp_key_set_uids (key, list);
		seahorse_object_list_free (list);
		list = g_list_prepend (NULL, subkey);
		seahorse_pgp_key_set_subkeys (key, list);
		seahorse_object_list_free (list);
		g_object_set (key,
		              "object-flags", flags,
		              "place", self,
		              NULL);

		seahorse_pgp_key_realize (key);
		gcr_simple_collection_add (results, G_OBJECT (key));
		g_object_unref (key);
	}

	g_free (fpr);
	g_free (uidstr);
}

static gboolean
on_search_search_completed (LDAPMessage *result,
                            gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_search_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (g_async_result_get_source_object (user_data));
	GError *error = NULL;
	char *message;
	int code;
	int type;
	int rc;

	type = ldap_msgtype (result);
	g_return_val_if_fail (type == LDAP_RES_SEARCH_ENTRY || type == LDAP_RES_SEARCH_RESULT, FALSE);

	/* An LDAP entry */
	if (type == LDAP_RES_SEARCH_ENTRY) {
		seahorse_debug ("Retrieved Key Entry:");
#ifdef WITH_DEBUG
		if (seahorse_debugging)
			dump_ldap_entry (closure->ldap, result);
#endif

		search_parse_key_from_ldap_entry (self, closure->results,
		                                  closure->ldap, result);
		return TRUE; /* keep calling this callback */

	/* All entries done */
	} else {
		rc = ldap_parse_result (closure->ldap, result, &code, NULL,
		                        &message, NULL, NULL, 0);
		g_return_val_if_fail (rc == LDAP_SUCCESS, FALSE);

		/* Error codes that we ignore */
		switch (code) {
		case LDAP_SIZELIMIT_EXCEEDED:
			code = LDAP_SUCCESS;
			break;
		};

		/* Failure */
		if (code != LDAP_SUCCESS)
			g_simple_async_result_set_error (res, LDAP_ERROR_DOMAIN,
			                                 code, "%s", message);
		else if (seahorse_ldap_source_propagate_error (self, code, &error))
			g_simple_async_result_take_error (res, error);

		ldap_memfree (message);
		seahorse_progress_end (closure->cancellable, res);
		g_simple_async_result_complete (res);
		return FALSE;
	}
}

static void
on_search_connect_completed (GObject *source,
                             GAsyncResult *result,
                             gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_search_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (source);
	GError *error = NULL;
	LDAPServerInfo *sinfo;
	int ldap_op;
	int rc;

	closure->ldap = seahorse_ldap_source_connect_finish (SEAHORSE_LDAP_SOURCE (source),
	                                                     result, &error);
	if (error != NULL) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);
		g_object_unref (res);
		return;
	}

	sinfo = get_ldap_server_info (self, TRUE);

	seahorse_debug ("Searching Server ... base: %s, filter: %s",
	                sinfo->base_dn, closure->filter);

	rc = ldap_search_ext (closure->ldap, sinfo->base_dn, LDAP_SCOPE_SUBTREE,
	                      closure->filter, (char **)PGP_ATTRIBUTES, 0,
	                      NULL, NULL, NULL, 0, &ldap_op);

	if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);

	} else {
		GSource *gsource = seahorse_ldap_gsource_new (closure->ldap, ldap_op,
		                                     closure->cancellable);
		g_source_set_callback (gsource, (GSourceFunc)on_search_search_completed,
		                       g_object_ref (res), g_object_unref);
		g_source_attach (gsource, g_main_context_default ());
		g_source_unref (gsource);
	}

	g_object_unref (res);
}


static void
seahorse_ldap_source_search_async (SeahorseServerSource *source,
                                   const gchar *match,
                                   GcrSimpleCollection *results,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
	SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (source);
	source_search_closure *closure;
	GSimpleAsyncResult *res;
	gchar *text;

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_ldap_source_search_async);
	closure = g_new0 (source_search_closure, 1);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	closure->results = g_object_ref (results);
	text = escape_ldap_value (match);
	closure->filter = g_strdup_printf ("(pgpuserid=*%s*)", text);
	g_free (text);
	g_simple_async_result_set_op_res_gpointer (res, closure, source_search_free);

	seahorse_progress_prep_and_begin (closure->cancellable, res, NULL);

	seahorse_ldap_source_connect_async (self, cancellable,
	                                    on_search_connect_completed,
	                                    g_object_ref (res));

	g_object_unref (res);
}

static gboolean
seahorse_ldap_source_search_finish (SeahorseServerSource *source,
                                    GAsyncResult *result,
                                    GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_ldap_source_search_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

typedef struct {
	GPtrArray *keydata;
	gint current_index;
	GCancellable *cancellable;
	LDAP *ldap;
} source_import_closure;

static void
source_import_free (gpointer data)
{
	source_import_closure *closure = data;
	g_ptr_array_free (closure->keydata, TRUE);
	g_clear_object (&closure->cancellable);
	if (closure->ldap)
		ldap_unbind_ext (closure->ldap, NULL, NULL);
	g_free (closure);
}

static void       import_send_key       (SeahorseLDAPSource *self,
                                         GSimpleAsyncResult *res);

/* Called when results come in for a key send */
static gboolean
on_import_add_completed (LDAPMessage *result,
                         gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_import_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (g_async_result_get_source_object (user_data));
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
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);
		return FALSE; /* don't call for this source again */
	}

	import_send_key (self, res);
	return FALSE; /* don't call for this source again */
}

static void
import_send_key (SeahorseLDAPSource *self,
                 GSimpleAsyncResult *res)
{
	source_import_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	LDAPServerInfo *sinfo;
	gchar *base;
	LDAPMod mod;
	LDAPMod *attrs[2];
	char *values[2];
	GSource *gsource;
	GError *error = NULL;
	gchar *keydata;
	int ldap_op;
	int rc;

	if (closure->current_index >= 0) {
		keydata = closure->keydata->pdata[closure->current_index];
		seahorse_progress_end (closure->cancellable, keydata);
	}

	closure->current_index++;

	/* All done, complete operation */
	if (closure->current_index == (gint)closure->keydata->len) {
		g_simple_async_result_complete (res);
		return;
	}

	keydata = closure->keydata->pdata[closure->current_index];
	seahorse_progress_begin (closure->cancellable, keydata);
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

	g_free (base);

	if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
		g_simple_async_result_complete (res);
		return;
	}

	gsource = seahorse_ldap_gsource_new (closure->ldap, ldap_op,
	                                     closure->cancellable);
	g_source_set_callback (gsource, (GSourceFunc)on_import_add_completed,
	                       g_object_ref (res), g_object_unref);
	g_source_attach (gsource, g_main_context_default ());
	g_source_unref (gsource);
}

static void
on_import_connect_completed (GObject *source,
                             GAsyncResult *result,
                             gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_import_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	closure->ldap = seahorse_ldap_source_connect_finish (SEAHORSE_LDAP_SOURCE (source),
	                                                     result, &error);
	if (error != NULL) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);
	} else {
		import_send_key (SEAHORSE_LDAP_SOURCE (source), res);
	}

	g_object_unref (res);
}

static void
seahorse_ldap_source_import_async (SeahorseServerSource *source,
                                   GInputStream *input,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
	SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (source);
	source_import_closure *closure;
	GSimpleAsyncResult *res;
	gchar *keydata;
	guint len;

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_ldap_source_import_async);
	closure = g_new0 (source_import_closure, 1);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	closure->current_index = -1;
	g_simple_async_result_set_op_res_gpointer (res, closure, source_import_free);

	closure->keydata =g_ptr_array_new_with_free_func (g_free);
	for (;;) {
		GString *buf = g_string_sized_new (2048);
		len = seahorse_util_read_data_block (buf, input, "-----BEGIN PGP PUBLIC KEY BLOCK-----",
		                                     "-----END PGP PUBLIC KEY BLOCK-----");
		if (len > 0) {
			keydata = g_string_free (buf, FALSE);
			g_ptr_array_add (closure->keydata, keydata);
			seahorse_progress_prep (closure->cancellable, keydata, NULL);
		} else {
			g_string_free (buf, TRUE);
			break;
		}
	}

	seahorse_ldap_source_connect_async (self, cancellable,
	                                    on_import_connect_completed,
	                                    g_object_ref (res));

	g_object_unref (res);
}

static GList *
seahorse_ldap_source_import_finish (SeahorseServerSource *source,
                                    GAsyncResult *result,
                                    GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_ldap_source_import_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	/* We don't know the keys that were imported, since this is a server */
	return NULL;
}

typedef struct {
	GPtrArray *fingerprints;
	gint current_index;
	GString *data;
	GCancellable *cancellable;
	LDAP *ldap;
} ExportClosure;

static void
export_closure_free (gpointer data)
{
	ExportClosure *closure = data;
	g_ptr_array_free (closure->fingerprints, TRUE);
	if (closure->data)
		g_string_free (closure->data, TRUE);
	g_clear_object (&closure->cancellable);
	if (closure->ldap)
		ldap_unbind_ext (closure->ldap, NULL, NULL);
	g_free (closure);
}

static void     export_retrieve_key     (SeahorseLDAPSource *self,
                                         GSimpleAsyncResult *res);

static gboolean
on_export_search_completed (LDAPMessage *result,
                            gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	ExportClosure *closure = g_simple_async_result_get_op_res_gpointer (res);
	SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (g_async_result_get_source_object (G_ASYNC_RESULT (res)));
	LDAPServerInfo *sinfo;
	char *message;
	GError *error = NULL;
	gchar *key;
	int code;
	int type;
	int rc;

	type = ldap_msgtype (result);
	g_return_val_if_fail (type == LDAP_RES_SEARCH_ENTRY || type == LDAP_RES_SEARCH_RESULT, FALSE);
	sinfo = get_ldap_server_info (self, TRUE);

	/* An LDAP Entry */
	if (type == LDAP_RES_SEARCH_ENTRY) {

		seahorse_debug ("Server Info Result:");
#ifdef WITH_DEBUG
		if (seahorse_debugging)
			dump_ldap_entry (closure->ldap, result);
#endif

		key = get_string_attribute (closure->ldap, result, sinfo->key_attr);

		if (key == NULL) {
			g_warning ("key server missing pgp key data");
			seahorse_ldap_source_propagate_error (self, LDAP_NO_SUCH_OBJECT, &error);
			g_simple_async_result_take_error (res, error);
			g_simple_async_result_complete (res);
			return FALSE;
		}

		g_string_append (closure->data, key);
		g_string_append_c (closure->data, '\n');

		g_free (key);
		return TRUE;

	/* No more entries, result */
	} else {
		rc = ldap_parse_result (closure->ldap, result, &code, NULL,
		                        &message, NULL, NULL, 0);
		g_return_val_if_fail (rc == LDAP_SUCCESS, FALSE);

		if (seahorse_ldap_source_propagate_error (self, code, &error)) {
			g_simple_async_result_take_error (res, error);
			g_simple_async_result_complete (res);
			return FALSE;
		}

		ldap_memfree (message);

		/* Process more keys if possible */
		export_retrieve_key (self, res);
		return FALSE;
	}
}

static void
export_retrieve_key (SeahorseLDAPSource *self,
                     GSimpleAsyncResult *res)
{
	ExportClosure *closure = g_simple_async_result_get_op_res_gpointer (res);
	LDAPServerInfo *sinfo;
	gchar *filter;
	char *attrs[2];
	GSource *gsource;
	const gchar *fingerprint;
	GError *error = NULL;
	int length, rc;
	int ldap_op;

	if (closure->current_index > 0) {
		fingerprint = closure->fingerprints->pdata[closure->current_index];
		seahorse_progress_end (closure->cancellable, fingerprint);
	}

	closure->current_index++;

	/* All done, complete operation */
	if (closure->current_index == (gint)closure->fingerprints->len) {
		g_simple_async_result_complete (res);
		return;
	}

	fingerprint = closure->fingerprints->pdata[closure->current_index];
	seahorse_progress_begin (closure->cancellable, fingerprint);
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
	g_free (filter);

	if (seahorse_ldap_source_propagate_error (self, rc, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);
		return;
	}

	gsource = seahorse_ldap_gsource_new (closure->ldap, ldap_op,
	                                     closure->cancellable);
	g_source_set_callback (gsource, (GSourceFunc)on_export_search_completed,
	                       g_object_ref (res), g_object_unref);
	g_source_attach (gsource, g_main_context_default ());
	g_source_unref (gsource);
}

static void
on_export_connect_completed (GObject *source,
                             GAsyncResult *result,
                             gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	ExportClosure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	closure->ldap = seahorse_ldap_source_connect_finish (SEAHORSE_LDAP_SOURCE (source),
	                                                     result, &error);
	if (error != NULL) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);
	} else {
		export_retrieve_key (SEAHORSE_LDAP_SOURCE (source), res);
	}

	g_object_unref (res);
}

static void
seahorse_ldap_source_export_async (SeahorseServerSource *source,
                                   const gchar **keyids,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
	SeahorseLDAPSource *self = SEAHORSE_LDAP_SOURCE (source);
	ExportClosure *closure;
	GSimpleAsyncResult *res;
	gchar *fingerprint;
	gint i;

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_ldap_source_export_async);
	closure = g_new0 (ExportClosure, 1);
	closure->data = g_string_sized_new (1024);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	closure->fingerprints = g_ptr_array_new_with_free_func (g_free);
	for (i = 0; keyids[i] != NULL; i++) {
		fingerprint = g_strdup (keyids[i]);
		g_ptr_array_add (closure->fingerprints, fingerprint);
		seahorse_progress_prep (closure->cancellable, fingerprint, NULL);
	}
	closure->current_index = -1;
	g_simple_async_result_set_op_res_gpointer (res, closure, export_closure_free);

	seahorse_ldap_source_connect_async (self, cancellable,
	                                    on_export_connect_completed,
	                                    g_object_ref (res));

	g_object_unref (res);
}

static gpointer
seahorse_ldap_source_export_finish (SeahorseServerSource *source,
                                    GAsyncResult *result,
                                    gsize *size,
                                    GError **error)
{
	ExportClosure *closure;
	gpointer output;

	g_return_val_if_fail (size != NULL, NULL);
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_ldap_source_export_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	closure = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
	*size = closure->data->len;
	output = g_string_free (closure->data, FALSE);
	closure->data = NULL;
	return output;
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

	seahorse_servers_register_type ("ldap", _("LDAP Key Server"), seahorse_ldap_is_valid_uri);
}

/**
 * seahorse_ldap_source_new
 * @uri: The server to connect to 
 * 
 * Creates a new key source for an LDAP PGP server.
 * 
 * Returns: A new LDAP Key Source
 */
SeahorseLDAPSource*   
seahorse_ldap_source_new (const gchar* uri, const gchar *host)
{
    g_return_val_if_fail (seahorse_ldap_is_valid_uri (uri), NULL);
    g_return_val_if_fail (host && *host, NULL);
    return g_object_new (SEAHORSE_TYPE_LDAP_SOURCE, "key-server", host, 
                         "uri", uri, NULL);
}

/**
 * seahorse_ldap_is_valid_uri
 * @uri: The uri to check
 * 
 * Returns: Whether the passed uri is valid for an ldap key source
 */
gboolean              
seahorse_ldap_is_valid_uri (const gchar *uri)
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
