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
#include <sys/wait.h>
#include <gnome.h>
#include <fcntl.h>

#include "seahorse-ssh-operation.h"
#include "seahorse-util.h"
#include "seahorse-gpgmex.h"

#ifndef DEBUG_OPERATION_ENABLE
#if _DEBUG
#define DEBUG_OPERATION_ENABLE 1
#else
#define DEBUG_OPERATION_ENABLE 0
#endif
#endif

#if DEBUG_OPERATION_ENABLE
#define DEBUG_OPERATION(x)  g_printerr x
#else
#define DEBUG_OPERATION(x)
#endif

/* -----------------------------------------------------------------------------
 * DEFINITIONS
 */
 
typedef void (*ResultCallback) (SeahorseSSHOperation *sop);

typedef struct _SeahorseSSHOperationPrivate {
    GString *sin;
    guint win;
    GIOChannel *iin;
    GString *sout;
    guint wout;
    GIOChannel *iout;
    GString *serr;
    guint werr;
    GIOChannel *ierr;
    GPid pid;
    guint wpid;
    gchar *message;
    ResultCallback result_cb;
} SeahorseSSHOperationPrivate;

enum {
    PROP_0,
    PROP_KEY_SOURCE,
    PROP_MESSAGE
};

#define SEAHORSE_SSH_OPERATION_GET_PRIVATE(obj)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SEAHORSE_TYPE_SSH_OPERATION, SeahorseSSHOperationPrivate))

