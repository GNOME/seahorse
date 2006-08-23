/*
 * Seahorse
 *
 * Copyright (C) 2006 Nate Nielsen
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
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <paths.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <ctype.h>

#include <gnome.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "seahorse-gconf.h"
#include "seahorse-agent.h"
#include "seahorse-context.h"
#include "seahorse-key.h"
#include "seahorse-ssh-key.h"
#include "seahorse-ssh-operation.h"
#include "seahorse-passphrase.h"
#include "seahorse-util.h"
#include "seahorse-unknown-source.h"

#ifndef DEBUG_SSHAGENT_ENABLE
#if _DEBUG
#define DEBUG_SSHAGENT_ENABLE 1
#else
#define DEBUG_SSHAGENT_ENABLE 0
#endif
#endif

#if DEBUG_SSHAGENT_ENABLE
#define DEBUG_MSG(x)  g_printerr x
#else
#define DEBUG_MSG(x)
#endif

static GSList *ssh_agent_cached_fingerprints = NULL;
static gboolean ssh_agent_fingerprints_loaded = FALSE;

static gboolean ssh_agent_initialized = FALSE;
static GList *ssh_agent_connections = NULL;

static gint ssh_agent_socket = -1;                      /* Socket we're listening on */
static GIOChannel *ssh_agent_iochan = NULL;             /* IO channel for above */
static gint ssh_agent_iochan_tag = 0;                   /* Event source tag for above */
static char ssh_agent_sockname[MAXPATHLEN] = { 0, };    /* Name of socket we're listening on */
static gchar ssh_client_sockname[MAXPATHLEN] = { 0, };  /* Name of socket we're connected to */

typedef struct _SSHProxyConn {
    gint instag;                /* glib source tag */
    GIOChannel *inchan;         /* Proxy Io channel for connection */
    gint outstag;               /* glib source tag */
    GIOChannel *outchan;        /* Connection to real SSH agent */
} SSHProxyConn;

#define GET_32BIT(cp) (((guint32)(guchar)(cp)[0] << 24) | ((guint32)(guchar)(cp)[1] << 16) | \
                       ((guint32)(guchar)(cp)[2] << 8) | ((guint32)(guchar)(cp)[3]))

#define GET_16BIT(cp) (((guint32)(guchar)(cp)[0] << 8) | ((guint32)(guchar)(cp)[1]))

#define PUT_32BIT(cp, value) \
    do { (cp)[0] = (value) >> 24; (cp)[1] = (value) >> 16; (cp)[2] = (value) >> 8; (cp)[3] = (value); } while (0)

#define PUT_16BIT(cp, value) \
    do { (cp)[0] = (value) >> 8; (cp)[1] = (value); } while (0)

/* The various messages we're interested in */
#define SSH2_AGENTC_REQUEST_IDENTITIES      11
#define SSH2_AGENT_IDENTITIES_ANSWER        12
#define SSH2_AGENTC_ADD_IDENTITY            17
#define SSH2_AGENTC_REMOVE_IDENTITY         18
#define SSH2_AGENTC_REMOVE_ALL_IDENTITIES   19
    
struct _SSHMsgHeader {
    guchar msgid;
} __attribute__ ((__packed__));

struct _SSHMsgNumIdentities {
    struct _SSHMsgHeader head;
    gint num_identities;
} __attribute__ ((__packed__));

typedef struct _SSHMsgHeader SSHMsgHeader;
typedef struct _SSHMsgNumIdentities SSHMsgNumIdentities;

/* -----------------------------------------------------------------------------
 * IMPLEMENTATION
 */

static void
clear_cached_fingerprints()
{
    DEBUG_MSG (("Clearing our internal cache of loaded SSH fingerprints\n"));
    seahorse_util_string_slist_free (ssh_agent_cached_fingerprints);
    ssh_agent_cached_fingerprints = NULL;
    ssh_agent_fingerprints_loaded = FALSE;
}

