/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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

#include <config.h>

#include <string.h>

#include <glib/gi18n.h>

#include "seahorse-icons.h"
#include "seahorse-widget.h"
#include "seahorse-util.h"
#include "seahorse-progress.h"

#include "ssh/seahorse-ssh-dialogs.h"
#include "ssh/seahorse-ssh-source.h"
#include "ssh/seahorse-ssh-key.h"
#include "ssh/seahorse-ssh-operation.h"

void              on_upload_input_changed                 (GtkWidget *dummy,
                                                           SeahorseWidget *swidget);

static void 
on_upload_complete (GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
	GError *error = NULL;

	if (!seahorse_ssh_op_upload_finish (SEAHORSE_SSH_SOURCE (source), result, &error))
		seahorse_util_handle_error (&error, NULL, _("Couldn't configure Secure Shell keys on remote computer."));
}

G_MODULE_EXPORT void
on_upload_input_changed (GtkWidget *dummy, SeahorseWidget *swidget)
{
    GtkWidget *widget;
    const gchar *user, *host, *port;
    gchar *t = NULL;

    widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "user-label"));
    user = gtk_entry_get_text (GTK_ENTRY (widget));
    g_return_if_fail (user && g_utf8_validate (user, -1, NULL));

    widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "host-label"));
    host = gtk_entry_get_text (GTK_ENTRY (widget));
    g_return_if_fail (host && g_utf8_validate (host, -1, NULL));
    
    /* Take off port if necessary */
    port = strchr (host, ':');
    if (port) {
        
        /* Copy hostname out */
        g_assert (port >= host);
        host = t = g_strndup (host, port - host);
    }

    widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "ok"));
    gtk_widget_set_sensitive (widget, host[0] && !seahorse_util_string_is_whitespace (host) && 
                                      user[0] && !seahorse_util_string_is_whitespace (user));
    
    /* Possibly allocated host */
    g_free (t);
}

static void
upload_keys (SeahorseWidget *swidget)
{
    GtkWidget *widget;
    const gchar *cuser, *chost;
    gchar *user, *host, *port;
    GList *keys;
    GCancellable *cancellable;

    keys = (GList*)g_object_steal_data (G_OBJECT (swidget), "upload-keys");
    if (keys == NULL)
        return;

    widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "user-label"));
    cuser = gtk_entry_get_text (GTK_ENTRY (widget));
    g_return_if_fail (cuser && g_utf8_validate (cuser, -1, NULL));
    
    widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "host-label"));
    chost = (gchar*)gtk_entry_get_text (GTK_ENTRY (widget));
    g_return_if_fail (chost && g_utf8_validate (chost, -1, NULL));
    
    user = g_strdup (cuser);
    host = g_strdup (chost);

    /* Port is anything past a colon */
    port = strchr (host, ':');
    if (port) {
        *port = 0;
        port++;
        
        /* Trim and check */
        seahorse_util_string_trim_whitespace (port);
        if (!port[0])
            port = NULL;
    }

    seahorse_util_string_trim_whitespace (host);
    seahorse_util_string_trim_whitespace (user);

    cancellable = g_cancellable_new ();

    /* Start an upload process */
    seahorse_ssh_op_upload_async (SEAHORSE_SSH_SOURCE (seahorse_object_get_place (keys->data)),
                                  keys, user, host, port, cancellable, on_upload_complete, NULL);

    g_free (host);
    g_free (user);

    seahorse_progress_show (cancellable, _("Configuring Secure Shell Keys..."), FALSE);
    g_object_unref (cancellable);
}

/**
 * seahorse_upload_show
 * @keys: Upload a certain set of SSH keys
 * 
 * Prompt a dialog to upload keys.
 **/
void
seahorse_ssh_upload_prompt (GList *keys, GtkWindow *parent)
{
    SeahorseWidget *swidget;
    GtkWindow *win;
    GtkWidget *w;
    
    g_return_if_fail (keys != NULL);
    
    swidget = seahorse_widget_new_allow_multiple ("ssh-upload", parent);
    g_return_if_fail (swidget != NULL);
    
    win = GTK_WINDOW (GTK_WIDGET (seahorse_widget_get_widget (swidget, swidget->name)));

    /* Default to the users current name */
    w = GTK_WIDGET (seahorse_widget_get_widget (swidget, "user-label"));
    gtk_entry_set_text (GTK_ENTRY (w), g_get_user_name ());
 
    /* Focus the host */
    w = GTK_WIDGET (seahorse_widget_get_widget (swidget, "host-label"));
    gtk_widget_grab_focus (w);    

    keys = g_list_copy (keys);
    g_object_set_data_full (G_OBJECT (swidget), "upload-keys", keys, 
                            (GDestroyNotify)g_list_free);

    on_upload_input_changed (NULL, swidget);

    for (;;) {
        switch (gtk_dialog_run (GTK_DIALOG (win))) {
        case GTK_RESPONSE_HELP:
            /* TODO: Help */
            continue;
        case GTK_RESPONSE_ACCEPT:
            upload_keys (swidget);
            break;
        default:
            break;
        };
        
        break;
    }
    
    seahorse_widget_destroy (swidget);
}
