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
#include <libintl.h>
#include <gnome.h>
#include <ldap.h>

#include "config.h"
#include "seahorse-gpgmex.h"
#include "seahorse-operation.h"
#include "seahorse-ldap-source.h"
#include "seahorse-util.h"
#include "seahorse-pgp-key.h"

#ifdef WITH_LDAP

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

#ifdef _DEBUG

static void
dump_ldap_entry (LDAP *ld, LDAPMessage *res)
{
    BerElement *pos;
    char **values;
    char **v;
    char *t;
    
    t = ldap_get_dn (ld, res);
    g_printerr ("dn: %s\n", t);
    ldap_memfree (t);
    
    for (t = ldap_first_attribute (ld, res, &pos); t; 
         t = ldap_next_attribute (ld, res, pos)) {
             
        values = ldap_get_values (ld, res, t);
        for (v = values; *v; v++) 
            g_printerr ("%s: %s\n", t, *v);
             
        ldap_value_free (values);
        ldap_memfree (t);
    }
    
    ber_free (pos, 0);
}

#endif

static GQuark
get_ldap_error_domain ()
{
    static GQuark q = 0;
    if(q == 0)
        q = g_quark_from_static_string ("seahorse-ldap-error");
    return q;
}

static gchar*
get_string_attribute (LDAP* ld, LDAPMessage *res, const char *attribute)
{
    char **vals;
    gchar *v;
    
    vals = ldap_get_values (ld, res, attribute);
    if (!vals)
        return NULL; 
    v = vals[0] ? g_strdup (vals[0]) : NULL;
    ldap_value_free (vals);
    return v;
}

static gboolean
get_boolean_attribute (LDAP* ld, LDAPMessage *res, const char *attribute)
{
    char **vals;
    gboolean b;
    
    vals = ldap_get_values (ld, res, attribute);
    if (!vals)
        return FALSE;
    b = vals[0] && atoi (vals[0]) == 1;
    ldap_value_free (vals);
    return b;
}

static long int
get_int_attribute (LDAP* ld, LDAPMessage *res, const char *attribute)
{
    char **vals;
    long int d;
    
    vals = ldap_get_values (ld, res, attribute);
    if (!vals)
        return 0;
    d = vals[0] ? atoi (vals[0]) : 0;
    ldap_value_free (vals);
    return d;         
}

static long int
get_date_attribute (LDAP* ld, LDAPMessage *res, const char *attribute)
{
    struct tm t;
    char **vals;
    long int d;
    
    vals = ldap_get_values (ld, res, attribute);
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

    ldap_value_free (vals);
    return d;         
}

static gpgme_pubkey_algo_t
get_algo_attribute (LDAP* ld, LDAPMessage *res, const char *attribute)
{
    gpgme_pubkey_algo_t a = 0;
    char **vals;
    
    vals = ldap_get_values (ld, res, attribute);
    if (!vals)
        return 0;
    
    if (vals[0]) {
        if (g_ascii_strcasecmp (vals[0], "DH/DSS") == 0 || 
            g_ascii_strcasecmp (vals[0], "Elg") == 0 ||
            g_ascii_strcasecmp (vals[0], "Elgamal") == 0)
            a = GPGME_PK_ELG;
        if (g_ascii_strcasecmp (vals[0], "RSA") == 0)
            a = GPGME_PK_RSA;
        if (g_ascii_strcasecmp (vals[0], "DSA") == 0)
            a = GPGME_PK_DSA;     
    }
    
    ldap_value_free (vals);
    return a;
}

