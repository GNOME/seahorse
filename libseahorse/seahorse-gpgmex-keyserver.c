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

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <glib.h>
#include <gpgme.h>

#include "config.h"
#include "seahorse-gpgmex.h"

#ifdef WITH_KEYSERVER 

/* Forward Decls */
struct _ServerOp;
typedef struct _ServerOp ServerOp;

typedef gboolean (*OpDataFunc) (ServerOp *op, gchar *line);
typedef gpgme_error_t (*OpCompletionFunc) (ServerOp *op);
typedef void (*OpDoneFunc) (ServerOp *op);

#define READ_BATCH_SIZE 256
#define MAX_READ_LENGTH 0x100000

/* TODO: Read from gconf, preferably */
#define KEYSERVER_TIMEOUT 180

/* WARNING: Keep this up to date with gnupg's include/keyserver.h */
#define KEYSERVER_OK               0 /* not an error */
#define KEYSERVER_INTERNAL_ERROR   1 /* gpgkeys_ internal error */
#define KEYSERVER_NOT_SUPPORTED    2 /* operation not supported */
#define KEYSERVER_VERSION_ERROR    3 /* VERSION mismatch */
#define KEYSERVER_GENERAL_ERROR    4 /* keyserver internal error */
#define KEYSERVER_NO_MEMORY        5 /* out of memory */
#define KEYSERVER_KEY_NOT_FOUND    6 /* key not found */
#define KEYSERVER_KEY_EXISTS       7 /* key already exists */
#define KEYSERVER_KEY_INCOMPLETE   8 /* key incomplete (EOF) */
#define KEYSERVER_UNREACHABLE      9 /* unable to contact keyserver */

/* Keyserver plugin strings */
#define SEARCH_PREFIX "SEARCH "
#define TOTAL_PREFIX "COUNT "
#define VERSION_PREFIX "VERSION "
#define PLUGIN_VERSION "0"

/* A ServerOp structure is used to perform an operation against
 * a keyserver plugin. It can be derived from as seen below */
struct _ServerOp {
 
    /* The context we're working with */
    gpgme_ctx_t ctx;

    /* The timeout source tag */
    gint timeout_stag;

    /* All stderr output from the child is concatenated onto this string. In
     * the case of an error it's used to form an error message */
    GString *err_output;
    
    /* The result code for this operation */
    gpgme_error_t status;
    
    /* Plugin IO ------------------------------------------------------------ */
    
    /* The plugin child's process id */
    GPid pid;                          
    
    /* Standard output from child. */
    GIOChannel *outio;
    guint outstag;
    
    /* Standard error from child */
    GIOChannel *errio;
    guint errstag;
    
    /* The current mode of the output */
    gboolean pre_mode;
    
    /* Sends data here  */
    OpDataFunc datafunc;        
       
    /* A line buffer for data */
    GString *linebuf;
           
    /* Callbacks ------------------------------------------------------------ */
   
    /* Called when completely done with this operation. This is the 
     * callback we got from our caller */
    gpgmex_keyserver_done_cb done_cb;   
    
    /* Data for callbacks */
    void *userdata;
};

/* A derived operation structure for retrieving keys */
typedef struct _RetrieveOp {
    ServerOp op;            /* The base struct */
    gpgme_data_t data;      /* Puts all data here */
    gboolean key_mode;      /* In the key or not */
} RetrieveOp;

/* A derived operation structure for key listing */
typedef struct _KeyListOp {
    ServerOp op;                        /* The base class */
    guint total;                        /* Total number of keys to list */
    gpgmex_keyserver_list_cb list_cb;   /* Call with keys */
} KeyListOp;


/* -----------------------------------------------------------------------------
 *  KEY FUNCTIONALITY 
 */

/* Key list field indexes */
enum {
    FIELD_FINGERPRINT,
    FIELD_USERID,
    FIELD_FLAGS,
    FIELD_KEYCREATED,
    FIELD_KEYEXPIRES,
    FIELD_ENTRYMODIFIED,
    FIELD_KEYTYPE,
    FIELD_KEYLENGTH,
    FIELD_END
};

static gpgme_pubkey_algo_t
parse_algo (const gchar *algo)
{
    if (g_ascii_strcasecmp (algo, "DH/DSS") || 
        g_ascii_strcasecmp (algo, "Elg") ||
        g_ascii_strcasecmp (algo, "Elgamal"))
        return GPGME_PK_ELG;
    if (g_ascii_strcasecmp (algo, "RSA"))
        return GPGME_PK_RSA;
    if (g_ascii_strcasecmp (algo, "DSA"))
        return GPGME_PK_DSA;

    return 0;
}

