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
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <signal.h>
#include <stdlib.h>
#include <eel/eel.h>
#include <gnome.h>

#include "seahorse-context.h"
#include "seahorse-widget.h"
#include "seahorse-gpg-options.h"

#define SETTING_CACHE       "/apps/seahorse/agent/cache_enabled"
#define SETTING_TTL         "/apps/seahorse/agent/cache_ttl"
#define SETTING_EXPIRE      "/apps/seahorse/agent/cache_expire"
#define SETTING_AUTH        "/apps/seahorse/agent/cache_authorize"

typedef enum {
    AGENT_NONE,
    AGENT_OTHER,
    AGENT_SEAHORSE
} AgentType;

/* -----------------------------------------------------------------------------
 *  SEAHORSE-AGENT CHECKS
 */

/* Check if given process is running */
static gboolean
is_pid_running (pid_t pid)
{
    /* 
     * We try to send it a harmless signal. Note that this won't
     * work when sending to another users process. But other users
     * shouldn't be running agent for this user anyway.
     */
    return (kill (pid, SIGWINCH) != -1);
}

/* Check if the server at the other end of the socket is seahorse-agent */
static AgentType
check_agent_id (int fd)
{
    AgentType ret = AGENT_NONE;
    GIOChannel *io;
    gchar *t;

    io = g_io_channel_unix_new (fd);

    /* Server always sends a response first */
    if (g_io_channel_read_line (io, &t, NULL, NULL, NULL) == G_IO_STATUS_NORMAL && t) {
        g_strstrip (t);
        if (g_str_has_prefix (t, "OK"))
            ret = AGENT_OTHER;
        g_free (t);

        /* Send back request for info */
        if (ret != AGENT_NONE &&
            g_io_channel_write_chars (io, "AGENT_ID\n", -1, NULL,
                                      NULL) == G_IO_STATUS_NORMAL
            && g_io_channel_flush (io, NULL) == G_IO_STATUS_NORMAL
            && g_io_channel_read_line (io, &t, NULL, NULL,
                                       NULL) == G_IO_STATUS_NORMAL && t) {
            g_strstrip (t);
            if (g_str_has_prefix (t, "OK seahorse-agent"))
                ret = AGENT_SEAHORSE;
            g_free (t);
        }
    }

    g_io_channel_shutdown (io, FALSE, NULL);
    g_io_channel_unref (io);
    return ret;
}

/* Open a connection to seahorse-agent */
static AgentType
get_listening_agent_type (const gchar *sockname)
{
    struct sockaddr_un addr;
    AgentType ret = AGENT_NONE;
    int len;
    int fd;

    /* Agent is always a unix socket */
    fd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (fd != -1) {
        memset (&addr, 0, sizeof (addr));
        addr.sun_family = AF_UNIX;
        g_strlcpy (addr.sun_path, sockname, sizeof (addr.sun_path));
        len = offsetof (struct sockaddr_un, sun_path) + strlen (addr.sun_path) + 1;

        if (connect (fd, (const struct sockaddr *) &addr, len) == 0)
            ret = check_agent_id (fd);
    }

    shutdown (fd, SHUT_RDWR);
    close (fd);
    return ret;
}

/* Given an agent info string make sure it's running and is seahorse-agent */
static AgentType
check_agent_info (const gchar *agent_info)
{
    AgentType ret = AGENT_NONE;
    gchar **info;
    gchar **t;
    int i;

    gchar *socket;
    pid_t pid;
    gint version;

    info = g_strsplit (agent_info, ":", 3);

    for (i = 0, t = info; *t && i < 3; t++, i++) {
        switch (i) {
            /* The socket name first */
        case 0:
            socket = *t;
            break;

            /* Then the process id */
        case 1:
            pid = (pid_t) atoi (*t);
            break;

            /* Then the protocol version */
        case 2:
            version = (gint) atoi (*t);
            break;

        default:
            g_assert_not_reached ();
        };
    }

    if (version == 1 && pid != 0 && is_pid_running (pid))
        ret = get_listening_agent_type (socket);
        
    g_strfreev (info);
    return ret;
}

