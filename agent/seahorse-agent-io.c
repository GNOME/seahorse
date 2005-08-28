/*
 * Seahorse
 *
 * Copyright (C) 2004-2005 Nate Nielsen
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

#include "seahorse-agent.h"

/* Override the DEBUG_REFRESH_ENABLE switch here */
/* #define DEBUG_AGENTIO_ENABLE 0 */

#ifndef DEBUG_AGENTIO_ENABLE
#if _DEBUG
#define DEBUG_AGENTIO_ENABLE 1
#else
#define DEBUG_AGENTIO_ENABLE 0
#endif
#endif

#if DEBUG_AGENTIO_ENABLE
#define DEBUG_AGENTIO(x)    g_printerr x
#else
#define DEBUG_AGENTIO(x)
#endif

/*
 * Handles the server and open sockets. Parses received commands 
 * and sends back responses when instructed to. Note that we don't
 * use a seperate thread. Every uses glib event sources. This allows
 * a clean and reliable implementation.
 */

static GList *g_connections = NULL;     /* All open connections */
static GMemChunk *g_memory = NULL;      /* Memory for connections */

static gint g_socket = -1;              /* Socket we're listening on */
static GIOChannel *g_iochannel = NULL;  /* IO channel for above */
static gint g_iochannel_tag = 0;        /* Event source tag for above */
static char g_socket_name[MAXPATHLEN];  /* Name of socket we're listening on */

struct _SeahorseAgentConn {
    gint stag;                  /* glib source tag */
    gboolean input;             /* Whether in input mode or not */
    GIOChannel *iochannel;      /* Io channel for connection */
    gboolean terminal_ok;        /* Whether this is from the current display */
};

/* -----------------------------------------------------------------------------
 *  DEFINES and DEFAULTS
 */

#define SOCKET_DIR  "seahorse-XXXXXX"
#define SOCKET_FILE "/S.gpg-agent"

/* Commands */
#define ASS_ID      "AGENT_ID"
#define ASS_NOP     "NOP"
#define ASS_BYE     "BYE"
#define ASS_RESET   "RESET"
#define ASS_OPTION  "OPTION"
#define ASS_GETPASS "GET_PASSPHRASE"
#define ASS_CLRPASS "CLEAR_PASSPHRASE"

#define ASS_OPT_DISPLAY "display="

/* Responses */
#define ASS_OK      "OK "
#define ASS_ERR     "ERR "
#define NL          "\n"

/* -------------------------------------------------------------------------- */

/* Create the socket and fill in sockname with its path */
int
seahorse_agent_io_socket ()
{
    struct sockaddr_un addr;
    gchar *t;
    int len;
    
    g_assert (g_socket == -1);

    /* We build put the socket in a directory called ~/.gnome2/seahorse-XXXXXX */
    t = gnome_util_home_file (SOCKET_DIR);
    strncpy (g_socket_name, t, KL (g_socket_name));
    g_free (t);

    /* Make the appropriate directory */
    if (!mkdtemp (g_socket_name)) {
        g_critical ("can't create directory: %s: %s", g_socket_name,
                    strerror (errno));
        return -1;
    }

    /* Make sure nobody else can use the socket */
    if (chmod(g_socket_name, 0700) == -1)
        g_warning ("couldn't set permissions on directory: %s", strerror (errno));

    /* Build the socket name */
    strncat (g_socket_name, SOCKET_FILE, KL (g_socket_name));
    g_socket_name[KL (g_socket_name)] = 0;

    memset (&addr, 0, sizeof (addr));
    addr.sun_family = AF_UNIX;
    strncpy (addr.sun_path, g_socket_name, KL (addr.sun_path));
    addr.sun_path[KL (addr.sun_path)] = 0;

    len = offsetof (struct sockaddr_un, sun_path) + strlen (addr.sun_path) + 1;

    /* Now make the appropriate socket */
    g_socket = socket (AF_UNIX, SOCK_STREAM, 0);
    if (g_socket == -1) {
        g_critical ("can't create socket: %s", strerror (errno));
        return -1;
    }

    /* Bind it to the address */
    if (bind (g_socket, (struct sockaddr *) &addr, len) == -1) {
        g_critical ("couldn't bind to socket: %s: %s", addr.sun_path,
                    strerror (errno));
        return -1;
    }

    /* Make sure nobody else can use the socket */
    if (chmod(g_socket_name, 0600) == -1)
        g_warning ("couldn't set permissions on socket: %s", strerror (errno));

    return 0;
}