/* Escapes a value so it's safe to use in an LDAP filter */
static gchar*
escape_ldap_value (const gchar *v)
{
    GString *value;
    
    g_return_val_if_fail (v, "");
    value = g_string_sized_new (strlen(v));
    
    for ( ; *v; v++) {
        switch(*v) {
        case ' ': case '#': case ',': case '+': case '\\':
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
    
    return g_string_free (value, FALSE);
}

/* -----------------------------------------------------------------------------
 *  LDAP OPERATION     
 */
 
#define SEAHORSE_TYPE_LDAP_OPERATION            (seahorse_ldap_operation_get_type ())
#define SEAHORSE_LDAP_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_LDAP_OPERATION, SeahorseLDAPOperation))
#define SEAHORSE_LDAP_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_LDAP_OPERATION, SeahorseLDAPOperationClass))
#define SEAHORSE_IS_LDAP_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_LDAP_OPERATION))
#define SEAHORSE_IS_LDAP_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_LDAP_OPERATION))
#define SEAHORSE_LDAP_OPERATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_LDAP_OPERATION, SeahorseLDAPOperationClass))

typedef gboolean (*OpLDAPCallback)   (SeahorseOperation *op, LDAPMessage *result);
    
DECLARE_OPERATION (LDAP, ldap)
    SeahorseLDAPSource *lsrc;       /* The source */
    LDAP *ldap;                     /* The LDAP connection */
    int ldap_op;                    /* The current LDAP async msg */
    guint stag;                     /* The tag for the idle event source */
    OpLDAPCallback ldap_cb;         /* Callback for next async result */
    OpLDAPCallback chain_cb;        /* Callback when connection is done */
END_DECLARE_OPERATION

IMPLEMENT_OPERATION (LDAP, ldap)


/* -----------------------------------------------------------------------------
 *  SEARCH OPERATION STUFF 
 */
 
static const char *kServerAttributes[] = {
    "basekeyspacedn",
    "pgpbasekeyspacedn",
    "version",
    NULL
};

static void 
seahorse_ldap_operation_init (SeahorseLDAPOperation *sop)
{
    sop->ldap_op = -1;
}

static void 
seahorse_ldap_operation_dispose (GObject *gobject)
{
    SeahorseLDAPOperation *lop = SEAHORSE_LDAP_OPERATION (gobject);
    
    if (lop->lsrc) {
        g_object_unref (lop->lsrc);
        lop->lsrc = NULL;
    }

    if (lop->ldap) {
        ldap_unbind (lop->ldap);
        lop->ldap = NULL;
    }
    
    if (lop->stag) {
        g_source_remove (lop->stag);
        lop->stag = 0;
    }
        
    G_OBJECT_CLASS (operation_parent_class)->dispose (gobject);  
}

static void 
seahorse_ldap_operation_finalize (GObject *gobject)
{
    SeahorseLDAPOperation *lop = SEAHORSE_LDAP_OPERATION (gobject);

    g_assert (lop->lsrc == NULL);
    g_assert (lop->ldap_op == -1);
    g_assert (lop->stag == 0);
    g_assert (lop->ldap == NULL);
    
    G_OBJECT_CLASS (operation_parent_class)->finalize (gobject);  
}

static void 
seahorse_ldap_operation_cancel (SeahorseOperation *operation)
{
    SeahorseLDAPOperation *lop;
    
    g_return_if_fail (SEAHORSE_IS_LDAP_OPERATION (operation));
    lop = SEAHORSE_LDAP_OPERATION (operation);
    
    if (lop->ldap_op != -1) {
        if (lop->ldap)
            ldap_abandon (lop->ldap, lop->ldap_op);
        lop->ldap_op = -1;
    }

    if (lop->ldap) {
        ldap_unbind (lop->ldap);
        lop->ldap = NULL;
    }

    if (lop->stag) {
        g_source_remove (lop->stag);
        lop->stag = 0;
    }
        
    seahorse_operation_mark_done (operation, TRUE, NULL);
}

/* Cancels operation and marks the LDAP operation as failed */
static void
fail_ldap_operation (SeahorseLDAPOperation *lop, int code)
{
    gchar *t;
    
    if (code == 0)
        ldap_get_option (lop->ldap, LDAP_OPT_ERROR_NUMBER, &code);
    
    g_object_get (lop->lsrc, "key-server", &t, NULL);
    seahorse_operation_mark_done (SEAHORSE_OPERATION (lop), FALSE, 
            g_error_new (LDAP_ERROR_DOMAIN, code, _("Couldn't communicate with '%s': %s"), 
                         t, ldap_err2string(code)));
    g_free (t);
}

/* Gets called regularly to check for results of LDAP async work */
static gboolean
result_callback (SeahorseLDAPOperation *lop)
{
    struct timeval timeout;
    LDAPMessage *result;
    gboolean ret;
    int r, i;
    
    g_return_val_if_fail (SEAHORSE_IS_LDAP_OPERATION (lop), FALSE);
    g_return_val_if_fail (lop->ldap != NULL, FALSE);
    g_return_val_if_fail (lop->ldap_op != -1, FALSE);
    
    for (i = 0; i < DEFAULT_LOAD_BATCH; i++) {
     
        /* This effects a poll */
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
    
        r = ldap_result (lop->ldap, lop->ldap_op, 0, &timeout, &result);
        switch (r) {   
        case -1: /* Scary error */
            g_return_val_if_reached (FALSE);
            break;
        case 0: /* Timeout exceeded */
            return TRUE;
        };
        
        ret = (lop->ldap_cb) (SEAHORSE_OPERATION (lop), result);
        ldap_msgfree (result);
        
        if(!ret)
            break;
    }

    /* We can't access lop at this point if not continuing. 
     * It could have been freed */
    if (ret) {
        /* We always need a callback if we're continuing */
        g_assert (lop->ldap_cb);

        /* Should not be marked as done. */
        g_assert (!seahorse_operation_is_done (SEAHORSE_OPERATION (lop)));
    }    
        
    return ret;
}

/* Called when retrieving server info is done, and we need to start work */
static gboolean
done_info_start_op (SeahorseOperation *op, LDAPMessage *result)
{
    SeahorseLDAPOperation *lop = SEAHORSE_LDAP_OPERATION (op);
    LDAPServerInfo *sinfo;
    char *message;
    int code;
    int r;
    
    g_return_val_if_fail (SEAHORSE_IS_LDAP_OPERATION (lop), FALSE);

    /* This can be null when we short-circuit the server info */
    if (result) {
        r = ldap_msgtype (result);
        g_return_val_if_fail (r == LDAP_RES_SEARCH_ENTRY || r == LDAP_RES_SEARCH_RESULT, FALSE);
        
        /* If we have results then fill in the server info */
        if (r == LDAP_RES_SEARCH_ENTRY) {
#ifdef _DEBUG
            g_printerr ("[ldap] Server Info Result:\n");
            dump_ldap_entry (lop->ldap, result);
#endif      
            /* NOTE: When adding attributes here make sure to add them to kServerAttributes */
            sinfo = g_new0 (LDAPServerInfo, 1);
            sinfo->version = get_int_attribute (lop->ldap, result, "version");
            sinfo->base_dn = get_string_attribute (lop->ldap, result, "basekeyspacedn");
            if (!sinfo->base_dn)
                sinfo->base_dn = get_string_attribute (lop->ldap, result, "pgpbasekeyspacedn");
            sinfo->key_attr = g_strdup (sinfo->version > 1 ? "pgpkeyv2" : "pgpkey");
            set_ldap_server_info (lop->lsrc, sinfo);
            
            ldap_abandon (lop->ldap, lop->ldap_op);
            lop->ldap_op = -1;
            
        } else {
            lop->ldap_op = -1;
            r = ldap_parse_result (lop->ldap, result, &code, NULL, &message, NULL, NULL, 0);
            g_return_val_if_fail (r == LDAP_SUCCESS, FALSE);

            if (code != LDAP_SUCCESS) 
                g_warning ("operation to get LDAP server info failed: %s", message);
            
            ldap_memfree (message);
        }
    }
    
    /* Call the main operation callback */
    return (lop->chain_cb) (op, NULL);
}

/* Called when LDAP bind is done, and we need to retrieve server info */
static gboolean
done_bind_start_info (SeahorseOperation *op, LDAPMessage *result)
{
    SeahorseLDAPOperation *lop = SEAHORSE_LDAP_OPERATION (op);
    LDAPServerInfo *sinfo;
    char *message;
    int code;
    int r;
    
    /* Always do this first, because we're done with the 
     * operation regardless of the result */
    lop->ldap_op = -1;

    g_return_val_if_fail (SEAHORSE_IS_LDAP_OPERATION (lop), FALSE);
    g_return_val_if_fail (result != NULL, FALSE);
    g_return_val_if_fail (ldap_msgtype (result) == LDAP_RES_BIND, FALSE);

    /* The result of the bind operation */
    r = ldap_parse_result (lop->ldap, result, &code, NULL, &message, NULL, NULL, 0);
    g_return_val_if_fail (r == LDAP_SUCCESS, FALSE);

    if (code != LDAP_SUCCESS) {
        seahorse_operation_mark_done (op, FALSE, 
                g_error_new_literal (LDAP_ERROR_DOMAIN, code, message));
        return FALSE;
    }

    ldap_memfree (message);
     
    /* Check if we need server info */
    sinfo = get_ldap_server_info (lop->lsrc, FALSE);
    if (sinfo != NULL)
        return done_info_start_op (op, NULL);
        
    /* Retrieve the server info */
    lop->ldap_op = ldap_search (lop->ldap, "cn=PGPServerInfo", LDAP_SCOPE_BASE,
                                "(objectclass=*)", (char**)kServerAttributes, 0);    
    if (lop->ldap_op == -1) {
        fail_ldap_operation (lop, 0);
        return FALSE;
    }

    lop->ldap_cb = done_info_start_op;
    return TRUE;
}

/* Start an LDAP (bind, server info) request */
static SeahorseLDAPOperation*
seahorse_ldap_operation_start (SeahorseLDAPSource *lsrc, OpLDAPCallback cb,
                               guint total)
{
    SeahorseLDAPOperation *lop;
    gchar *server = NULL;
    gchar *t;

    g_return_val_if_fail (SEAHORSE_IS_LDAP_SOURCE (lsrc), NULL);
    
    lop = g_object_new (SEAHORSE_TYPE_LDAP_OPERATION, NULL);
    lop->lsrc = lsrc;
    g_object_ref (lsrc);
    
    g_object_get (lsrc, "key-server", &server, NULL);
    g_return_val_if_fail (server && server[0], NULL);
    
    lop->ldap = ldap_init (server, LDAP_PORT);
    g_return_val_if_fail (lop->ldap != NULL, NULL);

    lop->ldap_cb = done_bind_start_info;
    lop->chain_cb = cb;

    seahorse_operation_mark_start (SEAHORSE_OPERATION (lop));
    
    t = g_strdup_printf (_("Connecting to: %s"), server);
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (lop), t, 0, total);
    g_free (t);

    g_free (server);
    
    /* Start the bind operation */
    lop->ldap_op = ldap_simple_bind (lop->ldap, NULL, NULL);
    if (lop->ldap_op == -1) 
        fail_ldap_operation (lop, 0);
        
    else   /* This starts looking for results */
        lop->stag = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, 
                        (GSourceFunc)result_callback, lop, NULL);
        
    return lop;
}

