/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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
#include <signal.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "seahorse-ssh-operation.h"
#include "seahorse-util.h"
#include "seahorse-passphrase.h"

#define DEBUG_FLAG SEAHORSE_DEBUG_OPERATION
#include "seahorse-debug.h"

#define COMMAND_PASSWORD "PASSWORD "
#define COMMAND_PASSWORD_LEN   9

typedef struct {
	SeahorseObject *key;
	GtkDialog *dialog;
	guint requests;
} SeahorseSshSourcePrompt;

typedef const gchar * (*SeahorseSshSourcePasswordCallback) (SeahorseSshSourcePrompt *prompt,
                                                            const gchar* message,
                                                            gpointer user_data);

typedef struct {
	GError *previous_error;

	/* Data written to SSH */
	GString *sin;
	guint win;
	GIOChannel *iin;

	/* Data being read from SSH */
	GString *sout;
	guint wout;
	GIOChannel *iout;

	/* Data from SSH error */
	GString *serr;
	guint werr;
	GIOChannel *ierr;

	/* Process Information */
	GPid pid;
	guint wpid;

	/* Callback for password prompting */
	SeahorseSshSourcePasswordCallback password_cb;
	SeahorseSshSourcePrompt *prompt;

	/* seahorse-ssh-askpass communication */
	GIOChannel *io_askpass;
	guint stag_askpass;
	int fds_askpass[2];

	GCancellable *cancellable;
	gulong cancelled_sig;
} ssh_operation_closure;

static void
ssh_operation_free (gpointer data)
{
	ssh_operation_closure *closure = data;

	/* Should have been used */
	g_assert (closure->previous_error == NULL);

	/* Disconnected when process exits */
	g_assert (closure->cancelled_sig == 0);
	g_clear_object (&closure->cancellable);

	g_assert (closure->prompt);
	if (closure->prompt->dialog)
		gtk_widget_destroy (GTK_WIDGET (closure->prompt->dialog));
	g_clear_object (&closure->prompt->key);
	g_free (closure->prompt);

	if (closure->win)
		g_source_remove (closure->win);
	if (closure->wout)
		g_source_remove (closure->wout);
	if (closure->werr)
		g_source_remove (closure->werr);

	if (closure->iin)
		g_io_channel_unref (closure->iin);
	if (closure->iout)
		g_io_channel_unref (closure->iout);
	if (closure->ierr)
		g_io_channel_unref (closure->ierr);

	if (closure->sin)
		g_string_free (closure->sin, TRUE);
	if (closure->sout)
		g_string_free (closure->sout, TRUE);
	g_string_free (closure->serr, TRUE);

	/* Close the sockets */
	if (closure->fds_askpass[0] != -1)
		close (closure->fds_askpass[0]);
	if (closure->fds_askpass[1] != -1)
		close (closure->fds_askpass[1]);

	/* watch_ssh_process always needs to have been called */
	g_assert (closure->pid == 0 && closure->wpid == 0);
	g_assert (closure->io_askpass == NULL && closure->stag_askpass == 0);

	g_free (closure);
}