/* Check if the agent is running */
static AgentType
which_agent_running ()
{
    gchar *value = NULL;
    AgentType ret;

    /* Seahorse edits gpg.conf by default */
    seahorse_gpg_options_find ("gpg-agent-info", &value, NULL);
    if (value != NULL) {
        ret = check_agent_info (value);
        g_free (value);
        return ret;
    }

    /* The user probably set this up on their own */
    value = (gchar *) g_getenv ("GPG_AGENT_INFO");
    if (value != NULL)
        return check_agent_info (value);

    return AGENT_NONE;
}

/* -----------------------------------------------------------------------------
 *  CONTROLS
 */

/* For the control callbacks */
typedef struct _CtlLinkups {
    gint notify_id;
    gchar *gconf_key;
} CtlLinkups;

/* Disconnect control from gconf */
static void
control_destroy (GtkWidget *widget, gpointer data)
{
    CtlLinkups *lu = (CtlLinkups *) data;
    g_assert (lu->gconf_key);
    g_assert (lu->notify_id);
    eel_gconf_notification_remove (lu->notify_id);

    g_free (lu->gconf_key);
    g_free (lu);
}

/* Disable a control based on this button */
static void
control_disable (GtkWidget *widget, gpointer data)
{
    gtk_widget_set_sensitive (GTK_WIDGET (data),
                              gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                                            (widget)));
}

/* Change gconf setting based on this button */
static void
check_toggled (GtkWidget *widget, gpointer data)
{
    CtlLinkups *lu = (CtlLinkups *) data;
    eel_gconf_set_boolean (lu->gconf_key,
                           gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                                         (widget)));
}

/* Change button based on gconf */
static void
check_notify (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (data),
                                  gconf_value_get_bool (gconf_entry_get_value
                                                        (entry)));
}

/* Hook a button into gconf */
static void
setup_check_control (SeahorseContext *ctx, SeahorseWidget *sw,
                     const gchar *name, const gchar * key)
{
    GtkWidget *ctl;
    CtlLinkups *lu;

    g_return_if_fail (sw != NULL);    

    ctl = glade_xml_get_widget (sw->xml, name);
    g_return_if_fail (ctl != NULL);

    /* Hookup load events */
    lu = g_new0 (CtlLinkups, 1);
    lu->gconf_key = g_strdup (key);
    lu->notify_id = eel_gconf_notification_add (key, check_notify, ctl);

    /* Hookup save events */
    g_signal_connect (ctl, "toggled", G_CALLBACK (check_toggled), lu);

    /* Cleanup */
    g_signal_connect (ctl, "destroy", G_CALLBACK (control_destroy), lu);

    /* Set initial value, and listen on events */
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ctl),
                                  eel_gconf_get_boolean (key));
}

/* Change gconf based on spinner */
static void
spinner_changed (GtkWidget *widget, gpointer data)
{
    CtlLinkups *lu = (CtlLinkups *) data;
    eel_gconf_set_integer (lu->gconf_key,
                           gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON
                                                             (widget)));
}

/* Change spinner based on gconf */
static void
spinner_notify (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (data),
                               (double)
                               gconf_value_get_int (gconf_entry_get_value (entry)));
}

/* Hook a spinner into gconf */
static void
setup_spinner_control (SeahorseContext *ctx, SeahorseWidget *sw,
                       const gchar *name, const gchar *key)
{
    GtkWidget *ctl;
    CtlLinkups *lu;
    
    g_return_if_fail (sw != NULL);    

    ctl = glade_xml_get_widget (sw->xml, name);
    g_return_if_fail (ctl != NULL);

    /* Hookup load events */
    lu = g_new0 (CtlLinkups, 1);
    lu->gconf_key = g_strdup (key);
    lu->notify_id = eel_gconf_notification_add (key, spinner_notify, ctl);

    /* Hookup save events */
    g_signal_connect (ctl, "changed", G_CALLBACK (spinner_changed), lu);

    /* Cleanup */
    g_signal_connect (ctl, "destroy", G_CALLBACK (control_destroy), lu);

    /* Set initial value, and listen on events */
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (ctl), eel_gconf_get_integer (key));
}

