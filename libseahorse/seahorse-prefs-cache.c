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

#include "config.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <signal.h>
#include <stdlib.h>
#include <gnome.h>

#include "seahorse-context.h"
#include "seahorse-widget.h"
#include "seahorse-gpg-options.h"
#include "seahorse-gconf.h"
#include "seahorse-util.h"
#include "seahorse-check-button-control.h"

#define AGENT_SETTINGS      "/apps/seahorse/agent"
#define SETTING_CACHE       AGENT_SETTINGS "/cache_enabled"
#define SETTING_METHOD      AGENT_SETTINGS "/cache_method"
#define SETTING_TTL         AGENT_SETTINGS "/cache_ttl"
#define SETTING_EXPIRE      AGENT_SETTINGS "/cache_expire"
#define SETTING_AUTH        AGENT_SETTINGS "/cache_authorize"

#define METHOD_INTERNAL     "internal"
#define METHOD_GNOME        "gnome"

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

/* Check if the server at the other end of the socket is our agent */
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

/* Open a connection to our agent */
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

/* Given an agent info string make sure it's running and is our agent */
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

static void
update_cache_choices (const char *gconf_key, SeahorseWidget *swidget)
{
    GtkWidget *widget;
    gchar *str;
    gint ttl;
    gboolean set;
    
    if (strcmp (gconf_key, SETTING_CACHE) == 0) {
        
        set = seahorse_gconf_get_boolean (SETTING_CACHE);
        if (!set) {
            widget = seahorse_widget_get_widget (swidget, "no-cache");
            g_return_if_fail (widget != NULL);
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
        }
        
        return;
    }
    
    if (strcmp (gconf_key, SETTING_METHOD) == 0) {
        
        str = seahorse_gconf_get_string (SETTING_METHOD);
        if (!str || strcmp (str, METHOD_GNOME) != 0)
            widget = seahorse_widget_get_widget (swidget, "session-cache");
        else
            widget = seahorse_widget_get_widget (swidget, "keyring-cache");
        g_free (str);

        g_return_if_fail (widget != NULL);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
        
        return;
    }
    
    if (strcmp (gconf_key, SETTING_TTL) == 0) {
        
        ttl = seahorse_gconf_get_integer (SETTING_TTL);
        if (ttl < 0)
            ttl = 0;
        widget = seahorse_widget_get_widget (swidget, "ttl");
        g_return_if_fail (widget != NULL);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), ttl);
        
        return;
    }
}

static void
cache_gconf_notify (GConfClient *client, guint id, 
              GConfEntry *entry, SeahorseWidget *swidget)
{
    update_cache_choices (gconf_entry_get_key (entry), swidget);
}


static void
save_cache_choices (GtkWidget *unused, SeahorseWidget *swidget)
{
    GtkWidget *widget, *widget_ttl;
    int ttl;
    
    widget_ttl = seahorse_widget_get_widget (swidget, "ttl");
    g_return_if_fail (widget_ttl != NULL);
    
    widget = seahorse_widget_get_widget (swidget, "no-cache");
    g_return_if_fail (widget != NULL);
    
    /* No cache */
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
        seahorse_gconf_set_boolean (SETTING_CACHE, FALSE);
        gtk_widget_set_sensitive (widget_ttl, FALSE);
        
        return;
    }
    
    widget = seahorse_widget_get_widget (swidget, "session-cache");
    g_return_if_fail (widget != NULL);
    
    /* Session cache */
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
        seahorse_gconf_set_boolean (SETTING_CACHE, TRUE);
        seahorse_gconf_set_string (SETTING_METHOD, METHOD_INTERNAL);
        gtk_widget_set_sensitive (widget_ttl, TRUE);
        ttl = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget_ttl));
        seahorse_gconf_set_boolean (SETTING_EXPIRE, ttl > 0 ? TRUE : FALSE);
        seahorse_gconf_set_integer (SETTING_TTL, ttl);
        return;
    }
    
#ifdef WITH_GNOME_KEYRING
    
    widget = seahorse_widget_get_widget (swidget, "keyring-cache");
    g_return_if_fail (widget != NULL);
    
    /* gnome-keyring cache */
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
        seahorse_gconf_set_boolean (SETTING_CACHE, TRUE);
        seahorse_gconf_set_string (SETTING_METHOD, METHOD_GNOME);
        gtk_widget_set_sensitive (widget_ttl, FALSE);
        seahorse_gconf_set_boolean (SETTING_EXPIRE, FALSE);
    }

#endif /* WITH_GNOME_KEYRING */
    
}

static void
save_ttl (GtkSpinButton *spinner, SeahorseWidget *swidget)
{
    int ttl = gtk_spin_button_get_value_as_int (spinner);
    if (ttl < 0)
        ttl = 0;
    seahorse_gconf_set_integer (SETTING_TTL, ttl);
}

/* Start up the gnome-session-properties */
static void
show_session_properties (GtkWidget *widget, gpointer data)
{
    GError *err = NULL;

    g_spawn_command_line_async ("gnome-session-properties", &err);

    if (err)
        seahorse_util_handle_error (err, _("Couldn't open the Session Properties"));
}

/* Startup our agent (seahorse-daemon) */
static void
start_agent (GtkWidget *widget, gpointer data)
{
    GError *err = NULL;
    gint status;

    g_spawn_command_line_sync ("seahorse-daemon", NULL, NULL, &status, &err);

    if (err)
        seahorse_util_handle_error (err, _("Couldn't start the 'seahorse-daemon' program"));
    else if (!(WIFEXITED (status) && WEXITSTATUS (status) == 0))
        seahorse_util_handle_error (NULL, _("The 'seahorse-daemon' program exited unsuccessfully."));
    else {
        /* Show the next message about starting up automatically */
        gtk_widget_hide (gtk_widget_get_parent (gtk_widget_get_parent (widget)));
        gtk_widget_show (GTK_WIDGET (data));
    }
}