static const gchar*
get_algorithm_text (guint algo)
{
    switch (algo) {
    case SSH_ALGO_DSA:
        return "dsa";
    case SSH_ALGO_RSA:
        return "rsa";
    default:
        g_return_val_if_reached (NULL);
        break;
    }
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

static const gchar*
seahorse_ssh_source_prompt_passphrase (SeahorseSshSourcePrompt *prompt,
                                       const gchar* title,
                                       const gchar* message,
                                       const gchar* check,
                                       gboolean confirm)
{
	const gchar *display;
	gchar *msg;

	if (prompt->dialog)
		gtk_widget_destroy (GTK_WIDGET (prompt->dialog));

	if (prompt->key)
		display = seahorse_object_get_label (prompt->key);
	else
		display = g_strdup (_("Secure Shell key"));
	msg = g_strdup_printf (message, display);

	prompt->dialog = seahorse_passphrase_prompt_show (title, msg, _("Password:"),
	                                                  check, confirm);
	g_free (msg);

	/* Run and check if cancelled? */
	if (gtk_dialog_run (prompt->dialog) != GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy (GTK_WIDGET (prompt->dialog));
		prompt->dialog = NULL;
		return NULL;
	}

	gtk_widget_hide (GTK_WIDGET (prompt->dialog));
	return seahorse_passphrase_prompt_get (prompt->dialog);
}

static void
on_ssh_operation_cancelled (GCancellable *cancellable,
                            gpointer user_data)
{
	GPid *pid = user_data;
	kill (*pid, SIGTERM);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

static void
on_watch_ssh_process (GPid pid,
                      gint status,
                      gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	ssh_operation_closure *closure = g_simple_async_result_get_op_res_gpointer (res);

	seahorse_debug ("SSHOP: SSH process done");

	/* Close off the askpass io channel etc... */
	if (closure->stag_askpass)
		g_source_remove (closure->stag_askpass);
	closure->stag_askpass = 0;

	/* Already have an error? */
	if (closure->previous_error) {
		g_simple_async_result_take_error (res, closure->previous_error);
		closure->previous_error = NULL;

	/* Was cancelled */
	} else if (WIFSIGNALED (status) && WTERMSIG (status) == SIGTERM) {
		g_simple_async_result_set_error (res, G_IO_ERROR, G_IO_ERROR_CANCELLED,
		                                 _("The operation was cancelled"));

	/* Failed abnormally */
	} else if (!WIFEXITED (status)) {
		g_simple_async_result_set_error (res, SEAHORSE_ERROR, 0, "%s",
		                                 _("The SSH command was terminated unexpectedly."));

	/* Command failed */
	} else if (WEXITSTATUS (status) != 0) {
		g_warning ("SSH command failed: (%d)", WEXITSTATUS (status));
		if (closure->serr->len)
			g_warning ("SSH error output: %s", closure->serr->str);
		g_simple_async_result_set_error (res, SEAHORSE_ERROR, 0, "%s",
		                                 closure->serr->len ? closure->serr->str : _("The SSH command failed."));

	}

	g_cancellable_disconnect (closure->cancellable,
	                          closure->cancelled_sig);
	closure->cancelled_sig = 0;

	g_spawn_close_pid (pid);
	closure->pid = 0;
	closure->wpid = 0;

	if (closure->io_askpass)
		g_io_channel_unref (closure->io_askpass);
	closure->io_askpass = NULL;

	g_simple_async_result_complete (res);
}

static gboolean
on_io_ssh_read (GIOChannel *source,
                GIOCondition condition,
                gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	ssh_operation_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError **error = NULL;
	gchar buf[128];
	GIOStatus status;
	gsize read = 0;
	GString *str;
	gboolean ret = TRUE;

	/* Figure out which buffer we're writing into */
	if (source == closure->iout) {
		str = closure->sout;
		seahorse_debug ("SSHOP: SSH output: ");
	} else if(source == closure->ierr) {
		str = closure->serr;
		seahorse_debug ("SSHOP: SSH errout: ");
	} else
		g_assert_not_reached ();

	if (!closure->previous_error)
		error = &closure->previous_error;

	do {
		status = g_io_channel_read_chars (source, buf, sizeof (buf),
		                                  &read, error);
		switch (status) {
		case G_IO_STATUS_ERROR:
			ret = FALSE;
			kill (closure->pid, SIGTERM);
			break;
		case G_IO_STATUS_AGAIN:
			continue;
		case G_IO_STATUS_EOF:
			break;
		default:
			g_string_append_len (str, buf, read);
			seahorse_debug ("%s", str->str + (str->len - read));
			break;
		}
	} while (read == sizeof (buf));

	return ret;
}

static gboolean
on_io_ssh_write (GIOChannel *source,
                 GIOCondition condition,
                 gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	ssh_operation_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError **error = NULL;
	GIOStatus status;
	gsize written = 0;
	gboolean ret = TRUE;

	if (closure->sin) {
		seahorse_debug ("SSHOP: SSH ready for input");

		if (!closure->previous_error)
			error = &closure->previous_error;

		status = g_io_channel_write_chars (closure->iin, closure->sin->str,
		                                   closure->sin->len, &written, error);
		switch (status) {
		case G_IO_STATUS_ERROR:
			ret = FALSE;
			kill (closure->pid, SIGTERM);
			break;
		case G_IO_STATUS_AGAIN:
			break;
		default:
			seahorse_debug ("SSHOP: Wrote %d bytes to SSH", (gint)written);
			g_string_erase (closure->sin, 0, written);
			break;
		}
	}

	if (closure->sin && !closure->sin->len) {
		seahorse_debug ("SSHOP: Finished writing SSH input");
		g_string_free (closure->sin, TRUE);
		closure->sin = NULL;
	}

	if (!closure->sin) {
		seahorse_debug ("SSHOP: Closing SSH input channel");
		g_io_channel_unref (closure->iin);
		closure->iin = NULL;
		g_source_remove (closure->win);
		closure->win = 0;
		return FALSE;
	}

	return ret;
}

/* Communication with seahorse-ssh-askpass */
static gboolean
on_io_askpass_handler (GIOChannel *source,
                       GIOCondition condition,
                       gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	ssh_operation_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	gchar *string = NULL;
	gsize length;
	GError *error = NULL;
	gboolean ret = TRUE;
	const gchar *line;
	const gchar *result = NULL;

	if (condition & G_IO_IN) {

		/* Read 1 line from the io channel, including newline character */
		g_io_channel_read_line (source, &string, &length, NULL, &error);

		if (error != NULL) {
			g_critical ("couldn't read from seahorse-ssh-askpass: %s",
			            error->message);
			g_clear_error (&error);
			ret = FALSE;
		}

		/* Process the line */
		if (string && ret) {

			string[length] = 0;
			seahorse_debug ("SSHOP: seahorse-ssh-askpass request: \"%s\"", string);

			if (g_ascii_strncasecmp (COMMAND_PASSWORD, string, COMMAND_PASSWORD_LEN) == 0) {
				line = g_strstrip (string + COMMAND_PASSWORD_LEN);

				/* Prompt for a password */
				if (closure->password_cb) {
					result = (closure->password_cb) (closure->prompt, line,
					                                 g_async_result_get_user_data (G_ASYNC_RESULT (res)));

					/* Cancelled prompt, cancel operation */
					if (!result) {
						kill (closure->pid, SIGTERM);
						seahorse_debug ("SSHOP: password prompt cancelled");
						ret = FALSE;
					}
				}

				closure->prompt->requests++;
			}

			if (ret) {
				/* And write the result back out to seahorse-ssh-askpass */
				seahorse_debug ("SSHOP: seahorse-ssh-askpass response: %s", result ? result : "");
				if (result)
					g_io_channel_write_chars (closure->io_askpass, result,
					                          strlen (result), &length, &error);
				if (error == NULL)
					g_io_channel_write_chars (closure->io_askpass, "\n", 1, &length, &error);
				if (error == NULL)
					g_io_channel_flush (closure->io_askpass, &error);
				if (error != NULL) {
					g_critical ("couldn't read from seahorse-ssh-askpass: %s",
					            error->message);
					g_clear_error (&error);
					ret = FALSE;
				}
			}
		}
	}

	if (condition & G_IO_HUP)
		ret = FALSE;

	if (!ret) {
		if (closure->io_askpass)
			g_io_channel_unref (closure->io_askpass);
		closure->io_askpass = NULL;
		closure->stag_askpass = 0;
	}

	g_free (string);
	return ret;
}


static void
on_spawn_setup_child (gpointer user_data)
{
	ssh_operation_closure *closure = user_data;
	gchar buf[15];

	/* No terminal for this process */
	setsid ();

	g_setenv ("SSH_ASKPASS", EXECDIR "seahorse-ssh-askpass", FALSE);

	/* We do screen scraping so we need locale C */
	if (g_getenv ("LC_ALL"))
		g_setenv ("LC_ALL", "C", TRUE);
	g_setenv ("LANG", "C", TRUE);

	/* Let child know which fd it is */
	if (closure->fds_askpass[1] != -1) {
		snprintf (buf, sizeof (buf), "%d", closure->fds_askpass[1]);
		g_setenv ("SEAHORSE_SSH_ASKPASS_FD", buf, TRUE);
	}

	/* Child doesn't need this stuff */
	if (closure->fds_askpass[0] != -1)
		close (closure->fds_askpass[0]);
}

static void
seahorse_ssh_operation_async (SeahorseSSHSource *source,
                              const gchar *command,
                              const gchar *input,
                              gssize length,
                              SeahorseSSHKey *key,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              SeahorseSshSourcePasswordCallback password,
                              gpointer user_data)
{
	GSimpleAsyncResult *res;
	ssh_operation_closure *closure;
	GError *error = NULL;
	int argc, r;
	int fin, fout, ferr;
	char **argv;

	g_return_if_fail (SEAHORSE_IS_SSH_SOURCE (source));
	g_return_if_fail (command && command[0]);

	if (!g_shell_parse_argv (command, &argc, &argv, NULL)) {
		g_critical ("couldn't parse ssh command line: %s", command);
		g_return_if_reached ();
	}

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_ssh_operation_async);
	closure = g_new0 (ssh_operation_closure, 1);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	closure->sout = g_string_new (NULL);
	closure->serr = g_string_new (NULL);
	closure->password_cb = password;
	closure->prompt = g_new0 (SeahorseSshSourcePrompt, 1);
	closure->prompt->key = key ? g_object_ref (key) : NULL;

	/* The seahorse-ssh-askpass pipes */
	if (socketpair (AF_UNIX, SOCK_STREAM, 0, closure->fds_askpass) == -1) {
		g_warning ("couldn't create pipes to communicate with seahorse-ssh-askpass: %s",
		           strerror(errno));
		closure->fds_askpass[0] = -1;
		closure->fds_askpass[1] = -1;
	}

	g_simple_async_result_set_op_res_gpointer (res, closure, ssh_operation_free);

	seahorse_debug ("SSHOP: Executing SSH command: %s", command);

	/* And off we go to run the program */
	r = g_spawn_async_with_pipes (NULL, argv, NULL,
	                              G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_LEAVE_DESCRIPTORS_OPEN,
	                              on_spawn_setup_child, closure, &closure->pid,
	                              input ? &fin : NULL, &fout, &ferr, &error);
	g_strfreev (argv);

	if (!r) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);
		g_object_unref (res);
		return;
	}

	/* Copy the input for later writing */
	if (input) {
		closure->sin = g_string_new_len (input, length == -1 ? strlen (input) : length);
		seahorse_debug ("SSHOP: Will send SSH input: %s", closure->sin->str);

		fcntl (fin, F_SETFL, O_NONBLOCK | fcntl (fin, F_GETFL));
		closure->iin = g_io_channel_unix_new (fin);
		g_io_channel_set_encoding (closure->iin, NULL, NULL);
		g_io_channel_set_close_on_unref (closure->iin, TRUE);
		closure->win = g_io_add_watch_full (closure->iin, G_PRIORITY_DEFAULT,
		                                    G_IO_OUT, on_io_ssh_write,
		                                    g_object_ref (res), g_object_unref);
	}

	/* Make all the proper IO Channels for the output/error */
	fcntl (fout, F_SETFL, O_NONBLOCK | fcntl (fout, F_GETFL));
	closure->iout = g_io_channel_unix_new (fout);
	g_io_channel_set_encoding (closure->iout, NULL, NULL);
	g_io_channel_set_close_on_unref (closure->iout, TRUE);
	closure->wout = g_io_add_watch_full (closure->iout, G_PRIORITY_DEFAULT,
	                                     G_IO_IN, on_io_ssh_read,
	                                     g_object_ref (res), g_object_unref);

	fcntl (ferr, F_SETFL, O_NONBLOCK | fcntl (ferr, F_GETFL));
	closure->ierr = g_io_channel_unix_new (ferr);
	g_io_channel_set_encoding (closure->ierr, NULL, NULL);
	g_io_channel_set_close_on_unref (closure->ierr, TRUE);
	closure->werr = g_io_add_watch_full (closure->ierr, G_PRIORITY_DEFAULT,
	                                     G_IO_IN, on_io_ssh_read,
	                                     g_object_ref (res), g_object_unref);

	/* Process watch */
	closure->wpid = g_child_watch_add_full (G_PRIORITY_DEFAULT, closure->pid,
	                                        on_watch_ssh_process,
	                                        g_object_ref (res), g_object_unref);

	/* Setup askpass communication */
	if (closure->fds_askpass[0] != -1) {
		closure->io_askpass = g_io_channel_unix_new (closure->fds_askpass[0]);
		g_io_channel_set_close_on_unref (closure->io_askpass, TRUE);
		g_io_channel_set_encoding (closure->io_askpass, NULL, NULL);
		closure->stag_askpass = g_io_add_watch_full (closure->io_askpass, G_PRIORITY_DEFAULT,
		                                             G_IO_IN | G_IO_HUP, on_io_askpass_handler,
		                                             g_object_ref (res), g_object_unref);
		closure->fds_askpass[0] = -1; /* closed by io channel */
	}

	/* The other end of the pipe, close it */
	if (closure->fds_askpass[1] != -1)
		close (closure->fds_askpass[1]);
	closure->fds_askpass[1] = -1;

	if (cancellable)
		closure->cancelled_sig = g_cancellable_connect (closure->cancellable,
		                                                G_CALLBACK (on_ssh_operation_cancelled),
		                                                &closure->pid, NULL);

	g_object_unref (res);
}