/* -----------------------------------------------------------------------------
 *  SEARCH OPERATION 
 */

static const char *kPGPAttributes[] = {
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
parse_key_from_ldap_entry (SeahorseLDAPOperation *lop, LDAPMessage *res)
{
    gpgme_pubkey_algo_t algo;
    long int timestamp;
    long int expires;
    gpgme_key_t key;
    gchar *fpr;
    gchar *uid;
    guint flags = 0;
    int length;
        
    g_return_if_fail (SEAHORSE_IS_LDAP_OPERATION (lop));
    g_return_if_fail (res && ldap_msgtype (res) == LDAP_RES_SEARCH_ENTRY);  
    
    fpr = get_string_attribute (lop->ldap, res, "pgpcertid");
    uid = get_string_attribute (lop->ldap, res, "pgpuserid");
    flags |= (get_boolean_attribute (lop->ldap, res, "pgprevoked") ? GPGMEX_KEY_REVOKED : 0);
    flags |= (get_boolean_attribute (lop->ldap, res, "pgpdisabled") ? GPGMEX_KEY_DISABLED : 0);
    timestamp = get_date_attribute (lop->ldap, res, "pgpkeycreatetime");
    expires = get_date_attribute (lop->ldap, res, "pgpkeyexpiretime");
    algo = get_algo_attribute (lop->ldap, res, "pgpkeytype");
    length = get_int_attribute (lop->ldap, res, "pgpkeysize");
    
    if (fpr && uid) {
        key = gpgmex_key_alloc ();
        gpgmex_key_add_subkey (key, fpr, flags, timestamp, 
                               expires, length, algo);
        gpgmex_key_add_uid (key, uid, flags);
        
        seahorse_server_source_add_key (SEAHORSE_SERVER_SOURCE (lop->lsrc), key);
        gpgmex_key_unref (key);
    }
    
    g_free (fpr);
    g_free (uid);
}

/* Got a search result */
static gboolean 
search_entry (SeahorseOperation *op, LDAPMessage *result)
{
    SeahorseLDAPOperation *lop = SEAHORSE_LDAP_OPERATION (op);
    char *message;
    int code;
    int r;
  
    r = ldap_msgtype (result);
    g_return_val_if_fail (r == LDAP_RES_SEARCH_ENTRY || r == LDAP_RES_SEARCH_RESULT, FALSE);
     
    /* An LDAP entry */
    if (r == LDAP_RES_SEARCH_ENTRY) {
#ifdef _DEBUG
            g_printerr ("[ldap] Retrieved Key Entry:\n");
            dump_ldap_entry (lop->ldap, result);
#endif      
        parse_key_from_ldap_entry (lop, result);
        return TRUE;
        
    /* All entries done */
    } else {
        lop->ldap_op = -1;
        r = ldap_parse_result (lop->ldap, result, &code, NULL, &message, NULL, NULL, 0);
        g_return_val_if_fail (r == LDAP_SUCCESS, FALSE);
        
        /* Error codes that we ignore */
        switch (code) {
        case LDAP_SIZELIMIT_EXCEEDED:
            code = LDAP_SUCCESS;
            break;
        };
        
        /* Failure */
        if (code != LDAP_SUCCESS) {
            if (!message || !message[0]) 
                fail_ldap_operation (lop, code);
            else
                seahorse_operation_mark_done (SEAHORSE_OPERATION (lop), FALSE, 
                            g_error_new_literal (LDAP_ERROR_DOMAIN, code, message));
            
        /* Success */
        } else {
            seahorse_operation_mark_done (SEAHORSE_OPERATION (lop), FALSE, NULL);
        }
        ldap_memfree (message);

        return FALSE;
    }       
}

/* Performs a search on an open LDAP connection */
static gboolean
start_search (SeahorseOperation *op, LDAPMessage *result)
{
    SeahorseLDAPOperation *lop = SEAHORSE_LDAP_OPERATION (op);  
    LDAPServerInfo *sinfo;
    gchar *filter, *t;
    
    g_return_val_if_fail (lop->ldap != NULL, FALSE);
    g_assert (lop->ldap_op == -1);
    
    filter = (gchar*)g_object_get_data (G_OBJECT (lop), "filter");
    g_return_val_if_fail (filter != NULL, FALSE);

    t = (gchar*)g_object_get_data (G_OBJECT (lop), "details");
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (lop), t, 0, 0);
    
    sinfo = get_ldap_server_info (lop->lsrc, TRUE);
    lop->ldap_op = ldap_search (lop->ldap, sinfo->base_dn, LDAP_SCOPE_SUBTREE,
                                filter, (char**)kPGPAttributes, 0);
    if (lop->ldap_op == -1) {
        fail_ldap_operation (lop, 0);
        return FALSE;
    }                                    
    
    lop->ldap_cb = search_entry;
    return TRUE;                                
}