/* Generate the Hand Cursor */
void
set_hand_cursor_on_realize(GtkWidget *widget, gpointer user_data)
{
    GdkCursor *cursor;

    cursor = gdk_cursor_new (GDK_HAND2);
    gdk_window_set_cursor (GTK_BUTTON (widget)->event_window, cursor);
    gdk_cursor_unref (cursor);
}

/* Find button label, underline and paint it blue. 
 * TODO: Get the system theme link color and use that instead of default blue.
 **/
void
paint_button_label_as_link (GtkButton *button, GtkLabel *label)
{
    const gchar * button_text;
    gchar *markup;
    
    button_text = gtk_label_get_label (label);
    
    markup = g_strdup_printf ("<u>%s</u>", button_text);
    gtk_label_set_markup (GTK_LABEL (label), markup);
    g_free (markup);

    GdkColor *link_color;
    GdkColor blue = { 0, 0x0000, 0x0000, 0xffff }; /* Default color */

    /* Could optionaly set link_color to the current theme color... */
    link_color = &blue;

    gtk_widget_modify_fg (GTK_WIDGET (label),
                  GTK_STATE_NORMAL, link_color);
    gtk_widget_modify_fg (GTK_WIDGET (label),
                  GTK_STATE_ACTIVE, link_color);
    gtk_widget_modify_fg (GTK_WIDGET (label),
                  GTK_STATE_PRELIGHT, link_color);
    gtk_widget_modify_fg (GTK_WIDGET (label),
                  GTK_STATE_SELECTED, link_color);

    if (link_color != &blue)
        gdk_color_free (link_color);
}

/* Initialize the cache tab */
void
seahorse_prefs_cache (SeahorseWidget *widget)
{
    GtkWidget *w, *w2;
    
    g_return_if_fail (widget != NULL);
    
    w = seahorse_widget_get_widget (widget, "no-cache");
    g_return_if_fail (w != NULL);
    g_signal_connect_after (w, "toggled", G_CALLBACK (save_cache_choices), widget);
    
    w = seahorse_widget_get_widget (widget, "session-cache");
    g_return_if_fail (w != NULL);
    g_signal_connect_after (w, "toggled", G_CALLBACK (save_cache_choices), widget);
    
    w = seahorse_widget_get_widget (widget, "keyring-cache");
    g_return_if_fail (w != NULL);
#ifdef WITH_GNOME_KEYRING    
    g_signal_connect_after (w, "toggled", G_CALLBACK (save_cache_choices), widget);
#else
    gtk_widget_hide (w);
#endif 
    
    w = seahorse_widget_get_widget (widget, "ttl");
    g_return_if_fail (w != NULL);
    g_signal_connect_after (w, "value-changed", G_CALLBACK (save_ttl), widget);
    
    /* Initial values, then listen */
    update_cache_choices (SETTING_CACHE, widget);
    update_cache_choices (SETTING_METHOD, widget);
    update_cache_choices (SETTING_TTL, widget);
    seahorse_gconf_notify_lazy (AGENT_SETTINGS, (GConfClientNotifyFunc)cache_gconf_notify, 
                                widget, widget);
                                
    /* Authorize check button */
    w = seahorse_widget_get_widget (widget, "authorize");
    g_return_if_fail (w != NULL);
    seahorse_check_button_gconf_attach (GTK_CHECK_BUTTON (w), SETTING_AUTH);

    /* Setup daemon button visuals */
    w = glade_xml_get_widget (widget->xml, "session-link");
    g_return_if_fail (w != NULL);
    
    w2 = glade_xml_get_widget (widget->xml, "label-start-seahorse-daemon");
    g_return_if_fail (w2 != NULL);
    
    paint_button_label_as_link (GTK_BUTTON (w), GTK_LABEL(w2));
    g_signal_connect (GTK_WIDGET (w), "realize", G_CALLBACK (set_hand_cursor_on_realize), NULL);

    w = glade_xml_get_widget (widget->xml, "start-link");
    g_return_if_fail (w != NULL);
    
    w2 = glade_xml_get_widget (widget->xml, "label-session-properties");
    g_return_if_fail (w2 != NULL);
    
    paint_button_label_as_link (GTK_BUTTON (w), GTK_LABEL(w2));
    g_signal_connect (GTK_WIDGET (w), "realize", G_CALLBACK (set_hand_cursor_on_realize), NULL);
    /* End -- Setup daemon button visuals */
    
    glade_xml_signal_connect_data (widget->xml, "on_session_link",
                                   G_CALLBACK (show_session_properties), NULL);

    switch (which_agent_running ()) {
      
    /* No agent running offer to start */
    case AGENT_NONE:
        w = glade_xml_get_widget (widget->xml, "agent-start");
        g_return_if_fail (w != NULL);
        gtk_widget_show (w);

        glade_xml_signal_connect_data (widget->xml, "on_start_link",
                                       G_CALLBACK (start_agent),
                                       glade_xml_get_widget (widget->xml, "agent-started"));
        break;
    
    /* We disable the agent preferences completely */
    case AGENT_OTHER:
        g_message (_("Another passphrase caching agent is running. Disabling cache preferences."));
        w = glade_xml_get_widget (widget->xml, "notebook");
        g_return_if_fail (w != NULL);
        gtk_notebook_remove_page (GTK_NOTEBOOK (w), 1);
        break;
   
    /* Seahorse agent running, behave normally */
    case AGENT_SEAHORSE:
        gtk_widget_show (GTK_WIDGET (glade_xml_get_widget (widget->xml, "agent-started")));
        break;
        
    default:
        g_assert_not_reached ();
        break;
    };
}