static GString *
seahorse_ssh_operation_finish (SeahorseSSHSource *source,
                               GAsyncResult *result,
                               GError **error)
{
	GString *output;
	ssh_operation_closure *closure;

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_ssh_operation_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	closure = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
	output = closure->sout;
	closure->sout = NULL;
	return output;
}

/* -----------------------------------------------------------------------------
 * UPLOAD KEY 
 */

typedef struct {
	GMemoryOutputStream *output;
	GCancellable *cancellable;
	gchar *username;
	gchar *hostname;
	gchar *port;
} ssh_upload_closure;

static void
ssh_upload_free (gpointer data)
{
	ssh_upload_closure *closure = data;
	g_object_unref (closure->output);
	g_clear_object (&closure->cancellable);
	g_free (closure->username);
	g_free (closure->hostname);
	g_free (closure->port);
	g_free (closure);
}

static const gchar*
on_upload_send_password (SeahorseSshSourcePrompt *prompt,
                         const gchar* message,
                         gpointer user_data)
{
    seahorse_debug ("in upload_password_cb");

    /* Just prompt over and over again */
    return seahorse_ssh_source_prompt_passphrase (prompt, _("Remote Host Password"),
                                                  message, NULL, FALSE);
}

static void
on_upload_send_complete (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	GError *error = NULL;

	if (!seahorse_ssh_operation_finish (SEAHORSE_SSH_SOURCE (source), result, &error))
		g_simple_async_result_take_error (res, error);

	g_simple_async_result_complete (res);
	g_object_unref (res);
}