/* TODO: This is just nasty. Gotta get rid of these weird macros */
IMPLEMENT_OPERATION_PROPS(SSH, ssh)

    g_object_class_install_property (gobject_class, PROP_KEY_SOURCE,
        g_param_spec_object ("key-source", "SSH Key Source", "Key source this operation works on.", 
                             SEAHORSE_TYPE_SSH_SOURCE, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_MESSAGE,
        g_param_spec_string ("message", "Progress Message", "Progress message for display in progress dialog.", 
                             NULL, G_PARAM_READABLE | G_PARAM_WRITABLE));   

    g_type_class_add_private (gobject_class, sizeof (SeahorseSSHOperationPrivate));

END_IMPLEMENT_OPERATION_PROPS

/* -----------------------------------------------------------------------------
 * HELPERS
 */

static void 
watch_ssh_process (GPid pid, gint status, SeahorseSSHOperation *sop)
{
    SeahorseSSHOperationPrivate *pv = SEAHORSE_SSH_OPERATION_GET_PRIVATE (sop);
    
    DEBUG_OPERATION (("SSHOP: SSH process done\n"));
    
    if (!seahorse_operation_is_done (SEAHORSE_OPERATION (sop))) {

        /* Was killed */
        if (!WIFEXITED (status)) {
            seahorse_operation_mark_done (SEAHORSE_OPERATION (sop), FALSE, 
                g_error_new (SEAHORSE_ERROR, 0, _("The SSH command was terminated unexpectedly.")));
            
        /* Command failed */
        } else if (WEXITSTATUS (status) != 0) {
            g_warning ("SSH command failed: (%d)", WEXITSTATUS (status));
            if (pv->serr->len)
                g_warning ("SSH error output: %s", pv->serr->str);
            seahorse_operation_mark_done (SEAHORSE_OPERATION (sop), FALSE, 
                g_error_new_literal (SEAHORSE_ERROR, 0, pv->serr->len ? pv->serr->str : _("The SSH command failed.")));

        /* Successful completion */
        } else {
            
            /*
             * If a result callback is set (by one of our specific operations below)
             * then we let it setup the result. Otherwise use the output string.
             */
            if (pv->result_cb)
                (pv->result_cb) (sop);
            else
                seahorse_operation_mark_result (SEAHORSE_OPERATION (sop), pv->sout->str, NULL);
            
            seahorse_operation_mark_done (SEAHORSE_OPERATION (sop), FALSE, NULL);
        }
    }

    g_spawn_close_pid (pid);
    pv->pid = 0;
    pv->wpid = 0;
    
    /* This watch holds a ref on the operation, release */
    g_object_unref (sop);
}

static gboolean    
io_ssh_write (GIOChannel *source, GIOCondition condition, SeahorseSSHOperation *sop)
{
    SeahorseSSHOperationPrivate *pv = SEAHORSE_SSH_OPERATION_GET_PRIVATE (sop);
    GError *error = NULL;
    GIOStatus status;
    gsize written = 0;

    if (!seahorse_operation_is_done (SEAHORSE_OPERATION (sop)) && pv->sin) {
        DEBUG_OPERATION (("SSHOP: SSH ready for input\n"));
        
        status = g_io_channel_write_chars (pv->iin, pv->sin->str, pv->sin->len,
                                           &written, &error);
        switch (status) {
        case G_IO_STATUS_ERROR:
            seahorse_operation_mark_done (SEAHORSE_OPERATION (sop), FALSE, error);
            break;
        case G_IO_STATUS_AGAIN:
            break;
        default:
            DEBUG_OPERATION (("SSHOP: Wrote %d bytes to SSH\n", written));
            g_string_erase (pv->sin, 0, written);
            break;
        }
    }
    
    if (pv->sin && !pv->sin->len) {
        DEBUG_OPERATION (("SSHOP: Finished writing SSH input\n"));
        g_string_free (pv->sin, TRUE);
        pv->sin = NULL;
    }
    
    if (seahorse_operation_is_done (SEAHORSE_OPERATION (sop)) || !pv->sin) {
        DEBUG_OPERATION (("SSHOP: Closing SSH input channel\n"));
        g_io_channel_unref (pv->iin);
        pv->iin = NULL;
        g_source_remove (pv->win);
        pv->win = 0;
        return FALSE;
    }
    
    return TRUE;
}

static gboolean    
io_ssh_read (GIOChannel *source, GIOCondition condition, SeahorseSSHOperation *sop)
{
    SeahorseSSHOperationPrivate *pv = SEAHORSE_SSH_OPERATION_GET_PRIVATE (sop);
    GError *error = NULL;
    gchar buf[128];
    GIOStatus status;
    gsize read = 0;
    GString *str;
    
    if (seahorse_operation_is_done (SEAHORSE_OPERATION (sop)))
        return TRUE;
    
    /* Figure out which buffer we're writing into */
    if (source == pv->iout) {
        str = pv->sout;
        DEBUG_OPERATION (("SSHOP: SSH output: "));    
    } else if(source == pv->ierr) {
        str = pv->serr;
        DEBUG_OPERATION (("SSHOP: SSH errout: "));
    } else
        g_assert_not_reached ();

    do {
        status = g_io_channel_read_chars (source, buf, sizeof (buf), &read, &error);
        switch (status) {
        case G_IO_STATUS_ERROR:
            seahorse_operation_mark_done (SEAHORSE_OPERATION (sop), FALSE, error);
            break;
        case G_IO_STATUS_AGAIN:
            continue;
        case G_IO_STATUS_EOF:
            break;
        default:
            g_string_append_len (str, buf, read);
            DEBUG_OPERATION (("%s\n", str->str + (str->len - read)));
            break;
        }
    } while (read == sizeof (buf));
    
    return TRUE;
}

static void
ssh_child_setup (gpointer user_data)
{
    /* Install our askpass program if none present */
    g_setenv ("SSH_ASKPASS", EXECDIR "seahorse-ssh-askpass", 0);
    
    /* No terminal for this process */
    setsid ();
}

static void
ssh_sync_child_setup (gpointer user_data)
{
    /* No terminal for this process */
    setsid ();
}

static const gchar*
get_algorithm_text (guint algo)
{
    switch (algo) {
    case SSH_ALGO_DSA:
        return "dsa";
    case SSH_ALGO_RSA:
        return "rsa";
    case SSH_ALGO_RSA1:
        return "rsa1";
    default:
        g_return_val_if_reached (NULL);
        break;
    }
}

static gchar*
find_nonexistant_ssh_filename (SeahorseSSHSource *src, guint type)
{
    const gchar *algo;
    gchar *filename;
    gchar *dir;
    guint i = 0;
    
    g_object_get (src, "base-directory", &dir, NULL);
    g_return_val_if_fail (dir, NULL);
    
    switch (type) {
    case SSH_ALGO_DSA:
        algo = "id_dsa";
        break;
    case SSH_ALGO_RSA:
        algo = "id_rsa";
        break;
    case SSH_ALGO_RSA1:
        algo = "identity";
        break;
    default:
        g_return_val_if_reached (NULL);
        break;
    }
    
    for (i = 0; i < ~0; i++) {
        if (i == 0)
            filename = g_strdup_printf ("%s/%s", dir, algo);
        else
            filename = g_strdup_printf ("%s/%s.%d", dir, algo, i);
        
        if (!g_file_test (filename, G_FILE_TEST_EXISTS))
            break;
        
        g_free (filename);
        filename = NULL;
    }
        
    return filename;
}

static gchar*
escape_shell_arg (const gchar *arg)
{
    guint len = 2;
    const gchar *t;
    gchar *e, *escaped;
    
    for (t = arg; *t; t++) {
        if (*t == '\'')
            len += 3;
        ++len;
    }
    
    escaped = g_new0 (gchar, len + 1);
    escaped[0] = '\'';
    
    for (t = arg, e = escaped + 1; *t; t++) {
        if (*t == '\'') {
            strcpy (e, "'\''");
            e += 4;
        } else {
            *e = *t;
            e++;
        }
    }

    g_assert (e < escaped + len);
    *e = '\'';
    
    return escaped;
}

static void
generate_result_cb (SeahorseSSHOperation *sop)
{
    SeahorseSSHKey *skey;
    const char *filename;
    
    filename = g_object_get_data (G_OBJECT (sop), "filename");
    g_return_if_fail (filename != NULL);
    
    /* The result of the operation is the key we generated */
    skey = seahorse_ssh_source_key_for_filename (sop->sksrc, filename);
    g_return_if_fail (SEAHORSE_IS_SSH_KEY (skey));
    
    seahorse_operation_mark_result (SEAHORSE_OPERATION (sop), skey, NULL);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void 
seahorse_ssh_operation_init (SeahorseSSHOperation *sop)
{
    SeahorseSSHOperationPrivate *pv = SEAHORSE_SSH_OPERATION_GET_PRIVATE (sop);
    pv->sout = g_string_new (NULL);
    pv->serr = g_string_new (NULL);
}

static void 
seahorse_ssh_operation_set_property (GObject *gobject, guint prop_id, 
                                     const GValue *value, GParamSpec *pspec)
{
    
}

static void 
seahorse_ssh_operation_get_property (GObject *gobject, guint prop_id, 
                                     GValue *value, GParamSpec *pspec)
{
    SeahorseSSHOperation *sop = SEAHORSE_SSH_OPERATION (gobject);
    SeahorseSSHOperationPrivate *pv = SEAHORSE_SSH_OPERATION_GET_PRIVATE (sop);
    
    switch (prop_id) {
    case PROP_KEY_SOURCE:
        g_value_set_object (value, sop->sksrc);
        break;
    case PROP_MESSAGE:
        g_value_set_string (value, pv->message);
        break;
    }
}

static void 
seahorse_ssh_operation_dispose (GObject *gobject)
{
    SeahorseOperation *op = SEAHORSE_OPERATION (gobject);
    if (!seahorse_operation_is_done (op))
        seahorse_ssh_operation_cancel (op);
    G_OBJECT_CLASS (operation_parent_class)->dispose (gobject);  
}

static void 
seahorse_ssh_operation_finalize (GObject *gobject)
{
    SeahorseSSHOperation *sop = SEAHORSE_SSH_OPERATION (gobject);
    SeahorseSSHOperationPrivate *pv = SEAHORSE_SSH_OPERATION_GET_PRIVATE (sop);
    
    if (pv->win)
        g_source_remove (pv->win);
    if (pv->wout)
        g_source_remove (pv->wout);
    if (pv->werr)
        g_source_remove (pv->werr);

    if (pv->iin)
        g_io_channel_unref (pv->iin);
    if (pv->iout)
        g_io_channel_unref (pv->iout);
    if (pv->ierr)
        g_io_channel_unref (pv->ierr);
        
    if (pv->sin)
        g_string_free (pv->sin, TRUE);
    g_string_free (pv->sout, TRUE);
    g_string_free (pv->serr, TRUE);
    
    /* watch_ssh_process always needs to have been called */
    g_assert (pv->pid == 0 && pv->wpid == 0);
    
    g_free (pv->message);
    pv->message = NULL;
        
    G_OBJECT_CLASS (operation_parent_class)->finalize (gobject);  
}

static void 
seahorse_ssh_operation_cancel (SeahorseOperation *operation)
{
    SeahorseSSHOperation *sop = SEAHORSE_SSH_OPERATION (operation);    
    SeahorseSSHOperationPrivate *pv = SEAHORSE_SSH_OPERATION_GET_PRIVATE (sop);

    seahorse_operation_mark_done (operation, TRUE, NULL);

    if (pv->pid != 0)
        kill (pv->pid, SIGTERM);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseOperation*
seahorse_ssh_operation_new (SeahorseSSHSource *ssrc, const gchar *command, 
                            const gchar *input, gint length, const gchar *progress)
{
    SeahorseSSHOperationPrivate *pv;
    SeahorseSSHOperation *sop;
    GError *error = NULL;
    int argc, r;
    int fin, fout, ferr;
    char **argv;
    
    g_return_val_if_fail (SEAHORSE_IS_SSH_SOURCE (ssrc), NULL);
    g_return_val_if_fail (command && command[0], NULL);
    
    if (!g_shell_parse_argv (command, &argc, &argv, NULL)) {
        g_critical ("couldn't parse ssh command line: %s\n", command);
        g_return_val_if_reached (NULL);
    }
    
    sop = g_object_new (SEAHORSE_TYPE_SSH_OPERATION, NULL);
    pv = SEAHORSE_SSH_OPERATION_GET_PRIVATE (sop);

    sop->sksrc = ssrc;
    
    DEBUG_OPERATION (("SSHOP: Executing SSH command: %s\n", command));
    
    r = g_spawn_async_with_pipes (NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, 
                                  ssh_child_setup, NULL, &pv->pid, 
                                  input ? &fin : NULL, &fout, &ferr, &error);
    g_strfreev (argv);  

    if (!r)
        return seahorse_operation_new_complete (error);
    
    /* Copy the input for later writing */
    if (input) {
        pv->sin = g_string_new_len (input, length == -1 ? strlen (input) : length);
        DEBUG_OPERATION (("SSHOP: Will send SSH input: %s", pv->sin->str));    
        
        fcntl (fin, F_SETFL, O_NONBLOCK | fcntl (fin, F_GETFL));
        pv->iin = g_io_channel_unix_new (fin);
        g_io_channel_set_encoding (pv->iin, NULL, NULL);
        g_io_channel_set_close_on_unref (pv->iin, TRUE);
        pv->win = g_io_add_watch (pv->iin, G_IO_OUT, (GIOFunc)io_ssh_write, sop);
    }
    
    /* Make all the proper IO Channels for the output/error */
    fcntl (fout, F_SETFL, O_NONBLOCK | fcntl (fout, F_GETFL));
    pv->iout = g_io_channel_unix_new (fout);
    g_io_channel_set_encoding (pv->iout, NULL, NULL);
    g_io_channel_set_close_on_unref (pv->iout, TRUE);
    pv->wout = g_io_add_watch (pv->iout, G_IO_IN, (GIOFunc)io_ssh_read, sop);
    
    fcntl (ferr, F_SETFL, O_NONBLOCK | fcntl (ferr, F_GETFL));
    pv->ierr = g_io_channel_unix_new (ferr);
    g_io_channel_set_encoding (pv->ierr, NULL, NULL);
    g_io_channel_set_close_on_unref (pv->ierr, TRUE);
    pv->werr = g_io_add_watch (pv->ierr, G_IO_IN, (GIOFunc)io_ssh_read, sop);
    
    /* Process watch */
    g_object_ref (sop); /* When the process ends, reference is released */
    pv->wpid = g_child_watch_add (pv->pid, (GChildWatchFunc)watch_ssh_process, sop);
    
    pv->message = g_strdup (progress);

    seahorse_operation_mark_start (SEAHORSE_OPERATION (sop));
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (sop), progress, 0, 0);
    
    return SEAHORSE_OPERATION (sop);
}

gchar*
seahorse_ssh_operation_sync (SeahorseSSHSource *ssrc, const gchar *command, 
                             GError **error)
{
    GError *err = NULL;
    gchar *sout, *serr;
    gint status;
    gint argc;
    gchar **argv;
    gboolean r;
    
    g_assert (!error || !*error);
    
    /* We use this internally, so guarantee it exists */
    if (!error)
        error = &err;
    
    if (!g_shell_parse_argv (command, &argc, &argv, NULL)) {
        g_critical ("couldn't parse ssh command line: %s\n", command);
        return NULL;
    }
    
    DEBUG_OPERATION (("SSHOP: executing SSH command: %s\n", command));
    
    r = g_spawn_sync (NULL, argv, NULL, 0, ssh_sync_child_setup, NULL, 
                      &sout, &serr, &status, error);
    g_strfreev (argv);
    
    if (!r) {
        g_critical ("couldn't execute SSH command: %s (%s)", command, *error ? (*error)->message : "");
        return NULL;
    }
    
    if (!WIFEXITED (status)) {
        g_critical ("SSH command didn't exit properly: %s", command);
        g_set_error (error, SEAHORSE_ERROR, 0, "%s", _("The SSH command was terminated unexpectedly."));
        g_free (sout);
        g_free (serr);
        return NULL;
    }
    
    if (WEXITSTATUS (status) != 0) {
        g_set_error (error, SEAHORSE_ERROR, 0, "%s", _("The SSH command failed."));
        g_warning ("SSH command failed: %s (%d)", command, WEXITSTATUS (status));
        if (serr && serr[0])
            g_warning ("SSH error output: %s", serr);
        g_free (serr);
        g_free (sout);
        return NULL;
    }
    
    g_free (serr);
    return sout;
}

SeahorseOperation*  
seahorse_ssh_operation_upload (SeahorseSSHSource *ssrc, GList *keys, 
                               const gchar *username, const gchar *hostname)
{
    SeahorseOperation *op;
    gpgme_data_t data;
    gpgme_error_t gerr;
    gchar *input;
    size_t length;
    gchar *cmd;
    
    g_return_val_if_fail (keys != NULL, NULL);
    g_return_val_if_fail (username && username[0], NULL);
    g_return_val_if_fail (hostname && hostname[0], NULL);
    
    gerr = gpgme_data_new (&data);
    g_return_val_if_fail (GPG_IS_OK (gerr), NULL);
    
    /* Buffer for what we send to the server */
    op = seahorse_key_source_export (SEAHORSE_KEY_SOURCE (ssrc), keys, FALSE, data);
    g_return_val_if_fail (op != NULL, NULL);
    
    /* 
     * We happen to know that seahorse_ssh_source_export always returns
     * completed operations, so we don't need to factor that in. If this
     * ever changes, then we need to recode this bit 
     */
    g_return_val_if_fail (seahorse_operation_is_done (op), NULL);
    
    /* Return any errors */
    if (!seahorse_operation_is_successful (op)) {
        gpgme_data_release (data);
        return op;
    }
    
    /* Free the export operation */
    g_object_unref (op);

    /* 
     * This command creates the .ssh directory if necessary (with appropriate permissions) 
     * and then appends all input data onto the end of .ssh/authorized_keys
     */
    /* TODO: Important, we should handle the host checking properly */
    cmd = g_strdup_printf (SSH_PATH " %s@%s -o StrictHostKeyChecking=no "
                                    "\"umask 077; test -d .ssh || mkdir .ssh ; cat >> .ssh/authorized_keys\"", 
                           username, hostname);
    input = gpgme_data_release_and_get_mem (data, &length);
    
    op = seahorse_ssh_operation_new (ssrc, cmd, input, length, _("Sending SSH public key..."));
    
    g_free (cmd);
    free (input);

    return op;
}

SeahorseOperation*
seahorse_ssh_operation_change_passphrase (SeahorseSSHKey *skey)
{
    SeahorseKeySource *ssrc;
    SeahorseOperation *op;
    gchar *cmd;
    
    g_return_val_if_fail (SEAHORSE_IS_SSH_KEY (skey), NULL);
    g_return_val_if_fail (skey->keydata && skey->keydata->filename, NULL);
    
    ssrc = seahorse_key_get_source (SEAHORSE_KEY (skey));
    g_return_val_if_fail (SEAHORSE_IS_SSH_SOURCE (ssrc), NULL);
    
    cmd = g_strdup_printf (SSH_KEYGEN_PATH " -p -f %s", skey->keydata->filename);
    op = seahorse_ssh_operation_new (SEAHORSE_SSH_SOURCE (ssrc), cmd, NULL, -1, NULL);
    g_free (cmd);
    
    return op;
}

SeahorseOperation*   
seahorse_ssh_operation_generate (SeahorseSSHSource *src, const gchar *email, 
                                 guint type, guint bits)
{
    SeahorseSSHOperationPrivate *pv;
    SeahorseOperation *op;
    gchar *filename, *comment;
    const gchar *algo;
    gchar *cmd;
    
    filename = find_nonexistant_ssh_filename (src, type);
    g_assert (filename);
    
    comment = escape_shell_arg (email);
    
    algo = get_algorithm_text (type);
    g_return_val_if_fail (algo != NULL, NULL);
    
    /* Default number of bits */
    if (bits == 0)
        bits = 2048;
    
    cmd = g_strdup_printf (SSH_KEYGEN_PATH " -b %d -t %s -C %s -f '%s'",
                           bits, algo, comment, filename);
    g_free (comment);
    
    op = seahorse_ssh_operation_new (SEAHORSE_SSH_SOURCE (src), cmd, NULL, -1, NULL);
    g_free (cmd);

    pv = SEAHORSE_SSH_OPERATION_GET_PRIVATE (op);
    pv->result_cb = generate_result_cb;
    g_object_set_data_full (G_OBJECT (op), "filename", filename, g_free);
    
    return op;
}