/* Initiate a serch operation by uid  */
static SeahorseLDAPOperation*
start_search_operation (SeahorseLDAPSource *lsrc, const gchar *pattern)
{
    SeahorseLDAPOperation *lop;
    gchar *filter;
    gchar *t;
    
    g_return_val_if_fail (pattern && pattern[0], NULL);
    
    t = escape_ldap_value (pattern);
    filter = g_strdup_printf ("(pgpuserid=*%s*)", t);
    g_free (t);
    
    lop = seahorse_ldap_operation_start (lsrc, start_search, 0);
    g_return_val_if_fail (lop != NULL, NULL);
    
    g_object_set_data_full (G_OBJECT (lop), "filter", filter, g_free);
    
    t = g_strdup_printf (_("Searching for keys containing '%s'..."), pattern);
    g_object_set_data_full (G_OBJECT (lop), "details", t, g_free);
    
    return lop;
}

/* Initiate a search operation by fingerprint */
static SeahorseLDAPOperation *
start_search_operation_fpr (SeahorseLDAPSource *lsrc, const gchar *fpr)
{
    SeahorseLDAPOperation *lop;
    gchar *filter, *t;
    guint l;
    
    g_return_val_if_fail (fpr && fpr[0], NULL);
    
    l = strlen (fpr);
    if (l > 16)
        fpr += (l - 16);
    
    filter = g_strdup_printf ("(pgpcertid=%.16s)", fpr);
    
    lop = seahorse_ldap_operation_start (lsrc, start_search, 1);
    g_return_val_if_fail (lop != NULL, NULL);
    
    g_object_set_data_full (G_OBJECT (lop), "filter", filter, g_free);
    
    t = g_strdup_printf (_("Searching for key id '%s'..."), fpr);
    g_object_set_data_full (G_OBJECT (lop), "details", t, g_free);
    
    return lop;
}