const gchar*
seahorse_agent_io_get_socket ()
{
    return g_socket_name;
}

/* Disconnect the connection */
static void
disconnect (SeahorseAgentConn *cn)
{
    /* The watch tag */
    if (cn->stag) {
        g_source_remove (cn->stag);
        cn->stag = 0;

        if (cn->iochannel)
            g_io_channel_shutdown (cn->iochannel, TRUE, NULL);
    }
}        

/* Free the given connection structure */
static void
free_conn (SeahorseAgentConn *cn)
{
    if (cn->iochannel) {
        disconnect (cn);

        g_io_channel_unref (cn->iochannel);
        cn->iochannel = NULL;
    }

    g_connections = g_list_remove (g_connections, cn);
    g_chunk_free (cn, g_memory);
}

/* Is the argument a assuan null parameter? */
static gboolean
is_null_argument (gchar *arg)
{
    return (strcmp (arg, "X") == 0);
}

static const gchar HEX_CHARS[] = "0123456789ABCDEF";

/* Decode an assuan parameter */
static void
decode_assuan_arg (gchar *arg)
{
    gchar *t;
    gint len;

    for (len = strlen (arg); len > 0; arg++, len--) {
        switch (*arg) {
            /* + becomes a space */
        case '+':
            *arg = ' ';
            break;

            /* hex encoded as in URIs */
        case '%':
            *arg = '?';
            t = strchr (HEX_CHARS, arg[1]);
            if (t != NULL) {
                *arg = ((t - HEX_CHARS) & 0xf) << 4;
                t = strchr (HEX_CHARS, arg[2]);
                if (t != NULL)
                    *arg |= (t - HEX_CHARS) & 0xf;
            }
            len -= 2;
            if (len < 1)        /* last char, null terminate */
                arg[1] = 0;
            else                /* collapse rest */
                memmove (arg + 1, arg + 3, len);
            break;
        };
    }
}

/* Split a line into each of it's arguments. This modifies line */
static void
split_arguments (gchar *line, ...)
{
    gchar **cur;
    va_list ap;

    va_start (ap, line);

    while (*line && isspace (*line))
        line++;

    while ((cur = va_arg (ap, gchar **)) != NULL) {
        if (*line) {
            *cur = line;

            while (*line && !isspace (*line))
                line++;

            while (*line && isspace (*line)) {
                *line = 0;
                line++;
            }

            decode_assuan_arg (*cur);
        } else {
            *cur = NULL;
        }
    }

    va_end (ap);
}