static gboolean
parse_cached_fingerprint (SeahorseSSHKeyData *keydata, gpointer arg)
{
    ssh_agent_cached_fingerprints = 
        g_slist_prepend (ssh_agent_cached_fingerprints, g_strdup (keydata->fingerprint));
    seahorse_ssh_key_data_free (keydata);
    return TRUE;
}

static void
load_cached_fingerprints ()
{
    SeahorseKeySource *sksrc;
    gchar *output;
    GError *err = NULL;
    
    if (ssh_agent_fingerprints_loaded)
        return;
    
    clear_cached_fingerprints ();
    
    DEBUG_MSG (("Loading internal cache of fingerprints\n"));
    ssh_agent_fingerprints_loaded = TRUE;
    
    sksrc = seahorse_context_find_key_source (SCTX_APP (), SKEY_SSH, SKEY_LOC_LOCAL);
    g_return_if_fail (SEAHORSE_IS_SSH_SOURCE (sksrc));
    
    /* TODO: This could possibly done more efficiently by talking with the agent ourselves */
    output = seahorse_ssh_operation_sync (SEAHORSE_SSH_SOURCE (sksrc), "/bin/sh -c \"" SSH_ADD_PATH " -L; true\"", &err);
    if (!output) {
        g_warning ("couldn't list keys in SSH agent: %s", err && err->message ? err->message : "");
        return;
    }
    
    seahorse_ssh_key_data_parse (output, parse_cached_fingerprint, NULL, NULL);
    g_free (output);
}

/* Free the given connection structure */
static void
free_connection (SSHProxyConn *cn)
{
    if (cn->instag)
        g_source_remove (cn->instag);
    cn->instag = 0;
        
    if (cn->inchan) {
        g_io_channel_shutdown (cn->inchan, TRUE, NULL);
        g_io_channel_unref (cn->inchan);
        cn->inchan = NULL;
    }
    
    if (cn->outstag)
        g_source_remove (cn->outstag);
    cn->outstag = 0;
    
    if (cn->outchan) {
        g_io_channel_shutdown (cn->outchan, TRUE, NULL);
        g_io_channel_unref (cn->outchan);
        cn->outchan = NULL;
    }
    
    DEBUG_MSG (("closed connection\n"));

    ssh_agent_connections = g_list_remove (ssh_agent_connections, cn);
    g_free (cn);
}

/* Callback for freeing all connections */
static void
free_connections (gpointer data, gpointer user_data)
{
    free_connection ((SSHProxyConn*)data);
}

static gchar*
read_ssh_message (GIOChannel* source, gsize* length)
{
    GError *err = NULL;
    gchar buf[256];
    gsize bytes;
    GString *msg = NULL;
    guint r;
    
    *length = 0;
    
    /* First read the length of the response packet */
    g_io_channel_read_chars (source, (gchar*)buf, 4, &bytes, &err);
    
    if (err != NULL || bytes != 4) {
        if(bytes == 0)
            return NULL;
        g_critical ("couldn't read length from socket: %s", 
                    err && err->message ? err->message : "");
        g_clear_error (&err);
        return NULL;
    }
    
    *length = GET_32BIT (buf);
    msg = g_string_sized_new (1024);
    
    while (msg->len < *length) {
        
        r = (*length - msg->len) > sizeof (buf) ? sizeof (buf) : (*length - msg->len);
        
        /* Now read the actual msg */
        g_io_channel_read_chars (source, (gchar*)buf, r, &bytes, &err);
        
        if (err != NULL || bytes != r) {
            g_critical ("couldn't read from socket: %s (%d/%d/%d)", 
                         err && err->message ? err->message : "", r, bytes, *length);
            g_clear_error (&err);
            g_string_free (msg, TRUE);
            return NULL;
        }
        
        g_string_append_len (msg, buf, r);
    }
    
    g_assert (*length == msg->len);

    DEBUG_MSG (("received message: %d (len: %d)\n", (int)(gchar)msg->str[0], *length));
    return (gchar*)g_string_free (msg, FALSE);
}