/* -----------------------------------------------------------------------------
 *  GET OPERATION 
 */
 
static gboolean get_key_from_ldap (SeahorseOperation *op, LDAPMessage *result);

/* Called when results come in from a key get */
static gboolean 
get_callback (SeahorseOperation *op, LDAPMessage *result)
{
    SeahorseLDAPOperation *lop = SEAHORSE_LDAP_OPERATION (op);  
    LDAPServerInfo *sinfo;
    gpgme_data_t data;
    char *message;
    gchar *key;
    int code;
    int r;
  
    r = ldap_msgtype (result);
    g_return_val_if_fail (r == LDAP_RES_SEARCH_ENTRY || r == LDAP_RES_SEARCH_RESULT, FALSE);

    sinfo = get_ldap_server_info (lop->lsrc, TRUE);
     
    /* An LDAP Entry */
    if (r == LDAP_RES_SEARCH_ENTRY) {
        
#ifdef _DEBUG
        g_printerr ("[ldap] Retrieved Key Data:\n");
        dump_ldap_entry (lop->ldap, result);
#endif                 
        key = get_string_attribute (lop->ldap, result, sinfo->key_attr);
        
        if (key == NULL) {
            g_warning ("keyserver missing pgp key data");
            fail_ldap_operation (lop, LDAP_NO_SUCH_OBJECT); 
        }
        
        data = (gpgme_data_t)g_object_get_data (G_OBJECT (lop), "result");
        g_return_val_if_fail (data != NULL, FALSE);
        
        r = gpgme_data_write (data, key, strlen (key));
        g_return_val_if_fail (r != -1, FALSE);

        g_free (key);
        return TRUE;

    /* No more entries, result */
    } else {
       
        lop->ldap_op = -1;
        r = ldap_parse_result (lop->ldap, result, &code, NULL, &message, NULL, NULL, 0);
        g_return_val_if_fail (r == LDAP_SUCCESS, FALSE);
        
        if (code != LDAP_SUCCESS) {
            seahorse_operation_mark_done (SEAHORSE_OPERATION (lop), FALSE, 
                            g_error_new_literal (LDAP_ERROR_DOMAIN, code, message));
        }

        ldap_memfree (message);

        /* Process more keys if possible */
        if (code == LDAP_SUCCESS) 
            return get_key_from_ldap (op, NULL);
    }       
    
    return FALSE;
}