/* Process a request line from client */
static void
process_line (SeahorseAgentConn *cn, gchar *string)
{
    gchar *args;

    DEBUG_AGENTIO (("[agent-io] got line:\n%s", string));

    g_strstrip (string);

    if (strlen (string) == 0)
        return;                 /* don't worry about empty lines */

    /* Split the command off from the args */
    args = strchr (string, ' ');
    if (args) {
        *args = 0;
        args++;
    } else {
        /* Pointer to the end, empty string */
        args = string + strlen (string);
    }

    /* We received a line, hold on until response is sent */
    cn->input = FALSE;

    if (strcasecmp (string, ASS_OPTION) == 0) {
        gchar *option;
        
        split_arguments (args, &option, NULL);
        
        if (!option) {
            seahorse_agent_io_reply (cn, FALSE, "105 parameter error");
            g_warning ("received invalid option argument");
            return;
        }
            
        /* 
         * If the option is a display option we make sure it's 
         * the same as our display. Otherwise we don't answer.
         */
        if (g_ascii_strncasecmp (option, ASS_OPT_DISPLAY, KL (ASS_OPT_DISPLAY)) == 0) {
            option += KL (ASS_OPT_DISPLAY);
            if (g_ascii_strcasecmp (option, g_getenv("DISPLAY")) == 0) {
                cn->terminal_ok = TRUE;
            } else {
                g_warning ("received request different display: %s", option);
                seahorse_agent_io_reply (cn, FALSE, "105 parameter conflict");
                return;
            }
        }
        
        /* We don't do anything with the other options right now */
        seahorse_agent_io_reply (cn, TRUE, NULL);
    }

    else if (strcasecmp (string, ASS_GETPASS) == 0) {
        gchar *id;
        gchar *errmsg;
        gchar *prompt;
        gchar *description;

        /* We don't answer this unless it's from the right terminal */
        if (!cn->terminal_ok) {
            seahorse_agent_io_reply (cn, FALSE, "113 Server Resource Problem");
            g_warning ("received passphrase request from wrong terminal");
            return;
        }
                
        split_arguments (args, &id, &errmsg, &prompt, &description, NULL);

        if (!id || !errmsg || !prompt || !description) {
            seahorse_agent_io_reply (cn, FALSE, "105 parameter error");
            g_warning ("received invalid passphrase request");
            return;
        }

        if (is_null_argument (id))
            id = NULL;
        if (is_null_argument (errmsg))
            errmsg = NULL;
        if (is_null_argument (prompt))
            prompt = NULL;
        if (is_null_argument (description))
            description = NULL;

        seahorse_agent_actions_getpass (cn, id, errmsg, prompt, description);
    }

    else if (strcasecmp (string, ASS_CLRPASS) == 0) {
        gchar *id;

        /* We don't answer this unless it's from the right terminal */
        if (!cn->terminal_ok) {
            seahorse_agent_io_reply (cn, FALSE, "113 Server Resource Problem");
            g_warning ("received passphrase request from wrong terminal");
            return;
        }

        split_arguments (args, &id, NULL);

        if (!id) {
            seahorse_agent_io_reply (cn, FALSE, "105 parameter error");
            g_warning ("received invalid clear pass request: %s", string);
        }

        seahorse_agent_actions_clrpass (cn, id);
    }

    else if (strcasecmp (string, ASS_NOP) == 0) {
        seahorse_agent_io_reply (cn, TRUE, NULL);
    }

    else if (strcasecmp (string, ASS_BYE) == 0) {
        seahorse_agent_io_reply (cn, TRUE, "closing connection");
        disconnect (cn);
    }

    else if (strcasecmp (string, ASS_RESET) == 0) {
        /* We keep no state :) */
        seahorse_agent_io_reply (cn, TRUE, NULL);
    }

    else if (strcasecmp (string, ASS_ID) == 0) {
        seahorse_agent_io_reply (cn, TRUE, "seahorse-agent");
    }

    else {
        g_warning ("unrecognized command: %s", string);
        seahorse_agent_io_reply (cn, FALSE, "103 unknown command");
    }
}

/* Callback for data coming from client */
static gboolean
io_handler (GIOChannel *source, GIOCondition condition, gpointer data)
{
    SeahorseAgentConn *cn = (SeahorseAgentConn *) data;
    gchar *string;
    gsize length;
    GError *err = NULL;
    gboolean ret = TRUE;

    if (condition & G_IO_IN) {
        /* Read 1 line from the io channel, including newline character */
        g_io_channel_read_line (source, &string, &length, NULL, &err);

        if (err != NULL) {
            g_critical ("couldn't read from socket: %s", err->message);
            g_clear_error (&err);
            free_conn (cn);
            cn = NULL;
            ret = FALSE;
        }

        else if (length > 0) {
            /* Send it off for processing */
            process_line (cn, string);
        }

        if (string)
            g_free (string);
    }

    if (cn && condition & G_IO_HUP) {
        free_conn (cn);
        ret = FALSE;            /* removes watch */
    }

    return ret;
}

/* Double check that a connection hasn't gone away */
static gboolean
is_valid_conn (SeahorseAgentConn *cn)
{
    return cn && cn->iochannel && 
           g_list_index (g_connections, cn) != -1;
}

/* Write all passed data to socket */
static int
write_raw_data (int fd, const gchar *data, int len)
{
    int r, x = 0;

    /* 
     * Guarantee that all data is written. We don't want to go
     * through g_io_channel_xxx functions because they might cache
     * their data. Passwords and the like shouldn't be cached anywhere
     * but the secure memory.
     */

    if (len == -1)
        len = strlen (data);

    while (len > 0) {
        r = write (fd, data, len);
        if (r == -1) {
            if (errno != EAGAIN && errno != EINTR) {
                if (errno != EPIPE)
                    g_critical ("couldn't write data to socket: %s", strerror (errno));
                return -1;
            }
        }

        else {
            data += r;
            len -= r;
            x += r;
        }
    }

    return x;
}