static void
on_upload_export_complete (GObject *source,
                           GAsyncResult *result,
                           gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	ssh_upload_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;
	gchar *data;
	size_t length;
	gchar *cmd;

	if (!seahorse_source_export_finish (SEAHORSE_SOURCE (source), result, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);
		g_object_unref (res);
		return;
	}

	/*
	 * This command creates the .ssh directory if necessary (with appropriate permissions)
	 * and then appends all input data onto the end of .ssh/authorized_keys
	 */
	/* TODO: Important, we should handle the host checking properly */
	cmd = g_strdup_printf (SSH_PATH " '%s@%s' %s %s -o StrictHostKeyChecking=no "
	                       "\"umask 077; test -d .ssh || mkdir .ssh ; cat >> .ssh/authorized_keys\"",
	                       closure->username, closure->hostname,
	                       closure->port ? "-p" : "", closure->port ? closure->port : "");

	data = g_memory_output_stream_get_data (closure->output);
	length = g_memory_output_stream_get_data_size (closure->output);

	seahorse_ssh_operation_async (SEAHORSE_SSH_SOURCE (source), cmd, data, length, NULL,
	                              closure->cancellable, on_upload_send_complete,
	                              on_upload_send_password, g_object_ref (res));

	g_free (cmd);
	g_object_unref (res);
}