static gboolean
write_ssh_message (GIOChannel* source, gchar *buf, gsize bufsize)
{
    GError *err = NULL;
    gchar lbuf[8];
    gsize written;

    PUT_32BIT (lbuf, bufsize);
    g_io_channel_write_chars (source, lbuf, 4, &written, &err);
    
    if (err != NULL || written != 4) {
        g_critical ("couldn't write to socket: %s", 
                     err && err->message ? err->message : "");
        g_clear_error (&err);
        return FALSE;
    }
    
    g_io_channel_write_chars (source, (gchar*)buf, bufsize, &written, &err);
    
    if (err != NULL || written != bufsize) {
        g_critical ("couldn't write to socket: %s", 
                     err && err->message ? err->message : "");
        g_clear_error (&err);
        return FALSE;
    }

    DEBUG_MSG (("sent message: %d (len: %d)\n", (int)(gchar)buf[0], bufsize));
    return TRUE;
}

static gint
get_num_identities (SSHProxyConn *cn)
{
    gchar *msg;
    SSHMsgHeader req;
    SSHMsgNumIdentities *resp;
    guint length;
    guint ret = -1;
    
    memset (&req, 0, sizeof (req));
    req.msgid = SSH2_AGENTC_REQUEST_IDENTITIES;
    
    if (!write_ssh_message (cn->outchan, (gchar*)(&req), sizeof (req)))
        return -1;
    
    msg = read_ssh_message (cn->outchan, &length);
    if (!msg)
        return -1;
    
    resp = (SSHMsgNumIdentities*)msg;
    if (length >= sizeof (*resp)) {
        if (resp->head.msgid == SSH2_AGENT_IDENTITIES_ANSWER) 
            ret = resp->num_identities;
    } else {
        g_warning ("can't understand SSH agent protocol");
    }
    
    g_free (msg);
    return ret;
}

static gboolean 
filter_keys (SeahorseKey *key, gpointer data)
{
    guint algo;
    g_return_val_if_fail (SEAHORSE_IS_SSH_KEY (key), FALSE);
    algo = seahorse_ssh_key_get_algo (SEAHORSE_SSH_KEY (key));
    return (algo == SSH_ALGO_RSA || algo == SSH_ALGO_DSA) && 
            seahorse_key_get_etype (key) == SKEY_PRIVATE;
}

static GList*
find_ssh_keys ()
{
    SeahorseKeyPredicate skp;
    
    memset (&skp, 0, sizeof (skp));
    skp.ktype = SKEY_SSH;
    skp.location = SKEY_LOC_LOCAL;
    skp.custom = filter_keys;
    
    return seahorse_context_find_keys_full (SCTX_APP (), &skp);
}

static gboolean
update_status (gpointer dummy)
{
    seahorse_agent_status_update ();
    return FALSE;
}

static void
load_ssh_key (SeahorseSSHKey *skey)
{
    SeahorseOperation *op;
    SeahorseSSHSource *ssrc;
    
    DEBUG_MSG (("Loading SSH key: %s\n", seahorse_ssh_key_get_location (skey)));
    ssrc = SEAHORSE_SSH_SOURCE (seahorse_key_get_source (SEAHORSE_KEY (skey)));
    
    op = seahorse_ssh_operation_agent_load (ssrc, skey);
    seahorse_operation_wait (op);
    
    if (!seahorse_operation_is_successful (op))
        g_warning ("couldn't run ssh-add to add a key identity: %s", 
                   seahorse_operation_get_error (op)->message);
    
    clear_cached_fingerprints ();
    g_timeout_add (500, update_status, NULL);

    g_object_unref (op);
}