/* Basic GError handler */
static void
handle_error (GError *err, const gchar *desc)
{
    GtkWidget *dialog;
    gchar *msg;

    if (desc && err)
        msg = g_strdup_printf ("%s\n\n%s", desc, err->message);
    else if (desc)
        msg = g_strdup (desc);
    else
        msg = g_strdup (err->message);

    g_clear_error (&err);

    dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE, msg);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    g_free (msg);
}

/* Start up the gnome-session-properties */
static void
show_session_properties (GtkWidget *widget, gpointer data)
{
    GError *err = NULL;

    g_spawn_command_line_async ("gnome-session-properties", &err);

    if (err)
        handle_error (err, _("Couldn't open the Session Properties"));
}

/* Startup seahorse-agent */
static void
start_agent (GtkWidget *widget, gpointer data)
{
    GError *err = NULL;
    gint status;

    g_spawn_command_line_sync ("seahorse-agent", NULL, NULL, &status, &err);

    if (err)
        handle_error (err, _("Couldn't start the 'seahorse-agent' program"));
    else if (!(WIFEXITED (status) && WEXITSTATUS (status) == 0))
        handle_error (NULL, _("The 'seahorse-agent' program exited unsucessfully."));
    else {
        /* Show the next message about starting up automatically */
        gtk_widget_hide (gtk_widget_get_parent (widget));
        gtk_widget_show (GTK_WIDGET (data));
    }
}

/* Initialize the cache tab */
void
seahorse_prefs_cache (SeahorseContext *ctx, SeahorseWidget *widget)
{
    GtkWidget *w;
    
    g_return_if_fail (widget != NULL);

    w = glade_xml_get_widget (widget->xml, "use-cache");
    g_return_if_fail (w != NULL);
    g_signal_connect_after (w , "toggled", G_CALLBACK (control_disable),
                            glade_xml_get_widget (widget->xml, "cache-options"));
                            
    w = glade_xml_get_widget (widget->xml, "expire");        
    g_return_if_fail (w != NULL);
    g_signal_connect_after (w , "toggled", G_CALLBACK (control_disable),
                            glade_xml_get_widget (widget->xml, "ttl"));

    setup_spinner_control (ctx, widget, "ttl", SETTING_TTL);
    setup_check_control (ctx, widget, "use-cache", SETTING_CACHE);
    setup_check_control (ctx, widget, "expire", SETTING_EXPIRE);
    setup_check_control (ctx, widget, "authorize", SETTING_AUTH);

    switch (which_agent_running ()) {
      
    /* No agent running offer to start */
    case AGENT_NONE:
        w = glade_xml_get_widget (widget->xml, "agent-start");
        g_return_if_fail (w != NULL);
        gtk_widget_show (w);

        glade_xml_signal_connect_data (widget->xml, "on_start_link",
                                       G_CALLBACK (start_agent),
                                       glade_xml_get_widget (widget->xml, "agent-started"));
        glade_xml_signal_connect_data (widget->xml, "on_session_link",
                                       G_CALLBACK (show_session_properties), NULL);
        break;
    
    /* We disable the agent preferences completely */
    case AGENT_OTHER:
        g_message (_("Another password caching agent is running. Disabling cache preferences."));
        w = glade_xml_get_widget (widget->xml, "notebook");
        g_return_if_fail (w != NULL);
        gtk_notebook_remove_page (GTK_NOTEBOOK (w), 1);
        break;
   
    /* Seahorse agent running, behave normally */
    case AGENT_SEAHORSE:
        break;
        
    default:
        g_assert_not_reached ();
        break;
    };
}