void
seahorse_ssh_op_upload_async (SeahorseSSHSource *source,
                              GList *keys,
                              const gchar *username,
                              const gchar *hostname,
                              const gchar *port,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
	GSimpleAsyncResult *res;
	ssh_upload_closure *closure;

	g_return_if_fail (keys != NULL);
	g_return_if_fail (username && username[0]);
	g_return_if_fail (hostname && hostname[0]);

	if (port && !port[0])
		port = NULL;

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_ssh_op_upload_async);
	closure = g_new0 (ssh_upload_closure, 1);
	closure->output = G_MEMORY_OUTPUT_STREAM (g_memory_output_stream_new (NULL, 0, g_realloc, NULL));
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	closure->username = g_strdup (username);
	closure->hostname = g_strdup (hostname);
	closure->port = g_strdup (port);
	g_simple_async_result_set_op_res_gpointer (res, closure, ssh_upload_free);

	/* Buffer for what we send to the server */
	seahorse_source_export_async (SEAHORSE_SOURCE (source), keys, G_OUTPUT_STREAM (closure->output),
	                              cancellable, on_upload_export_complete, g_object_ref (res));

	g_object_unref (res);

}

gboolean
seahorse_ssh_op_upload_finish (SeahorseSSHSource *source,
                               GAsyncResult *result,
                               GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_ssh_op_change_passphrase_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

/* -----------------------------------------------------------------------------
 * CHANGE PASSPHRASE 
 */

static const gchar *
on_change_passphrase_password (SeahorseSshSourcePrompt *prompt,
                               const gchar* message,
                               gpointer user_data)
{
	const gchar *ret = NULL;
	gchar *lcase;

	lcase = g_strdup (message ? message : "");
	seahorse_util_string_lower (lcase);

	seahorse_debug ("in change_password_cb");

	/* Need the old passphrase */
	if (strstr (lcase, "old pass"))
		ret = seahorse_ssh_source_prompt_passphrase (prompt, _("Old Key Passphrase"),
		                                             _("Enter the old passphrase for: %s"), NULL, FALSE);

	/* Look for the new passphrase thingy */
	else if (strstr (lcase, "new pass"))
		ret = seahorse_ssh_source_prompt_passphrase (prompt, _("New Key Passphrase"),
		                                             _("Enter the new passphrase for: %s"), NULL, TRUE);

	/* Confirm the new passphrase, just send it again */
	else if (strstr (lcase, "again") && prompt->dialog)
		ret = seahorse_passphrase_prompt_get (prompt->dialog);

	/* Something we don't understand */
	else
		ret = seahorse_ssh_source_prompt_passphrase (prompt, _("Enter Key Passphrase"),
		                                             message, NULL, FALSE);

	g_free (lcase);
	return ret;
}

static void
on_change_passphrase_complete (GObject *source,
                               GAsyncResult *result,
                               gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	SeahorseSSHKey *key = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	if (seahorse_ssh_operation_finish (SEAHORSE_SSH_SOURCE (source), result, &error))
		seahorse_object_refresh (SEAHORSE_OBJECT (key));
	else
		g_simple_async_result_take_error (res, error);

	g_simple_async_result_complete (res);
	g_object_unref (res);
}

void
seahorse_ssh_op_change_passphrase_async  (SeahorseSSHKey *key,
                                          GCancellable *cancellable,
                                          GAsyncReadyCallback callback,
                                          gpointer user_data)
{
	GSimpleAsyncResult *res;
	SeahorseSource *source;
	gchar *cmd;

	g_return_if_fail (SEAHORSE_IS_SSH_KEY (key));
	g_return_if_fail (key->keydata && key->keydata->privfile);

	source = seahorse_object_get_source (SEAHORSE_OBJECT (key));
	g_return_if_fail (SEAHORSE_IS_SSH_SOURCE (source));

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_ssh_op_change_passphrase_async);
	g_simple_async_result_set_op_res_gpointer (res, g_object_ref (key), g_object_unref);

	cmd = g_strdup_printf (SSH_KEYGEN_PATH " -p -f '%s'", key->keydata->privfile);
	seahorse_ssh_operation_async (SEAHORSE_SSH_SOURCE (source), cmd, NULL, 0, key, cancellable,
	                              on_change_passphrase_complete, on_change_passphrase_password,
	                              g_object_ref (res));

	g_free (cmd);
	g_object_unref (res);
}