static void
parse_key_line (KeyListOp *kop, gchar *line)
{
    gpgme_pubkey_algo_t algo;
    guint flags;
    long int timestamp;
    long int expires;
    unsigned int length;
    gpgme_key_t key;
    
    gboolean invalid = FALSE;
    gchar **fields;
    gchar **f;
    gchar *t;
    gint i;
    
    fields = g_strsplit (line, ":", FIELD_END);    
    for (f = fields, i = 0; f && *f && !invalid; f++, i++) {
        
        t = *f;
        
        switch (i)
        {
        case FIELD_FINGERPRINT:
            if (t[0] == 0)
                invalid = TRUE;
            break;
        case FIELD_USERID:
            /* TODO: Decode user ID */
            if (t[0] == 0)
                invalid = TRUE;
            break;
        case FIELD_FLAGS:
            flags = atoi (t);
            break;
        case FIELD_KEYCREATED:
            timestamp = atol (t);
            if (timestamp <= 0)
                invalid = TRUE;
            break;
        case FIELD_KEYEXPIRES:
            expires = atol (t);
            if (expires < 0)
                invalid = TRUE;
            break;
        case FIELD_KEYTYPE:
            algo = parse_algo (t);
            if (algo == 0)
                invalid = TRUE;
            break;
        case FIELD_KEYLENGTH:
            length = atoi (t);
            if (length <= 0)
                invalid = TRUE;
            break;
        case FIELD_ENTRYMODIFIED:
            break;            
        case FIELD_END:
        default:
            invalid = TRUE;
            break;
        }
    }
    
    if (i != FIELD_END)
        invalid = TRUE;
    
    if (invalid) {
        g_warning ("invalid key line from key server plugin: %s", line);
        return;
    }
    
    key = gpgmex_key_alloc ();
    gpgmex_key_add_subkey (key, fields[FIELD_FINGERPRINT], flags, timestamp, 
                           expires, length, algo);
    gpgmex_key_add_uid (key, fields[FIELD_USERID], flags);
    
    g_strfreev (fields);
    
    g_assert (kop->list_cb);
    (kop->list_cb) (kop->op.ctx, kop, key, kop->total, kop->op.userdata);
}
 
/* -----------------------------------------------------------------------------
 *  IO HANDLING 
 */

static void
close_io (GIOChannel **io, guint* stag)
{
    g_return_if_fail (io && stag);
    if (*io) {
        g_io_channel_shutdown (*io, TRUE, NULL);
        g_io_channel_unref (*io);
        *io = NULL;
    }
    if (*stag) {
        g_source_remove (*stag);
        *stag = 0;
    }
}

/* Called to complete any server op. In addition frees 
 * the ServerOp */
static void
complete_op (ServerOp *op)
{
    const gchar *message = NULL;
    int status = 0;
    
    /* When we're failing we kill the program */
    if (!GPG_IS_OK (op->status))
        kill ((pid_t)op->pid, SIGTERM);
        
    /* Wait for the process to exit, and check result */
    if (waitpid((pid_t)op->pid, &status, 0) == -1)
        g_critical ("couldn't wait on process");
        
    /* TODO: We really should be parsing the result code */
    else if(!WIFEXITED(status) || WEXITSTATUS(status) != KEYSERVER_OK) {
        /* The process failed, handle as a failure */
        if (GPG_IS_OK (op->status)) {
            op->status = GPG_E (GPG_ERR_KEYSERVER);
            if (op->err_output)
                message = op->err_output->str;
        }
    }

    g_spawn_close_pid (op->pid);
   
    /* Call the user callback */
    if (op->done_cb)
        (op->done_cb) (op->ctx, op, op->status, message, op->userdata);
    
    /* Now go about freeing everything */
    if (op->err_output)
        g_string_free (op->err_output, TRUE);
    
    if (op->timeout_stag)
        g_source_remove (op->timeout_stag);
    
    close_io (&(op->outio), &(op->outstag));
    close_io (&(op->errio), &(op->errstag));
    g_free (op);
}

static gboolean
complete_op_later (ServerOp *op)
{
    complete_op (op);
    return FALSE; /* Remove timeout source */
}

