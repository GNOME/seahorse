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
#define SSH_AGENTC_REQUEST_RSA_IDENTITIES   1
#define SSH_AGENT_RSA_IDENTITIES_ANSWER     2
#define SSH2_AGENTC_REQUEST_IDENTITIES      11
#define SSH2_AGENT_IDENTITIES_ANSWER        12
    
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

static gint
read_ssh_message (GIOChannel* source, gchar *buf, gsize bufsize)
{
    GError *err = NULL;
    gsize bytes;
    guint length, read, r;
    
    g_assert (bufsize >= 4);
    
    /* First read the length of the response packet. */
    g_io_channel_read_chars (source, buf, 4, &bytes, &err);
    
    if (err != NULL || bytes != 4) {
        if(bytes == 0)
            return -1;
        g_critical ("couldn't read length from socket: %s", 
                    err && err->message ? err->message : "");
        g_clear_error (&err);
        return -1;
    }
    
    length = GET_32BIT (buf);
    
    /* To prevent buffer overflow discard anything over bufsize */
    for (read = 0; read < length; read += bufsize) {
        
        r = length > bufsize ? bufsize : length;
        
        /* Now read the actual msg */
        g_io_channel_read_chars (source, buf, r, &bytes, &err);
        
        if (err != NULL || bytes != r) {
            g_critical ("couldn't read from socket: %s", 
                         err && err->message ? err->message : "");
            g_clear_error (&err);
            return -1;
        }
        
        read += r;
        length -= r;
    }

    DEBUG_MSG (("received message: %d (len: %d)\n", (int)(guchar)buf[0], bytes));    
    return bytes;
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
    
    g_io_channel_write_chars (source, buf, bufsize, &written, &err);
    
    if (err != NULL || written != bufsize) {
        g_critical ("couldn't write to socket: %s", 
                     err && err->message ? err->message : "");
        g_clear_error (&err);
        return FALSE;
    }

    DEBUG_MSG (("sent message: %d (len: %d)\n", (int)(guchar)buf[0], bufsize));
    return TRUE;
}

static guint
get_num_identities (SSHProxyConn *cn, gboolean version1)
{
    gchar buf[1024];
    SSHMsgHeader *req;
    SSHMsgNumIdentities *resp;
    guint length;
    
    memset (buf, 0, sizeof (buf));
    
    req = (SSHMsgHeader*)buf;
    req->msgid = version1 ? SSH_AGENTC_REQUEST_RSA_IDENTITIES : 
                            SSH2_AGENTC_REQUEST_IDENTITIES;
    
    if (!write_ssh_message (cn->outchan, buf, sizeof (*req)))
        return -1;
    
    length = read_ssh_message (cn->outchan, buf, sizeof (buf));
    
    resp = (SSHMsgNumIdentities*)buf;
    if (length >= sizeof (*resp)) {
        
        if ((version1 && resp->head.msgid == SSH_AGENT_RSA_IDENTITIES_ANSWER) || 
            (!version1 && resp->head.msgid == SSH2_AGENT_IDENTITIES_ANSWER)) {
            return resp->num_identities;
        }
    }
    
    g_warning ("can't understand SSH agent protocol");
    return -1;
}

static gboolean 
filter_version_one (SeahorseKey *key, gpointer data)
{
    g_return_val_if_fail (SEAHORSE_IS_SSH_KEY (key), FALSE);
    return seahorse_ssh_key_get_algo (SEAHORSE_SSH_KEY (key)) == SSH_ALGO_RSA1;
}

static gboolean 
filter_version_two (SeahorseKey *key, gpointer data)
{
    guint algo;
    g_return_val_if_fail (SEAHORSE_IS_SSH_KEY (key), FALSE);
    algo = seahorse_ssh_key_get_algo (SEAHORSE_SSH_KEY (key));
    return algo == SSH_ALGO_RSA || algo == SSH_ALGO_DSA;
}

static GList*
find_ssh_keys (gboolean version1)
{
    SeahorseKeyPredicate skp;
    
    memset (&skp, 0, sizeof (skp));
    skp.ktype = SKEY_SSH;
    skp.location = SKEY_LOC_LOCAL;
    skp.custom = version1 ? filter_version_one : filter_version_two;
    
    return seahorse_context_find_keys_full (SCTX_APP (), &skp);
}

static void
load_ssh_key (SeahorseSSHKey *skey)
{
    SeahorseOperation *op;
    SeahorseSSHSource *ssrc;
    gchar *cmd;
    
    DEBUG_MSG (("Loading SSH key: %s\n", seahorse_ssh_key_get_filename (skey, TRUE)));
    
    cmd = g_strdup_printf (SSH_ADD_PATH " '%s'", seahorse_ssh_key_get_filename (skey, TRUE));
    ssrc = SEAHORSE_SSH_SOURCE (seahorse_key_get_source (SEAHORSE_KEY (skey)));
    op = seahorse_ssh_operation_new (ssrc, cmd, NULL, 0, _("SSH Passphrase"), skey);
    
    g_free (cmd);
    
    seahorse_operation_wait (op);
    
    if (!seahorse_operation_is_successful (op))
        g_warning ("couldn't run ssh-add to add a key identity: %s", 
                   seahorse_operation_get_error (op)->message);
    
    g_object_unref (op);
}

static gboolean
process_message (SSHProxyConn *cn, gboolean from_client, gchar *msg, gsize len)
{
    SSHMsgHeader *smsg = (SSHMsgHeader*)msg;
    gboolean version1 = FALSE;
    GList *keys, *l;
    guint count;
    
    if (!from_client)
        return TRUE;
    
    switch (smsg->msgid) {
        
    /* For pings we just write the same thing back */
    case SEAHORSE_SSH_PING_MSG:
        write_ssh_message (cn->inchan, msg, len);
        return FALSE;
    
    case SSH_AGENTC_REQUEST_RSA_IDENTITIES:
        version1 = TRUE;
        /* Fall through */;
    case SSH2_AGENTC_REQUEST_IDENTITIES:
        break;
    
    /* Ignore all other messages */
    default:
        return TRUE;
    };
    
    /* Check if we're supposed to load */
    if(!seahorse_gconf_get_boolean(SETTING_AGENT_SSH)) 
        return TRUE;

    /* If keys are already loaded then return */
    count = get_num_identities (cn, version1);
    if (count != 0)
        return TRUE;
    
    keys = find_ssh_keys (version1);
    
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
    gchar msg[1024];
    gboolean ret = FALSE;
    gint length;
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
        
        length = read_ssh_message (source, msg, sizeof (msg));
        
        if (length <= 0) {
            free_connection (cn);
            cn = NULL;
            goto finally;
        }
        
        /* Filter it and send it off */
        if (process_message (cn, from_client, msg, length))
            write_ssh_message (out, msg, length);

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
    
    ssh_agent_initialized = FALSE;
}