gboolean
seahorse_ssh_op_change_passphrase_finish (SeahorseSSHKey *key,
                                          GAsyncResult *result,
                                          GError **error)
{
	SeahorseSource *source;

	source = seahorse_object_get_source (SEAHORSE_OBJECT (key));
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_ssh_op_change_passphrase_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

/* -----------------------------------------------------------------------------
 * KEY GENERATE OPERATION
 */

typedef struct {
	gchar *filename;
	SeahorseSSHKey *key;
} ssh_generate_closure;

static void
ssh_generate_free (gpointer data)
{
	ssh_generate_closure *closure = data;
	g_free (closure->filename);
	g_clear_object (&closure->key);
	g_free (closure);
}

static void
on_generate_complete (GObject *source,
                      GAsyncResult *result,
                      gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	ssh_generate_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	if (!seahorse_ssh_operation_finish (SEAHORSE_SSH_SOURCE (source), result, &error)) {
		g_simple_async_result_take_error (res, error);

	} else {
		/* The result of the operation is the key we generated */
		closure->key = seahorse_ssh_source_key_for_filename (SEAHORSE_SSH_SOURCE (source),
		                                                     closure->filename);
		g_return_if_fail (SEAHORSE_IS_SSH_KEY (closure->key));
		g_object_ref (closure->key);
	}

	g_simple_async_result_complete (res);
	g_object_unref (res);
}

static const gchar *
on_generate_password (SeahorseSshSourcePrompt *prompt,
                      const gchar* message,
                      gpointer user_data)
{
	seahorse_debug ("in generate_password_cb");

	/* If the first time then prompt */
	if (!prompt->dialog)
		return seahorse_ssh_source_prompt_passphrase (prompt,
		                                              _("Passphrase for New Secure Shell Key"),
		                                              _("Enter a passphrase for your new Secure Shell key."),
		                                              NULL, TRUE);

	/* Otherwise return the entered passphrase */
	return seahorse_passphrase_prompt_get (prompt->dialog);
}

void
seahorse_ssh_op_generate_async (SeahorseSSHSource *source,
                                const gchar *email,
                                guint type,
                                guint bits,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
	ssh_generate_closure *closure;
	GSimpleAsyncResult *res;
	const gchar *algo;
	gchar *comment;
	gchar *cmd;

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_ssh_op_generate_async);
	closure = g_new0 (ssh_generate_closure, 1);
	closure->filename = seahorse_ssh_source_file_for_algorithm (source, type);
	g_simple_async_result_set_op_res_gpointer (res, closure, ssh_generate_free);

	comment = escape_shell_arg (email);
	algo = get_algorithm_text (type);
	g_return_if_fail (algo != NULL);

	/* Default number of bits */
	if (bits == 0)
		bits = 2048;

	cmd = g_strdup_printf (SSH_KEYGEN_PATH " -b '%d' -t '%s' -C %s -f '%s'",
	                       bits, algo, comment, closure->filename);
	g_free (comment);

	seahorse_ssh_operation_async (source, cmd, NULL, 0, NULL, cancellable,
	                              on_generate_complete, on_generate_password,
	                              g_object_ref (res));

	g_free (cmd);
	g_object_unref (res);
}

SeahorseObject *
seahorse_ssh_op_generate_finish (SeahorseSSHSource *source,
                                 GAsyncResult *result,
                                 GError **error)
{
	ssh_generate_closure *closure;

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_ssh_op_generate_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	closure = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
	return SEAHORSE_OBJECT (closure->key);
}

/* -----------------------------------------------------------------------------
 * IMPORT A PUBLIC KEY 
 */

void
seahorse_ssh_op_import_public_async (SeahorseSSHSource *source,
                                     SeahorseSSHKeyData *data,
                                     const gchar* filename,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
	GError *error = NULL;
	GSimpleAsyncResult *res;

	g_return_if_fail (seahorse_ssh_key_data_is_valid (data));
	g_return_if_fail (data->rawdata);
	g_return_if_fail (SEAHORSE_IS_SSH_SOURCE (source));

	seahorse_ssh_key_data_filter_file (filename, data, NULL, &error);

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_ssh_op_import_public_async);
	g_simple_async_result_set_op_res_gpointer (res, g_strdup (data->fingerprint), g_free);
	g_simple_async_result_complete_in_idle (res);
	g_object_unref (res);
}