static gboolean
process_message (SSHProxyConn *cn, gboolean from_client, gchar *msg, gsize len)
{
    SSHMsgHeader *smsg = (SSHMsgHeader*)msg;
    GList *keys, *l;
    guint count;
    
    if (!from_client)
        return TRUE;
    
    switch (smsg->msgid) {
        
    /* For pings we just write the same thing back */
    case SEAHORSE_SSH_PING_MSG:
        write_ssh_message (cn->inchan, (gchar*)msg, len);
        return FALSE;
    
    case SSH2_AGENTC_REQUEST_IDENTITIES:
        break;
    
    /* Keep our internal representation of what's cached consistent */
    case SSH2_AGENTC_ADD_IDENTITY:
    case SSH2_AGENTC_REMOVE_IDENTITY:
    case SSH2_AGENTC_REMOVE_ALL_IDENTITIES:
        clear_cached_fingerprints ();
        g_timeout_add (500, update_status, NULL);
        return TRUE;
    
    /* Ignore all other messages */
    default:
        return TRUE;
    };
    
    /* Check if we're supposed to load */
    if(!seahorse_gconf_get_boolean(SETTING_AGENT_SSH)) 
        return TRUE;

    /* If keys are already loaded then return */
    count = get_num_identities (cn);
    if (count != 0)
        return TRUE;
    
    keys = find_ssh_keys ();
    
    for (l = keys; l; l = g_list_next (l))
        load_ssh_key (SEAHORSE_SSH_KEY (l->data));
    
    g_list_free (keys);

    /* Let the message continue, now with keys cached */
    return TRUE;
}

/* Callback for data coming from client */
static gboolean
io_handler (GIOChannel *source, GIOCondition condition, SSHProxyConn *cn)
{
    GIOChannel *out;
    gchar *msg;
    gboolean ret = FALSE;
    gsize length;
    gboolean from_client = FALSE;
    
    memset (msg, 0, sizeof (msg));
    
    if (condition & G_IO_IN) {

        if (source == cn->inchan) {
            out = cn->outchan;
            from_client = TRUE;
            DEBUG_MSG (("data from client\n"));
        }
        else if (source == cn->outchan) {
            out = cn->inchan;
            from_client = FALSE;
            DEBUG_MSG (("data from agent\n"));
        }
        else
            g_return_val_if_reached (FALSE);
        
        msg = read_ssh_message (source, &length);
        
        if (!msg) {
            free_connection (cn);
            cn = NULL;
            goto finally;
        }
        
        /* Filter it and send it off */
        if (process_message (cn, from_client, msg, length))
            write_ssh_message (out, msg, length);

        g_free (msg);
        ret = TRUE;
    }

    if (cn && condition & G_IO_HUP) {
        free_connection (cn);
        goto finally;
    }

finally:
    return ret;
}