/* Gets a key over an open LDAP connection */
static gboolean
get_key_from_ldap (SeahorseOperation *op, LDAPMessage *result)
{
    SeahorseLDAPOperation *lop = SEAHORSE_LDAP_OPERATION (op);  
    LDAPServerInfo *sinfo;
    GSList *fingerprints, *fprfull;
    gchar *filter;
    char *attrs[2];
    const gchar *fpr;
    int l;
    
    g_return_val_if_fail (lop->ldap != NULL, FALSE);
    g_assert (lop->ldap_op == -1);
    
    fingerprints = (GSList*)g_object_get_data (G_OBJECT (lop), "fingerprints");
    fprfull = (GSList*)g_object_get_data (G_OBJECT (lop), "fingerprints-full");

    l = g_slist_length (fprfull);
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (lop), 
                                      _("Retrieving remote keys..."), 
                                      l - g_slist_length (fingerprints), l);
    
    if (fingerprints) {
     
        fpr = (const gchar*)(fingerprints->data);
        g_return_val_if_fail (fpr != NULL, FALSE);

        /* Keep track of the ones that have already been done */
        fingerprints = g_slist_next (fingerprints);
        g_object_set_data (G_OBJECT (lop), "fingerprints", fingerprints);
                
        l = strlen (fpr);
        if (l > 16)
            fpr += (l - 16);
    
        filter = g_strdup_printf ("(pgpcertid=%.16s)", fpr);
        sinfo = get_ldap_server_info (lop->lsrc, TRUE);
        
        attrs[0] = sinfo->key_attr;
        attrs[1] = NULL;
        
        lop->ldap_op = ldap_search (lop->ldap, sinfo->base_dn, LDAP_SCOPE_SUBTREE,
                                    filter, attrs, 0);

        g_free (filter);

        if (lop->ldap_op == -1) {
            fail_ldap_operation (lop, 0);
            return FALSE;
        }                                    
                
        lop->ldap_cb = get_callback;
        return TRUE;   
    }
    
    /* At this point we're done */
    seahorse_operation_mark_done (op, FALSE, NULL);
    return FALSE;
}

/* Starts a get operation for multiple keys */
static SeahorseLDAPOperation *
start_get_operation_multiple (SeahorseLDAPSource *lsrc, GSList *fingerprints, 
                              gpgme_data_t data)
{
    SeahorseLDAPOperation *lop;
    gpgme_error_t gerr;
    
    g_return_val_if_fail (g_slist_length (fingerprints) > 0, NULL);
    
    lop = seahorse_ldap_operation_start (lsrc, get_key_from_ldap, 
                                         g_slist_length(fingerprints));
    g_return_val_if_fail (lop != NULL, NULL);
        
    if (data) {
        /* Note that we don't auto-free this */
        g_object_set_data (G_OBJECT (lop), "result", data);
    } else {
        /* But when we auto create a data object then we free it */
        gerr = gpgme_data_new (&data);
        g_return_val_if_fail (data != NULL, NULL);
        g_object_set_data_full (G_OBJECT (lop), "result", data, 
                                (GDestroyNotify)gpgme_data_release);
    }
    
    g_object_set_data (G_OBJECT (lop), "fingerprints", fingerprints);
    g_object_set_data_full (G_OBJECT (lop), "fingerprints-full", fingerprints, 
                            (GDestroyNotify)seahorse_util_string_slist_free);

    return lop;
}

/* Starts a get operation for a single key */
static SeahorseLDAPOperation *
start_get_operation (SeahorseLDAPSource *lsrc, const gchar *fpr, 
                     gpgme_data_t data)
{
    GSList *fingerprints = NULL;
    fingerprints = g_slist_prepend (fingerprints, g_strdup (fpr));
    return start_get_operation_multiple (lsrc, fingerprints, data);
}

/* -----------------------------------------------------------------------------
 *  SEND OPERATION 
 */

static gboolean send_key_to_ldap (SeahorseOperation *op, LDAPMessage *result);