gchar *
seahorse_ssh_op_import_public_finish (SeahorseSSHSource *source,
                                      GAsyncResult *result,
                                      GError **error)
{
	const gchar *fingerprint;

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_ssh_op_authorize_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	fingerprint = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
	return g_strdup (fingerprint);
}

/* -----------------------------------------------------------------------------
 * IMPORT A PRIVATE KEY 
 */

typedef struct {
	gchar *pubfile;
	gchar *comment;
	gchar *fingerprint;
} ssh_import_closure;

static void
ssh_import_free (gpointer data)
{
	ssh_import_closure *closure = data;
	g_free (closure->pubfile);
	g_free (closure->comment);
	g_free (closure->fingerprint);
	g_free (closure);
}

static const gchar *
on_import_private_password (SeahorseSshSourcePrompt *prompt,
                            const gchar* message,
                            gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	ssh_import_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	const gchar *ret;
	gchar* details;

	/* Add the comment to the output */
	if (closure->comment)
		details = g_strdup_printf (_("Importing key: %s"), closure->comment);
	else
		details = g_strdup (_("Importing key. Enter passphrase"));

	ret = seahorse_ssh_source_prompt_passphrase (prompt, _("Import Key"),
	                                             details, NULL, FALSE);
	g_free (details);

	return ret;
}

static void
on_import_private_complete (GObject *source,
                            GAsyncResult *result,
                            gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	ssh_import_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	SeahorseSSHKeyData *keydata;
	GError *error = NULL;
	GString *output;
	gsize pos;

	output = seahorse_ssh_operation_finish (SEAHORSE_SSH_SOURCE (source), result, &error);
	if (error == NULL) {
		/* Only use the first line of the output */
		pos = strcspn (output->str, "\n\r");
		if (pos < output->len)
			g_string_erase (output, pos, -1);

		/* Parse the data so we can get the fingerprint */
		keydata = seahorse_ssh_key_data_parse_line (output->str, -1);
		if (seahorse_ssh_key_data_is_valid (keydata))
			closure->fingerprint = g_strdup (keydata->fingerprint);
		else
			g_warning ("couldn't parse imported private key fingerprint");
		seahorse_ssh_key_data_free (keydata);

		/* Add the comment to the output */
		if (closure->comment) {
			g_string_append_c (output, ' ');
			g_string_append (output, closure->comment);
		}

		/* The file to write to */
		seahorse_util_write_file_private (closure->pubfile, output->str, &error);
	}

	if (error != NULL)
		g_simple_async_result_take_error (res, error);
	g_simple_async_result_complete (res);
	g_object_unref (res);
}

void
seahorse_ssh_op_import_private_async (SeahorseSSHSource *source,
                                      SeahorseSSHSecData *data,
                                      const gchar *filename,
                                      GCancellable *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
	GSimpleAsyncResult *res;
	gchar *cmd, *privfile = NULL;
	GError *error = NULL;
	ssh_import_closure *closure;

	g_return_if_fail (data && data->rawdata);
	g_return_if_fail (SEAHORSE_IS_SSH_SOURCE (source));

	/* No filename specified, make one up */
	if (!filename) {
		filename = privfile = seahorse_ssh_source_file_for_algorithm (source, data->algo);
		g_return_if_fail (privfile);
	}

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_ssh_op_import_private_async);
	closure = g_new0 (ssh_import_closure, 1);
	closure->pubfile = g_strdup_printf ("%s.pub", filename);
	closure->comment = g_strdup (data->comment);
	g_simple_async_result_set_op_res_gpointer (res, closure, ssh_import_free);

	/* Write the private key into the file */
	if (!seahorse_util_write_file_private (filename, data->rawdata, &error)) {
		g_free (privfile);
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);
		g_object_unref (res);
		return;
	}

	/* Start command to generate public key */
	cmd = g_strdup_printf (SSH_KEYGEN_PATH " -y -f '%s'", privfile);
	seahorse_ssh_operation_async (source, cmd, NULL, 0, NULL, cancellable,
	                              on_import_private_complete,
	                              on_import_private_password,
	                              g_object_ref (res));
	g_free (cmd);

	g_object_unref (res);
	g_free (privfile);

}

gchar *
seahorse_ssh_op_import_private_finish (SeahorseSSHSource *source,
                                       GAsyncResult *result,
                                       GError **error)
{
	ssh_import_closure *closure;
	gchar *fingerprint;

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_ssh_op_import_private_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	closure = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
	fingerprint = closure->fingerprint;
	closure->fingerprint = NULL;
	return fingerprint;

}

/* -----------------------------------------------------------------------------
 * AUTHORIZE A PUBLIC KEY 
 */ 