static gboolean
premode_out_data (ServerOp *op, const gchar *line)
{
    const gchar *t;
    
    /* VERSION 0 */
    if (g_str_has_prefix (line, VERSION_PREFIX)) {
        t = line + strlen (VERSION_PREFIX);
        while (*t && g_ascii_isspace (*t))
            t++;
        if (strcmp (t, PLUGIN_VERSION) != 0) {
            op->status = GPG_E(GPG_ERR_INV_ENGINE);
            complete_op (op);
            return FALSE;
        }
    }
    
    /* Blank line */
    else if (line[0] == 0)
        op->pre_mode = FALSE;
    
    return TRUE;
}

/* Sends lines to the appropriate place based on the current
 * operation. */
static gboolean
process_output_lines (ServerOp *op, gboolean flush)
{
    gboolean ret;
    gchar *e;
    
    /* Parse each line in the buffer individually */
    while ((e = strchr (op->linebuf->str, '\n')) != NULL || flush) { 
      
        if (e != NULL)
            *e = 0;
        else
            flush = FALSE;
                 
        if (op->pre_mode)
            ret = premode_out_data (op, op->linebuf->str);
        else if (op->datafunc)
            ret = (op->datafunc) (op, op->linebuf->str);
        else
            ret = TRUE;
            
        if (e != NULL)
            op->linebuf = g_string_erase (op->linebuf, 0, (e - op->linebuf->str) + 1);
           
        if (!ret)
            return FALSE;
    }

    return TRUE;
}

/* Called when a file descriptor closes to see if we're
 * done with the child. Waits for child and completes if so */
static void
check_op_complete (ServerOp *op)
{
    if (op->outio || op->errio)
        return;
        
    /* Flush any remaining output data */
    process_output_lines (op, TRUE);
        
    complete_op (op);
}

/* Callback for a key search operation */
static gboolean
keylist_out_data (ServerOp *op, gchar *line)
{
    KeyListOp *kop = (KeyListOp*)op;

    g_return_val_if_fail (kop != NULL, FALSE);
    g_return_val_if_fail (line != NULL, FALSE);
  
    /* No use doing anything if nobody's listening */
    if (kop->list_cb == NULL)
        return TRUE;

    if (line[0] == 0) {
        g_warning ("invalid output line");
        
    } else if (g_str_has_prefix (line, SEARCH_PREFIX)) {
        ;
        
    } else if (g_str_has_prefix (line, TOTAL_PREFIX)) {
        line += strlen (TOTAL_PREFIX);
        kop->total = atoi (line);
        
    } else {
        parse_key_line (kop, line);
    }

    return TRUE;
}

/* Callback for data from child when retrieving a key */
static gboolean
retrieve_out_data (ServerOp *op, gchar *line)
{
    RetrieveOp *rop = (RetrieveOp*)op;
    
    if (g_str_has_prefix (line, "-----BEGIN"))
        rop->key_mode = TRUE;
    else if (g_str_has_prefix (line, "-----END"))
        rop->key_mode = FALSE;

    if (rop->key_mode) {        
        /* Append to our data block */
        g_return_val_if_fail (rop->data != NULL, FALSE);
        if (gpgme_data_write (rop->data, line, strlen(line)) == -1) {
            g_critical ("couldn't write to data block");
            return FALSE;
        }
    }
    
    return TRUE;
}

static void
buffer_error_message (ServerOp *op, const gchar *data)
{
    const gchar *t = data;
    const gchar *p;
    guint len = strlen (data);

    /* We're looking for any text after 'gpg[a-z_]:' */
    
    while ((t = g_strrstr_len (data, len - (t - data), "gpg")) != NULL) {
        
        p = t;
        
        while (*p && (*p == ':' || !g_ascii_isspace (*p))) {
            p++;
        
            if (*p == ':') {
               
                while (*p && (*p == ':' || g_ascii_isspace (*p)))
                    p++;
                    
                op->err_output = g_string_assign (op->err_output, p);
                return;
            }
        }
    }
    
    op->err_output = g_string_append (op->err_output, data);
}

/* Called when input is available on child's stdout or stderr.
 * Sends data to operation specific callbacks above */