/* Called when seahorse-actions has a response to send back */
void
seahorse_agent_io_reply (SeahorseAgentConn *cn, gboolean ok, const gchar *response)
{
    int fd;

    /* The connection could have closed in the meantime */
    if (!is_valid_conn (cn))
        return;

    DEBUG_AGENTIO (("[agent-io] send line:\n%s%s\n", ok ? ASS_OK : ASS_ERR, response ? response : ""));

    fd = g_io_channel_unix_get_fd (cn->iochannel);

    if (write_raw_data (fd, ok ? ASS_OK : ASS_ERR, ok ? KL (ASS_OK) : KL (ASS_ERR))
        == -1 || (response && write_raw_data (fd, response, -1)) == -1
        || write_raw_data (fd, NL, KL (NL)) == -1) {
        /* error message already printed */
        disconnect (cn);
    }

    /* After sending back a response we're ready for more */
    cn->input = TRUE;
    return;
}

/* Callback for new incoming connections */
static gboolean
connect_handler (GIOChannel *source, GIOCondition cond, gpointer data)
{
    SeahorseAgentConn *cn;
    int fd;

    g_assert (source);
    g_assert (cond == G_IO_IN);

    fd = accept (g_io_channel_unix_get_fd (source), NULL, NULL);
    if (fd < 0) {
        if (errno != EINTR && errno != EAGAIN)
            g_critical ("couldn't accept connection: %s", strerror (errno));
        return TRUE;            /* don't stop listening */
    }

    cn = g_chunk_new0 (SeahorseAgentConn, g_memory);

    g_connections = g_list_append (g_connections, cn);

    /* Setup io channel for new socket */
    cn->iochannel = g_io_channel_unix_new (fd);
    g_io_channel_set_close_on_unref (cn->iochannel, TRUE);
    g_io_channel_set_encoding (cn->iochannel, NULL, NULL);
    cn->stag = g_io_add_watch (cn->iochannel, G_IO_IN | G_IO_HUP, io_handler, cn);

    /* 
     * Initial response on the connection. This also enables
     * listening on the connection
     */
    seahorse_agent_io_reply (cn, TRUE, "your orders please");

    return TRUE;
}

/* Initialize the connection system */
int
seahorse_agent_io_init ()
{
    g_assert (g_socket != -1);
    g_assert (g_iochannel == NULL);
    g_assert (g_iochannel_tag == 0);

    /* Listen for connections */
    if (listen (g_socket, 5) == -1) {
        g_critical ("couldn't listen for connections on socket: %s",
                    strerror (errno));
        return -1;
    }

    /* Watch for connections on the socket */
    g_iochannel = g_io_channel_unix_new (g_socket);
    g_io_channel_set_close_on_unref (g_iochannel, TRUE);
    g_iochannel_tag = g_io_add_watch (g_iochannel, G_IO_IN, connect_handler, NULL);

    /* The main memory queue */
    g_memory = g_mem_chunk_create (SeahorseAgentConn, 8, G_ALLOC_AND_FREE);

    return 0;
}

/* Callback for freeing all connections */
static void
free_connections (gpointer data, gpointer user_data)
{
    free_conn ((SeahorseAgentConn *) data);
}

/* Close all connections, free memory etc... */
void
seahorse_agent_io_uninit ()
{
    if (g_connections) {
        g_list_foreach (g_connections, free_connections, NULL);
        g_list_free (g_connections);
        g_connections = NULL;
    }

    if (g_memory) {
        g_mem_chunk_destroy (g_memory);
        g_memory = NULL;
    }

    if (g_socket) {
        GError *err = NULL;
        gchar *t;

        g_source_remove (g_iochannel_tag);
        g_iochannel_tag = 0;
        g_io_channel_shutdown (g_iochannel, FALSE, &err);
        g_io_channel_unref (g_iochannel);
        g_iochannel = NULL;
        g_socket = -1;

        g_clear_error (&err);

        /* Remove the socket */
        unlink (g_socket_name);

        /* Remove the directory */
        t = strrchr (g_socket_name, '/');
        if (t != NULL) {
            *t = 0;
            rmdir (g_socket_name);
        }

        g_socket_name[0] = 0;
    }    
}