/* Called when results come in for a key send */
static gboolean 
send_callback (SeahorseOperation *op, LDAPMessage *result)
{
    SeahorseLDAPOperation *lop = SEAHORSE_LDAP_OPERATION (op);  
    char *message;
    int code;
    int r;

    lop->ldap_op = -1;

    g_return_val_if_fail (ldap_msgtype (result) == LDAP_RES_ADD, FALSE);

    r = ldap_parse_result (lop->ldap, result, &code, NULL, &message, NULL, NULL, 0);
    g_return_val_if_fail (r == LDAP_SUCCESS, FALSE);
    
    /* TODO: Somehow communicate this to the user */
    if (code == LDAP_ALREADY_EXISTS)
        code = LDAP_SUCCESS;
        
    if (code != LDAP_SUCCESS) 
        seahorse_operation_mark_done (SEAHORSE_OPERATION (lop), FALSE, 
                        g_error_new_literal (LDAP_ERROR_DOMAIN, code, message));

    ldap_memfree (message);

    /* Process more keys */
    if (code == LDAP_SUCCESS)
        return send_key_to_ldap (op, NULL);

    return FALSE;
}

/* Initiate a key send over an open LDAP connection */
static gboolean
send_key_to_ldap (SeahorseOperation *op, LDAPMessage *result)
{
    SeahorseLDAPOperation *lop = SEAHORSE_LDAP_OPERATION (op);  
    LDAPServerInfo *sinfo;
    GSList *keys, *keysfull;
    gchar *key;
    gchar *base;
    LDAPMod mod;
    LDAPMod *attrs[2];
    char *values[2];
    guint l;

    g_return_val_if_fail (lop->ldap != NULL, FALSE);
    g_assert (lop->ldap_op == -1);
    
    keys = (GSList*)g_object_get_data (G_OBJECT (lop), "key-data");
    keysfull = (GSList*)g_object_get_data (G_OBJECT (lop), "key-data-full");
    
    l = g_slist_length (keysfull);
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (lop), 
                                      _("Sending keys to key server..."), 
                                      l - g_slist_length (keys), l);
    
    if (keys) {
     
        key = (gchar*)(keys->data);
        g_return_val_if_fail (key != NULL, FALSE);

        /* Keep track of the ones that have already been done */
        keys = g_slist_next (keys);
        g_object_set_data (G_OBJECT (lop), "key-data", keys);

        sinfo = get_ldap_server_info (lop->lsrc, TRUE);
        
        values[0] = key;
        values[1] = NULL;
                
        memset (&mod, 0, sizeof (mod));
        mod.mod_op = LDAP_MOD_ADD;
        mod.mod_type = sinfo->key_attr;
        mod.mod_values = values;
        
        attrs[0] = &mod;
        attrs[1] = NULL;
        
        base = g_strdup_printf ("pgpCertid=virtual,%s", sinfo->base_dn);
        
        lop->ldap_op = ldap_add (lop->ldap, base, attrs);

        g_free (base);
                
        if (lop->ldap_op == -1) {
            fail_ldap_operation (lop, 0);
            return FALSE;
        }                                    
                
        lop->ldap_cb = send_callback;
        return TRUE;   
    }
    
    /* At this point we're done */
    seahorse_operation_mark_done (op, FALSE, NULL);
    return FALSE;
}

/* Start a key send operation for multiple keys */
static SeahorseLDAPOperation *
start_send_operation_multiple (SeahorseLDAPSource *lsrc, GSList *keys)
{
    SeahorseLDAPOperation *lop;

    g_return_val_if_fail (g_slist_length (keys) > 0, NULL);
    
    lop = seahorse_ldap_operation_start (lsrc, send_key_to_ldap, 
                                         g_slist_length (keys));
    g_return_val_if_fail (lop != NULL, NULL);

    g_object_set_data (G_OBJECT (lop), "key-data", keys);
    g_object_set_data_full (G_OBJECT (lop), "key-data-full", keys, 
                            (GDestroyNotify)seahorse_util_string_slist_free);

    return lop;
}

/* Start a key send operation */
static SeahorseLDAPOperation *
start_send_operation (SeahorseLDAPSource *lsrc, gchar *key)
{
    GSList *keys = NULL;
    keys = g_slist_prepend (keys, key);
    return start_send_operation_multiple (lsrc, keys);
}

/* -----------------------------------------------------------------------------
 *  SEAHORSE LDAP SOURCE
 */
 
/* GObject handlers */
static void seahorse_ldap_source_class_init (SeahorseLDAPSourceClass *klass);

/* SeahorseKeySource methods */
static SeahorseOperation*  seahorse_ldap_source_load      (SeahorseKeySource *src,
                                                           SeahorseKeySourceLoad load,
                                                           const gchar *match);