static gboolean
stderr_handler (GIOChannel *source, GIOCondition condition, gpointer data)
{
    ServerOp *op = (ServerOp*)data;
    gchar buf[READ_BATCH_SIZE + 1];
    gboolean ret = TRUE;
    GError *err = NULL;
    gsize read;
    
    if (condition & G_IO_IN) {
        do {
            g_io_channel_read_chars (source, buf, READ_BATCH_SIZE, &read, &err);
            
            if(err != NULL) {
                g_critical ("couldn't read data from child: %s", err->message);
                g_clear_error (&err);
                ret = FALSE; /* Remove this io handler */
                break;
            }
            
            /* Append error output to that buffer */
            if (op->err_output->len >= MAX_READ_LENGTH) {
                g_critical ("too much data from child");
                ret = FALSE;
                break;
            }
            
            buf[read] = 0;
            buffer_error_message (op, buf);
        }
        while (read > 0);
    }
    
    if (condition & G_IO_HUP) 
        ret = FALSE;
    
    /* Check for completion */
    if (ret == FALSE) {
        close_io (&(op->errio), &(op->errstag));
        check_op_complete (op);
    }
        
    return ret;
}

/* Called when input is available on child's stdout.
 * Sends data to operation specific callbacks above */
static gboolean
stdout_handler (GIOChannel *source, GIOCondition condition, gpointer data)
{
    ServerOp *op = (ServerOp*)data;
    gchar buf[READ_BATCH_SIZE];
    gboolean ret = TRUE;
    GError *err = NULL;
    gsize read;
    
    if (condition & G_IO_IN) {
        do {
            g_io_channel_read_chars (source, buf, READ_BATCH_SIZE, &read, &err);
            
            if(err != NULL) {
                g_critical ("couldn't read data from child: %s", err->message);
                g_clear_error (&err);
                ret = FALSE; /* Remove this io handler */
                break;
            }
            
            /* Add output to the line buffer */
            op->linebuf = g_string_append_len (op->linebuf, buf, read);
            if (!process_output_lines (op, FALSE)) {
                ret = FALSE;
                break;
            }
        }
        while (read > 0);
    }
    
    if (condition & G_IO_HUP) 
        ret = FALSE;
    
    /* Check for completion */
    if (ret == FALSE) {
        close_io (&(op->outio), &(op->outstag));
        check_op_complete (op);
    }
        
    return ret;
}

static gboolean 
timeout_handler (ServerOp *op)
{
    /* We've timed out, bummer */
    op->status = GPG_E (GPG_ERR_TIMEOUT);
    complete_op (op);
    return FALSE;
}

/* Code adapted from GnuPG (file g10/keyserver.c) */
static gboolean 
parse_keyserver_uri (char *uri, char **scheme, char **host,
                     char **port, char **opaque)
{
    int assume_hkp=0;

    g_return_val_if_fail (uri != NULL, FALSE);
    g_return_val_if_fail (scheme != NULL && host != NULL && port != NULL &&
                          opaque != NULL, FALSE);

    *host = NULL;
    *port = NULL;
    *opaque = NULL;

    /* Get the scheme */

    *scheme = strsep(&uri, ":");
    if (uri == NULL) {
      /* Assume HKP if there is no scheme */
      assume_hkp = 1;
      uri = *scheme;
      *scheme = "hkp";
    }

    if (assume_hkp || (uri[0] == '/' && uri[1] == '/')) {
        /* Two slashes means network path. */

        /* Skip over the "//", if any */
        if (!assume_hkp)
            uri += 2;

        /* Get the host */
        *host = strsep (&uri, ":/");
        if (*host[0] == '\0')
            return FALSE;

        if (uri == NULL || uri[0] == '\0')
            *port = NULL;
        else {
            char *ch;

            /* Get the port */
            *port = strsep (&uri, "/");

            /* Ports are digits only */
            ch = *port;
            while (*ch != '\0') {
                if (!g_ascii_isdigit (*ch))
                    return FALSE;

                ch++;
            }
        }

    /* (any path part of the URI is discarded for now as no keyserver
       uses it yet) */
    } else if (uri[0] != '/') {
        /* No slash means opaque.  Just record the opaque blob and get out. */
        *opaque = uri;
        return TRUE;
    } else {
        /* One slash means absolute path.  We don't need to support that yet. */
        return FALSE;
    }

    if (*scheme[0] == '\0')
        return FALSE;

    return TRUE;
}

static gchar *
keyserver_uri_to_commands (const gchar *uri, GString *commands)
{
    gchar *u;
    gchar *scheme;
    gchar *host;
    gchar *port;
    gchar *opaque;
    gchar *ret = NULL;

    g_return_val_if_fail (uri != NULL, NULL);
    u = g_strdup (uri);
    
    if (parse_keyserver_uri (u, &scheme, &host, &port, &opaque)) {
        
        g_return_val_if_fail (host != NULL, NULL);
        g_string_append_printf (commands, "HOST %s\n", host);
        
        if (port)
            g_string_append_printf (commands, "PORT %s\n", port);
            
        if (opaque)
            g_string_append_printf (commands, "OPAQUE %s\n", opaque);

        ret = g_strdup_printf ("%s/gpgkeys_%s", GPGKEYS_PLUGIN_PATH, scheme);
    }
    
    g_free (u);
    return ret;
}