void
seahorse_ssh_op_authorize_async (SeahorseSSHSource *source,
                                 SeahorseSSHKey *key,
                                 gboolean authorize,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
	SeahorseSSHKeyData *keydata = NULL;
	GError *error = NULL;
	gchar* from = NULL;
	gchar* to = NULL;
	GSimpleAsyncResult *res;

	g_return_if_fail (SEAHORSE_IS_SSH_SOURCE (source));
	g_return_if_fail (SEAHORSE_IS_SSH_KEY (key));

	g_object_get (key, "key-data", &keydata, NULL);
	g_return_if_fail (keydata);

	if (authorize) {
		to = seahorse_ssh_source_file_for_public (source, TRUE);
	} else {
		from = seahorse_ssh_source_file_for_public (source, TRUE);
		to = seahorse_ssh_source_file_for_public (source, FALSE);
	}

	/* Take it out of the from file, and put into the to file */
	if (!from || seahorse_ssh_key_data_filter_file (from, NULL, keydata, &error))
		seahorse_ssh_key_data_filter_file (to, keydata, NULL, &error);

	g_free (from);
	g_free (to);

	/* Just reload that one key */
	if (!error)
		seahorse_object_refresh (SEAHORSE_OBJECT (key));

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_ssh_op_authorize_async);
	if (error != NULL)
		g_simple_async_result_take_error (res, error);
	g_simple_async_result_complete_in_idle (res);
	g_object_unref (res);
}

gboolean
seahorse_ssh_op_authorize_finish (SeahorseSSHSource *source,
                                  GAsyncResult *result,
                                  GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_ssh_op_authorize_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

/* -----------------------------------------------------------------------------
 * RENAME COMMENT ON A KEY 
 */

static gboolean
change_raw_comment (SeahorseSSHKeyData *keydata, const gchar *newcomment)
{
    const gchar *x = keydata->rawdata;
    gchar *result;
    gchar **parts;
    
    g_assert (x);
    while (*x && g_ascii_isspace (*x))
        ++x;
    
    parts = g_strsplit_set (x, " ", 3);
    if (!parts[0] || !parts[1])
        return FALSE;
    
    result = g_strconcat (parts[0], " ", parts[1], " ", newcomment, NULL);
    g_strfreev(parts);
    
    g_free (keydata->rawdata);
    keydata->rawdata = result;
    return TRUE;
}

void
seahorse_ssh_op_rename_async (SeahorseSSHSource *source,
                              SeahorseSSHKey *key,
                              const gchar *newcomment,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
	SeahorseSSHKeyData *keydata;
	GSimpleAsyncResult *res;
	GError *error = NULL;

	g_return_if_fail (SEAHORSE_IS_SSH_SOURCE (source));
	g_return_if_fail (SEAHORSE_IS_SSH_KEY (key));

	keydata = seahorse_ssh_key_data_dup (key->keydata);

	if (!newcomment)
		newcomment = "";

	if (!change_raw_comment (keydata, newcomment ? newcomment : ""))
		g_return_if_reached ();

	seahorse_debug ("renaming key to: %s", newcomment);

	/* Just part of a file for this key */
	if (keydata->partial) {
		g_assert (keydata->pubfile);
		seahorse_ssh_key_data_filter_file (keydata->pubfile, keydata, keydata, &error);

		/* A full file for this key */
	} else {
		g_assert (keydata->pubfile);
		seahorse_util_write_file_private (keydata->pubfile, keydata->rawdata, &error);
	}

	seahorse_ssh_key_data_free (keydata);

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_ssh_op_rename_async);
	if (error != NULL)
		g_simple_async_result_take_error (res, error);
	g_simple_async_result_complete_in_idle (res);
	g_object_unref (res);
}

gboolean
seahorse_ssh_op_rename_finish (SeahorseSSHSource *source,
                               GAsyncResult *result,
                               GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_ssh_op_rename_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

gboolean
seahorse_ssh_op_delete_sync (SeahorseSSHKey *key,
                             GError **error)
{
	SeahorseSSHKeyData *keydata = NULL;
	gboolean ret = TRUE;

	g_return_val_if_fail (SEAHORSE_IS_SSH_KEY (key), FALSE);

	g_object_get (key, "key-data", &keydata, NULL);
	g_return_val_if_fail (keydata, FALSE);

	/* Just part of a file for this key */
	if (keydata->partial) {
		/* Take just that line out of the file */
		if (keydata->pubfile)
			ret = seahorse_ssh_key_data_filter_file (keydata->pubfile, NULL, keydata, error);

	/* A full file for this key */
	} else {
		if (keydata->pubfile) {
			if (g_unlink (keydata->pubfile) == -1) {
				g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
				             "%s", g_strerror (errno));
				ret = FALSE;
			}
		}

		if (ret && keydata->privfile) {
			if (g_unlink (keydata->privfile) == -1) {
				g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
				             "%s", g_strerror (errno));
				ret = FALSE;
			}
		}
	}

	if (ret)
		seahorse_context_remove_object (seahorse_context_instance (),
		                                SEAHORSE_OBJECT (key));

	return ret;
}