/* Callback for new incoming connections */
static gboolean
connect_handler (GIOChannel *source, GIOCondition cond, gpointer data)
{
    struct sockaddr_un sunaddr;
    SSHProxyConn *cn;
    int agentfd;
    int fd;

    g_assert (source);
    g_assert (cond == G_IO_IN);

    fd = accept (g_io_channel_unix_get_fd (source), NULL, NULL);
    if (fd < 0) {
        if (errno != EINTR && errno != EAGAIN)
            g_critical ("couldn't accept connection: %s", strerror (errno));
        return TRUE;    /* don't stop listening */
    }
    
    DEBUG_MSG (("accepted connection\n"));

    /* Try to connect to the real agent */
    agentfd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (agentfd == -1) {
        g_warning ("couldn't create socket: %s", g_strerror (errno));
        return TRUE;
    }
    
    memset (&sunaddr, 0, sizeof (sunaddr));
    sunaddr.sun_family = AF_UNIX;
    g_strlcpy (sunaddr.sun_path, ssh_client_sockname, sizeof (sunaddr.sun_path));
    if (connect (agentfd, (struct sockaddr*) &sunaddr, sizeof sunaddr) < 0) {
        g_warning ("couldn't connect to SSH agent at: %s: %s", ssh_agent_sockname, 
                   g_strerror (errno));
        close (agentfd);
        return TRUE;
    }
    
    /* Setup io channel for new socket */
    cn = g_new0 (SSHProxyConn, 1);
    ssh_agent_connections = g_list_append (ssh_agent_connections, cn);
    
    cn->inchan = g_io_channel_unix_new (fd);
    g_io_channel_set_close_on_unref (cn->inchan, TRUE);
    g_io_channel_set_encoding (cn->inchan, NULL, NULL);
    g_io_channel_set_buffered (cn->inchan, FALSE);
    cn->instag = g_io_add_watch (cn->inchan, G_IO_IN | G_IO_HUP, (GIOFunc)io_handler, cn);
    
    cn->outchan = g_io_channel_unix_new (agentfd);
    g_io_channel_set_close_on_unref (cn->outchan, TRUE);
    g_io_channel_set_encoding (cn->outchan, NULL, NULL);
    g_io_channel_set_buffered (cn->outchan, FALSE);
    cn->outstag = g_io_add_watch (cn->outchan, G_IO_IN | G_IO_HUP, (GIOFunc)io_handler, cn);
    return TRUE;
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

gboolean
seahorse_agent_ssh_init ()
{
    struct sockaddr_un sunaddr;
    const gchar *sockname;
    mode_t prev_mask;
    gchar *t;
    
    g_assert (!ssh_agent_initialized);
    
    sockname = g_getenv ("SSH_AUTH_SOCK");
    if (!sockname) {
        g_warning ("no SSH agent running on the system. Cannot proxy SSH key requests.");
        return FALSE;
    }
    
    switch (seahorse_passphrase_detect_agent (SKEY_SSH)) {
    case SEAHORSE_AGENT_SEAHORSE:
        g_warning ("This SSH agent is already being proxied: %s", sockname);
        return FALSE;
    case SEAHORSE_AGENT_UNKNOWN:
    case SEAHORSE_AGENT_NONE:
        g_warning ("couldn't contact SSH agent. Cannot proxy SSH key requests.");
        return FALSE;
    default:
        break;
    };
    
    /* New sock names */
    t = g_strdup_printf ("%s.proxied-by-seahorse", sockname);
    g_strlcpy (ssh_client_sockname, t, sizeof (ssh_client_sockname));
    g_strlcpy (ssh_agent_sockname, sockname, sizeof (ssh_agent_sockname));
    g_free (t);
    
    /* Rename */
    g_unlink (ssh_client_sockname);
    if (g_rename (sockname, ssh_client_sockname) == -1) {
        g_warning ("couldn't replace SSH agent socket with seahorse proxy socket");
        ssh_client_sockname[0] = 0; /* No renaming back */
        return FALSE;
    }
    
    /* Start building our own socket */
    ssh_agent_socket = socket (AF_UNIX, SOCK_STREAM, 0);
    if (ssh_agent_socket == -1) {
        g_warning ("couldn't create SSH proxy socket: %s", g_strerror (errno));
        return FALSE;
    }
    
    memset (&sunaddr, 0, sizeof(sunaddr));
    sunaddr.sun_family = AF_UNIX;
    g_strlcpy (sunaddr.sun_path, sockname, sizeof (sunaddr.sun_path));
    prev_mask = umask (0177);
    if (bind (ssh_agent_socket, (struct sockaddr *) & sunaddr, sizeof(sunaddr)) < 0) {
        g_warning ("couldn't bind to SSH proxy socket: %s: %s", sockname, g_strerror (errno));
        umask (prev_mask);
        return FALSE;
    }
    umask (prev_mask);
    
    if (listen (ssh_agent_socket, 5) < 0) {
        g_warning ("couldn't listen on SSH proxy socket: %s: %s", sockname, g_strerror (errno));
        return FALSE;
    }
    
    /* Watch for connections on the socket */
    ssh_agent_iochan = g_io_channel_unix_new (ssh_agent_socket);
    g_io_channel_set_close_on_unref (ssh_agent_iochan, TRUE);
    ssh_agent_iochan_tag = g_io_add_watch (ssh_agent_iochan, G_IO_IN, connect_handler, NULL);
    
    DEBUG_MSG (("proxying SSH from %s -> %s\n", ssh_agent_sockname, ssh_client_sockname));

    /* All nicely done */
    ssh_agent_initialized = TRUE;
    
    /* We want any of our accesses to the agent to go directly: */
    g_setenv ("SSH_AUTH_SOCK", ssh_client_sockname, TRUE);
    return TRUE;
}

void 
seahorse_agent_ssh_uninit ()
{
    if (!ssh_agent_initialized)
        return;
    
    if (ssh_agent_connections) {
        g_list_foreach (ssh_agent_connections, free_connections, NULL);
        g_list_free (ssh_agent_connections);
        ssh_agent_connections = NULL;
    }
    
    if (ssh_agent_iochan_tag)
        g_source_remove (ssh_agent_iochan_tag);
    ssh_agent_iochan_tag = 0;
    
    if (ssh_agent_iochan) {
        g_io_channel_shutdown (ssh_agent_iochan, FALSE, NULL);
        g_io_channel_unref (ssh_agent_iochan);
        ssh_agent_iochan = NULL;
        ssh_agent_socket = -1;
    }
    
    /* Rename things back */
    if (ssh_agent_sockname[0])
        g_unlink (ssh_agent_sockname);
    if (ssh_agent_sockname[0] && ssh_client_sockname[0])
        g_rename (ssh_client_sockname, ssh_agent_sockname);
    
    /* Free the cached data */
    clear_cached_fingerprints ();
    
    ssh_agent_initialized = FALSE;
}

GList*   
seahorse_agent_ssh_cached_keys ()
{
    GList *keys = NULL;
    SeahorseKey *skey;
    SeahorseKeySource *sksrc;
    GSList *l;
    GQuark keyid;
    
    load_cached_fingerprints ();
    
    for (l = ssh_agent_cached_fingerprints; l; l = g_slist_next (l)) {
        
        keyid = seahorse_ssh_key_get_cannonical_id ((const gchar*)l->data);
        g_return_val_if_fail (keyid, NULL);
        
        skey = seahorse_context_find_key (SCTX_APP (), keyid, SKEY_LOC_LOCAL);
        if (!skey) {
            sksrc = seahorse_context_find_key_source (SCTX_APP (), SKEY_SSH, SKEY_LOC_UNKNOWN);
            g_return_val_if_fail (sksrc != NULL, NULL);
            skey = seahorse_unknown_source_add_key (SEAHORSE_UNKNOWN_SOURCE (sksrc), keyid, NULL);
        }
        
        if (skey)
            keys = g_list_prepend (keys, skey);
    }
    
    return keys;
}

guint    
seahorse_agent_ssh_count_keys ()
{
    load_cached_fingerprints ();
    return g_slist_length (ssh_agent_cached_fingerprints);
}

void
seahorse_agent_ssh_clearall ()
{
    SeahorseKeySource *sksrc;
    GError *err = NULL;
    gchar *output;

    sksrc = seahorse_context_find_key_source (SCTX_APP (), SKEY_SSH, SKEY_LOC_LOCAL);
    g_return_if_fail (SEAHORSE_IS_SSH_SOURCE (sksrc));
    
    /* TODO: This could possibly done more efficiently by talking with the agent ourselves */
    output = seahorse_ssh_operation_sync (SEAHORSE_SSH_SOURCE (sksrc), SSH_ADD_PATH " -D", &err);
    if (!output) {
        g_warning ("couldn't clear keys in SSH agent: %s", err && err->message ? err->message : "");
        return;
    }

    clear_cached_fingerprints ();
    update_status (NULL);
    
    g_free (output);
}