static gpgme_error_t
write_all_data (int fd, const gchar *data)
{
    gsize len;
    int x;
    
    len = strlen (data);
    while (len > 0) {
        if ((x = write (fd, data, len)) == -1) {
           if (errno != EINTR && errno != EAGAIN) 
                return gpgme_error_from_errno (errno);
        }
        
        len -= x;
        data += x;
    }
    
    return GPG_OK;
}

static void
reap_program (GPid pid)
{
    /* Bye, bye program */
    kill (pid, SIGTERM);
    waitpid (pid, NULL, 0);
}

static gpgme_error_t
spawn_keyserver_plugin (const gchar *app, const gchar *input, GPid *pid, 
                        GIOChannel **output, GIOChannel **errout)
{
    GError *err = NULL;
    gpgme_error_t gerr;
    gchar *argv[2];
    gint infd;
    gint outfd;
    gint errfd;
    
    g_assert (app != NULL);
    g_assert (pid != NULL);
    g_assert (output != NULL);
    g_assert (errout != NULL);
    
    *output = NULL;
    *errout = NULL;
    *pid = 0;
    
    argv[0] = (gchar*)app;
    argv[1] = NULL;
    
    g_print ("SENDING TEXT TO '%s': \n%s", app, input);
  
    /* Start the process */
    g_spawn_async_with_pipes (NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
                              NULL, NULL, pid, &infd, &outfd, &errfd, &err);

    if (err != NULL) {
        g_warning ("error running key server plugin: %s: %s", app, err->message);
        return GPG_E (GPG_ERR_INV_ENGINE);
    }
    
    /* Note that past this point we don't return haphazardly */

    /* Now write all the data on input in, and close */
    gerr = write_all_data (infd, input ? input : "");
    close (infd);
    
    
    if (GPG_IS_OK (gerr)) {
                
        *output = g_io_channel_unix_new (outfd);
        g_io_channel_set_encoding (*output, NULL, NULL);
        g_io_channel_set_close_on_unref (*output, TRUE);
        
        *errout = g_io_channel_unix_new (errfd);
        g_io_channel_set_encoding (*errout, NULL, NULL);
        g_io_channel_set_close_on_unref (*errout, TRUE);

    } else {
       
        reap_program (*pid);
        *pid = 0;
    }

    return gerr;    
}

static void
setup_operation (ServerOp *op, gpgme_ctx_t ctx, gpgmex_keyserver_done_cb dcb,
                 void *userdata, OpDataFunc ofunc)
{
    op->ctx = ctx;
    op->done_cb = dcb;
    op->userdata = userdata;
    op->err_output = g_string_sized_new (128);
    op->linebuf = g_string_sized_new (128);
    op->pre_mode = TRUE;
    op->datafunc = ofunc;
    op->status = GPG_OK;
}                  

static void
start_operation (ServerOp *op, GPid pid, GIOChannel *output, GIOChannel *errout)
{
    op->pid = pid;
    
    op->outio = output;
    op->outstag = g_io_add_watch (output, G_IO_IN | G_IO_HUP, stdout_handler, op);
    g_io_channel_set_flags (output, g_io_channel_get_flags (output) | G_IO_FLAG_NONBLOCK, NULL);

    op->errio = errout;
    op->errstag = g_io_add_watch (errout, G_IO_IN | G_IO_HUP, stderr_handler, op);
    g_io_channel_set_flags (errout, g_io_channel_get_flags (errout) | G_IO_FLAG_NONBLOCK, NULL);

    op->timeout_stag = g_timeout_add (KEYSERVER_TIMEOUT * 1000, 
                            (GSourceFunc)timeout_handler, op);
}