static SeahorseOperation*  seahorse_ldap_source_import    (SeahorseKeySource *sksrc, 
                                                           gpgme_data_t data);
static SeahorseOperation*  seahorse_ldap_source_export    (SeahorseKeySource *sksrc, 
                                                           GList *keys,     
                                                           gboolean complete,
                                                           gpgme_data_t data);
                                                           
static SeahorseKeySourceClass *parent_class = NULL;

GType
seahorse_ldap_source_get_type (void)
{
    static GType type = 0;
 
    if (!type) {
        
        static const GTypeInfo tinfo = {
            sizeof (SeahorseLDAPSourceClass), NULL, NULL,
            (GClassInitFunc) seahorse_ldap_source_class_init, NULL, NULL,
            sizeof (SeahorseLDAPSource), 0, NULL
        };
        
        type = g_type_register_static (SEAHORSE_TYPE_SERVER_SOURCE, 
                                       "SeahorseLDAPSource", &tinfo, 0);
    }
  
    return type;
}

/* Initialize the basic class stuff */
static void
seahorse_ldap_source_class_init (SeahorseLDAPSourceClass *klass)
{
    SeahorseKeySourceClass *key_class;
   
    key_class = SEAHORSE_KEY_SOURCE_CLASS (klass);
    key_class->load = seahorse_ldap_source_load;
    key_class->import = seahorse_ldap_source_import;
    key_class->export = seahorse_ldap_source_export;

    parent_class = g_type_class_peek_parent (klass);
}

static SeahorseOperation*
seahorse_ldap_source_load (SeahorseKeySource *src, SeahorseKeySourceLoad load,
                           const gchar *match)
{
    SeahorseOperation *op;
    SeahorseLDAPOperation *lop = NULL;

    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (src), NULL);
    g_return_val_if_fail (SEAHORSE_IS_LDAP_SOURCE (src), NULL);

    op = parent_class->load (src, load, match);
    if (op != NULL)
        return op;
    
    /* No way to find new or all keys */
    if (load == SKSRC_LOAD_NEW || load == SKSRC_LOAD_ALL) 
        return seahorse_operation_new_complete (NULL);

    /* Search for keys */
    else if (load == SKSRC_LOAD_SEARCH)
        lop = start_search_operation (SEAHORSE_LDAP_SOURCE (src), match);
        
    /* Load a specific key */
    else if (load == SKSRC_LOAD_KEY)
        lop = start_search_operation_fpr (SEAHORSE_LDAP_SOURCE (src), match);
    
    g_return_val_if_fail (lop != NULL, NULL);
    seahorse_server_source_take_operation (SEAHORSE_SERVER_SOURCE (src),
                                           SEAHORSE_OPERATION (lop));
    g_object_ref (lop);
    return SEAHORSE_OPERATION (lop);
}

static SeahorseOperation* 
seahorse_ldap_source_import (SeahorseKeySource *sksrc, gpgme_data_t data)
{
    SeahorseLDAPOperation *lop;
    SeahorseLDAPSource *lsrc;
    GSList *keydata = NULL;
    GString *buf = NULL;
    guint len;
    
    g_return_val_if_fail (SEAHORSE_IS_LDAP_SOURCE (sksrc), NULL);
    lsrc = SEAHORSE_LDAP_SOURCE (sksrc);
    
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
    
    keydata = g_slist_reverse (keydata);
    
    lop = start_send_operation_multiple (lsrc, keydata);
    g_return_val_if_fail (lop != NULL, NULL);
    
    return SEAHORSE_OPERATION (lop);
}

static SeahorseOperation* 
seahorse_ldap_source_export (SeahorseKeySource *sksrc, GList *keys, 
                             gboolean complete, gpgme_data_t data)
{
    SeahorseLDAPOperation *lop;
    SeahorseLDAPSource *lsrc;
    GSList *fingerprints = NULL;
    
    g_return_val_if_fail (SEAHORSE_IS_LDAP_SOURCE (sksrc), NULL);
    lsrc = SEAHORSE_LDAP_SOURCE (sksrc);
    
    for ( ; keys; keys = g_list_next (keys)) {
        g_return_val_if_fail (SEAHORSE_IS_KEY (keys->data), NULL);
        fingerprints = g_slist_prepend (fingerprints,
                g_strdup (seahorse_key_get_keyid (SEAHORSE_KEY (keys->data))));
    }
    
    fingerprints = g_slist_reverse (fingerprints);

    lop = start_get_operation_multiple (lsrc, fingerprints, data);
    g_return_val_if_fail (lop != NULL, NULL);
    
    return SEAHORSE_OPERATION (lop);    
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