void       
gpgmex_keyserver_start_retrieve (gpgme_ctx_t ctx, const char *server, const char *fpr,
                                 gpgme_data_t data, gpgmex_keyserver_done_cb dcb, 
                                 void *userdata, gpgmex_keyserver_op_t *op)
{
    GIOChannel *output;
    GIOChannel *errout;
    gpgme_error_t gerr = GPG_OK;
    GString *commands;
    RetrieveOp *rop;
    gchar *plugin;
    gchar *cmds;
    GPid pid;
    
    g_return_if_fail (ctx != NULL);
    g_return_if_fail (gpgme_get_protocol(ctx) == GPGME_PROTOCOL_OpenPGP);
    g_return_if_fail (server != NULL && server[0] != 0);
    g_return_if_fail (fpr != NULL && fpr[0] != 0);
    
    /* Setup retrieve stuff */        
    rop = g_new0(RetrieveOp, 1);
    rop->data = data;
            
    /* And now the base stuff */
    g_assert (((ServerOp*)rop) == &(rop->op));
    setup_operation (&(rop->op), ctx, dcb, userdata, retrieve_out_data);
                                 
    /* Start building command set */
    commands = g_string_sized_new (128);
    plugin = keyserver_uri_to_commands (server, commands);

    if (plugin == NULL) {
        g_string_free (commands, TRUE);
        gerr = GPG_E (GPG_ERR_BAD_URI);
    }
    
    if (GPG_IS_OK (gerr)) {
        g_string_append_printf (commands, "COMMAND GET\n\n%s\n\n", fpr);
        cmds = g_string_free (commands, FALSE);
        
        /* Now execute the command */
        gerr = spawn_keyserver_plugin (plugin, cmds, &pid, &output, &errout);
        g_free (plugin);
        g_free (cmds);
    }
        
    if (GPG_IS_OK (gerr)) 
        start_operation (&(rop->op), pid, output, errout);    
    else {
        rop->op.status = gerr;
        g_timeout_add (0, (GSourceFunc)complete_op_later, &(rop->op));
    }
    
    if (op)
        *op = &(rop->op);
}

void       
gpgmex_keyserver_start_list (gpgme_ctx_t ctx, const char *server, const char *pattern, 
                             unsigned int flags, gpgmex_keyserver_list_cb lcb, 
                             gpgmex_keyserver_done_cb dcb, void *userdata, 
                             gpgmex_keyserver_op_t *op)
{
    GIOChannel *output;
    GIOChannel *errout;
    gpgme_error_t gerr = GPG_OK;
    GString *commands;
    KeyListOp *kop;
    gchar *plugin;
    gchar *cmds;
    GPid pid;
    
    g_return_if_fail (ctx != NULL);
    g_return_if_fail (gpgme_get_protocol(ctx) == GPGME_PROTOCOL_OpenPGP);
    g_return_if_fail (server != NULL && server[0] != 0);
    g_return_if_fail (pattern != NULL && pattern[0] != 0);

    /* Setup key list stuff */        
    kop = g_new0(KeyListOp, 1);
    kop->list_cb = lcb;
    
    /* And now the base stuff */
    g_assert (((ServerOp*)kop) == &(kop->op));
    setup_operation (&(kop->op), ctx, dcb, userdata, keylist_out_data);
                             
    /* Start building command set */
    commands = g_string_sized_new (128);
    plugin = keyserver_uri_to_commands (server, commands);
    
    // TODO: Check at this point to make sure binary exists and is executable 
    if (plugin == NULL) {
        g_string_free (commands, TRUE);
        gerr = GPG_E (GPG_ERR_BAD_URI);
    }
    
    if (GPG_IS_OK (gerr)) {
        if (flags & GPGMEX_KEYLIST_REVOKED)
            g_string_append_printf (commands, "OPTION include-revoked\n");
        if (flags & GPGMEX_KEYLIST_SUBKEYS)
            g_string_append_printf (commands, "OPTION include-subkeys\n");
        
        g_string_append_printf (commands, "COMMAND SEARCH\n\n%s\n\n", pattern);
        cmds = g_string_free (commands, FALSE);
        
        /* Now execute the command */
        gerr = spawn_keyserver_plugin (plugin, cmds, &pid, &output, &errout);
        g_free (plugin);
        g_free (cmds);
    }
    
    if (GPG_IS_OK (gerr)) 
        start_operation (&(kop->op), pid, output, errout);    
    else {
        kop->op.status = gerr;
        g_timeout_add (0, (GSourceFunc)complete_op_later, &(kop->op));
    }
    
    if (op)
        *op = &(kop->op);    
}

void
gpgmex_keyserver_cancel (gpgmex_keyserver_op_t op)
{
    ServerOp *sop = (ServerOp*)op;
    sop->status = GPG_E (GPG_ERR_CANCELED);
    complete_op (sop);
}

#endif /* WITH_KEYSERVER */
