/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004 - 2006 Stefan Walter
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
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>

#include <gnome.h>
#include <glade/glade-xml.h>

#include "seahorse-gpgmex.h"
#include "seahorse-libdialogs.h"
#include "seahorse-widget.h"
#include "seahorse-util.h"
#include "seahorse-passphrase.h"
#include "seahorse-secure-entry.h"
#include "seahorse-gpg-options.h"
#include "agent/seahorse-agent.h"

#include "seahorse-ssh-key.h"
#include "seahorse-pgp-key.h"

#define HIG_SMALL      6        /* gnome hig small space in pixels */
#define HIG_LARGE     12        /* gnome hig large space in pixels */

/* Convert passed text to utf-8 if not valid */
static gchar *
utf8_validate (const gchar *text)
{
    gchar *result;

    if (!text)
        return NULL;

    if (g_utf8_validate (text, -1, NULL))
        return g_strdup (text);

    result = g_locale_to_utf8 (text, -1, NULL, NULL, NULL);
    if (!result) {
        gchar *p;

        result = p = g_strdup (text);
        while (!g_utf8_validate (p, -1, (const gchar **) &p))
            *p = '?';
    }
    return result;
}

static gboolean
key_press (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    /* Close the dialog when hitting "Esc". */
    if (event->keyval == GDK_Escape)
    {
        gtk_dialog_response(GTK_DIALOG (widget), GTK_RESPONSE_REJECT);
        return TRUE;
    }
    
    return FALSE;
}

static void
grab_keyboard (GtkWidget *win, GdkEvent *event, gpointer data)
{
#ifndef _DEBUG
    if (gdk_keyboard_grab (win->window, FALSE, gdk_event_get_time (event)))
        g_critical ("could not grab keyboard");
#endif
}

/* ungrab_keyboard - remove grab */
static void
ungrab_keyboard (GtkWidget *win, GdkEvent *event, gpointer data)
{
#ifndef _DEBUG
    gdk_keyboard_ungrab (gdk_event_get_time (event));
#endif
}

/* When enter is pressed in the confirm entry, move */
static void
confirm_callback (GtkWidget *widget, GtkDialog *dialog)
{
    GtkWidget *entry = GTK_WIDGET (g_object_get_data (G_OBJECT (dialog), "secure-entry"));
    g_assert (SEAHORSE_IS_SECURE_ENTRY (entry));
    gtk_widget_grab_focus (entry);
}

/* When enter is pressed in the entry, we simulate an ok */
static void
enter_callback (GtkWidget *widget, GtkDialog *dialog)
{
    gtk_dialog_response (dialog, GTK_RESPONSE_ACCEPT);
}

static void
entry_changed (GtkEditable *editable, GtkDialog *dialog)
{
    SeahorseSecureEntry *entry, *confirm;
    
    entry = SEAHORSE_SECURE_ENTRY (g_object_get_data (G_OBJECT (dialog), "secure-entry"));
    confirm = SEAHORSE_SECURE_ENTRY (g_object_get_data (G_OBJECT (dialog), "confirm-entry"));
    
    gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_ACCEPT, 
                                       strcmp (seahorse_secure_entry_get_text (entry), 
                                               seahorse_secure_entry_get_text (confirm)) == 0);
}

static void
constrain_size (GtkWidget *win, GtkRequisition *req, gpointer data)
{
    static gint width, height;
    GdkGeometry geo;

    if (req->width == width && req->height == height)
        return;

    width = req->width;
    height = req->height;
    geo.min_width = width;
    geo.max_width = 10000;  /* limit is arbitrary, INT_MAX breaks other things */
    geo.min_height = geo.max_height = height;
    gtk_window_set_geometry_hints (GTK_WINDOW (win), NULL, &geo,
                                   GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE);
}

GtkDialog*
seahorse_passphrase_prompt_show (const gchar *title, const gchar *description, 
                                 const gchar *prompt, const gchar *check,
                                 gboolean confirm)
{
    SeahorseSecureEntry *entry;
    GtkDialog *dialog;
    GtkWidget *w;
    GtkWidget *box;
    GtkWidget *ebox;
    GtkTable *table;
    GtkWidget *wvbox;
    GtkWidget *chbox;
    gchar *msg;
    
    if (!title)
        title = _("Passphrase");

    if (!prompt)
        prompt = _("Password:"); 
    
    w = gtk_dialog_new_with_buttons (title, NULL, GTK_DIALOG_NO_SEPARATOR, NULL);
    gtk_window_set_icon_name (GTK_WINDOW (w), GTK_STOCK_DIALOG_AUTHENTICATION);

    dialog = GTK_DIALOG (w);

    g_signal_connect (G_OBJECT (dialog), "size-request",
                      G_CALLBACK (constrain_size), NULL);

    g_signal_connect (G_OBJECT (dialog), "map-event", G_CALLBACK (grab_keyboard), NULL);
    g_signal_connect (G_OBJECT (dialog), "unmap-event", G_CALLBACK (ungrab_keyboard), NULL);

    wvbox = gtk_vbox_new (FALSE, HIG_LARGE * 2);
    gtk_container_add (GTK_CONTAINER (dialog->vbox), wvbox);
    gtk_container_set_border_width (GTK_CONTAINER (wvbox), HIG_LARGE);

    chbox = gtk_hbox_new (FALSE, HIG_LARGE);
    gtk_box_pack_start (GTK_BOX (wvbox), chbox, FALSE, FALSE, 0);

    /* The image */
    w = gtk_image_new_from_stock (GTK_STOCK_DIALOG_AUTHENTICATION, GTK_ICON_SIZE_DIALOG);
    gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.0);
    gtk_box_pack_start (GTK_BOX (chbox), w, FALSE, FALSE, 0);

    box = gtk_vbox_new (FALSE, HIG_SMALL);
    gtk_box_pack_start (GTK_BOX (chbox), box, TRUE, TRUE, 0);

    /* The description text */
    if (description) {
        msg = utf8_validate (description);
        w = gtk_label_new (msg);
        g_free (msg);

        gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
        gtk_label_set_line_wrap (GTK_LABEL (w), TRUE);
        gtk_box_pack_start (GTK_BOX (box), w, TRUE, FALSE, 0);
    }

    /* Two entries (usually on is hidden)  in a vbox */
    table = GTK_TABLE (gtk_table_new (3, 2, FALSE));
    gtk_table_set_row_spacings (table, HIG_SMALL);
    gtk_table_set_col_spacings (table, HIG_LARGE);
    gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (table), FALSE, FALSE, 0);

    /* The first entry if we have one */
    if (confirm) {
        ebox = gtk_hbox_new (FALSE, HIG_LARGE);
        msg = utf8_validate (prompt);
        w = gtk_label_new (msg);
        g_free (msg);
        gtk_table_attach (table, w, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

        entry = SEAHORSE_SECURE_ENTRY (seahorse_secure_entry_new ());
        gtk_widget_set_size_request (GTK_WIDGET (entry), 200, -1);
        g_object_set_data (G_OBJECT (dialog), "confirm-entry", entry);
        g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (confirm_callback), dialog);
        g_signal_connect (G_OBJECT (entry), "changed", G_CALLBACK (entry_changed), dialog);
        gtk_table_attach_defaults (table, GTK_WIDGET (entry), 1, 2, 0, 1);
        gtk_widget_grab_focus (GTK_WIDGET (entry));
    }

    /* The second and main entry */
    ebox = gtk_hbox_new (FALSE, HIG_LARGE);
    msg = utf8_validate (confirm ? _("Confirm:") : prompt);
    w = gtk_label_new (msg);
    g_free (msg);
    gtk_table_attach (table, w, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
    
    entry = SEAHORSE_SECURE_ENTRY (seahorse_secure_entry_new ());
    gtk_widget_set_size_request (GTK_WIDGET (entry), 200, -1);
    g_object_set_data (G_OBJECT (dialog), "secure-entry", entry);
    g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (enter_callback), dialog);
    gtk_table_attach_defaults (table, GTK_WIDGET (entry), 1, 2, 1, 2);
    if (!confirm)
        gtk_widget_grab_focus (GTK_WIDGET (entry));
    else
        g_signal_connect (G_OBJECT (entry), "changed", G_CALLBACK (entry_changed), dialog);

    /* The checkbox */
    if (check) {
        w = gtk_check_button_new_with_mnemonic (check);
        gtk_table_attach_defaults (table, w, 1, 2, 2, 3);
        g_object_set_data (G_OBJECT (dialog), "check-option", w);
    }

    gtk_widget_show_all (GTK_WIDGET (table));
    
    w = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
    gtk_dialog_add_action_widget (dialog, w, GTK_RESPONSE_REJECT);
    GTK_WIDGET_SET_FLAGS (w, GTK_CAN_DEFAULT);

    w = gtk_button_new_from_stock (GTK_STOCK_OK);
    gtk_dialog_add_action_widget (dialog, w, GTK_RESPONSE_ACCEPT);
    GTK_WIDGET_SET_FLAGS (w, GTK_CAN_DEFAULT);
    g_signal_connect_object (G_OBJECT (entry), "focus_in_event",
                             G_CALLBACK (gtk_widget_grab_default), G_OBJECT (w), 0);
    gtk_widget_grab_default (w);
    
    g_signal_connect (dialog, "key_press_event", G_CALLBACK (key_press), NULL);

    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_keep_above(GTK_WINDOW (dialog), TRUE);
    gtk_window_present (GTK_WINDOW (dialog));
    gtk_widget_show_all (GTK_WIDGET (dialog));

    if (confirm)
        entry_changed (NULL, dialog);
    
    return dialog;
}

const gchar*
seahorse_passphrase_prompt_get (GtkDialog *dialog)
{
    SeahorseSecureEntry *entry;

    entry = SEAHORSE_SECURE_ENTRY (g_object_get_data (G_OBJECT (dialog), "secure-entry"));
    return seahorse_secure_entry_get_text (entry);
}

gboolean
seahorse_passphrase_prompt_checked (GtkDialog *dialog)
{
    GtkToggleButton *button = GTK_TOGGLE_BUTTON (g_object_get_data (G_OBJECT (dialog), "check-option"));
    return GTK_IS_TOGGLE_BUTTON (button) ? gtk_toggle_button_get_active (button) : FALSE;
}


gpgme_error_t
seahorse_passphrase_get (gconstpointer dummy, const gchar *passphrase_hint, 
                         const char* passphrase_info, int flags, int fd)
{
    GtkDialog *dialog;
    gpgme_error_t err;
    gchar **split_uid = NULL;
    gchar *label = NULL;
    gchar *errmsg = NULL;
    const gchar *pass;
    
    if (passphrase_info && strlen(passphrase_info) < 16)
        flags |= SEAHORSE_PASS_NEW;
    
    if (passphrase_hint)
        split_uid = g_strsplit (passphrase_hint, " ", 2);

    if (flags & SEAHORSE_PASS_BAD) 
        errmsg = g_strdup_printf (_("Wrong passphrase."));
    
    if (split_uid && split_uid[0] && split_uid[1]) {
        if (flags & SEAHORSE_PASS_NEW) 
            label = g_strdup_printf (_("Enter new passphrase for '%s'"), split_uid[1]);
        else 
            label = g_strdup_printf (_("Enter passphrase for '%s'"), split_uid[1]);
    } else {
        if (flags & SEAHORSE_PASS_NEW) 
            label = g_strdup (_("Enter new passphrase"));
        else 
            label = g_strdup (_("Enter passphrase"));
    }

    g_strfreev (split_uid);

    dialog = seahorse_passphrase_prompt_show (NULL, errmsg ? errmsg : label, 
                                              NULL, NULL, FALSE);
    g_free (label);
    g_free (errmsg);
    
    switch (gtk_dialog_run (dialog)) {
    case GTK_RESPONSE_ACCEPT:
        pass = seahorse_passphrase_prompt_get (dialog);
        seahorse_util_printf_fd (fd, "%s\n", pass);
        err = GPG_OK;
        break;
    default:
        err = GPG_E (GPG_ERR_CANCELED);
        break;
    };
    
    gtk_widget_destroy (GTK_WIDGET (dialog));
    return err;
}

/* -----------------------------------------------------------------------------
 * GPG AGENT 
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
static SeahorseAgentType
check_agent_id (int fd)
{
    SeahorseAgentType ret = SEAHORSE_AGENT_NONE;
    GIOChannel *io;
    gchar *t;
    GError *err = NULL;

    io = g_io_channel_unix_new (fd);

    /* Server always sends a response first */
    if (g_io_channel_read_line (io, &t, NULL, NULL, NULL) == G_IO_STATUS_NORMAL && t) {
        g_strstrip (t);
        if (g_str_has_prefix (t, "OK"))
            ret = SEAHORSE_AGENT_OTHER;
        g_free (t);

        /* Send back request for info */
        if (ret != SEAHORSE_AGENT_NONE &&
            g_io_channel_write_chars (io, "AGENT_ID\n", -1, NULL,
                                      &err) == G_IO_STATUS_NORMAL
            && g_io_channel_flush (io, &err) == G_IO_STATUS_NORMAL
            && g_io_channel_read_line (io, &t, NULL, NULL,
                                       &err) == G_IO_STATUS_NORMAL && t) {
            g_strstrip (t);
            if (g_str_has_prefix (t, "OK seahorse-agent"))
                ret = SEAHORSE_AGENT_SEAHORSE;
            g_free (t);
        }
    }

    g_io_channel_shutdown (io, FALSE, NULL);
    g_io_channel_unref (io);
    
    if (err) {
        g_warning ("couldn't check GPG agent: %s", err->message);
        g_error_free (err);
        ret = SEAHORSE_AGENT_UNKNOWN;
    }
    
    return ret;
}

/* Open a connection to our agent */
static SeahorseAgentType
get_listening_agent_type (const gchar *sockname)
{
    struct sockaddr_un addr;
    SeahorseAgentType ret;
    int len;
    int fd;

    /* Agent is always a unix socket */
    fd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1)
        return SEAHORSE_AGENT_UNKNOWN;
    
    memset (&addr, 0, sizeof (addr));
    addr.sun_family = AF_UNIX;
    g_strlcpy (addr.sun_path, sockname, sizeof (addr.sun_path));
    len = offsetof (struct sockaddr_un, sun_path) + strlen (addr.sun_path) + 1;

    if (connect (fd, (const struct sockaddr *) &addr, len) == 0)
        ret = check_agent_id (fd);
    else
        ret = SEAHORSE_AGENT_UNKNOWN;

    shutdown (fd, SHUT_RDWR);
    close (fd);
    return ret;
}

/* Given an agent info string make sure it's running and is our agent */
static SeahorseAgentType
check_agent_info (const gchar *agent_info)
{
    SeahorseAgentType ret = SEAHORSE_AGENT_NONE;
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

static SeahorseAgentType
gpg_detect_agent ()
{
    gchar *value = NULL;

    /* Seahorse edits gpg.conf by default */
    seahorse_gpg_options_find ("gpg-agent-info", &value, NULL);
    if (value != NULL) {
        SeahorseAgentType ret = check_agent_info (value);
        g_free (value);
        return ret;
    }

    /* The user probably set this up on their own */
    value = (gchar*)g_getenv ("GPG_AGENT_INFO");
    if (value != NULL)
        return check_agent_info (value);

    return SEAHORSE_AGENT_NONE;
}

/* -----------------------------------------------------------------------------
 * SSH AGENT 
 */

const guchar REQ_MESSAGE[] = { 0x00, 0x00, 0x00, 0x01, SEAHORSE_SSH_PING_MSG };
const guint RESP_INDEX = 4;
const guint RESP_VALUE = SEAHORSE_SSH_PING_MSG;

static SeahorseAgentType
ssh_detect_agent ()
{
    SeahorseAgentType ret;
    const gchar *socketpath;
    struct sockaddr_un sunaddr;
    guchar buf[16];
    GIOChannel *io;
    int agentfd;
    gsize bytes_read = 0;
    GError *err = NULL;
    
    /* Guarantee we have enough space */
    g_assert (sizeof (buf) > RESP_INDEX);
    
    socketpath = g_getenv ("SSH_AUTH_SOCK");
    if (!socketpath || !socketpath[0])
        return SEAHORSE_AGENT_NONE;
    
    /* Try to connect to the real agent */
    agentfd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (agentfd == -1) {
        g_warning ("couldn't create socket: %s", g_strerror (errno));
        return SEAHORSE_AGENT_UNKNOWN;
    }
    
    memset (&sunaddr, 0, sizeof (sunaddr));
    sunaddr.sun_family = AF_UNIX;
    g_strlcpy (sunaddr.sun_path, socketpath, sizeof (sunaddr.sun_path));
    if (connect (agentfd, (struct sockaddr*) &sunaddr, sizeof (sunaddr)) < 0) {
        g_warning ("couldn't connect to SSH agent at: %s: %s", socketpath, 
                   g_strerror (errno));
        close (agentfd);
        return SEAHORSE_AGENT_UNKNOWN;
    }
    
    io = g_io_channel_unix_new (agentfd);
    g_io_channel_set_close_on_unref (io, TRUE);
    g_io_channel_set_encoding (io, NULL, NULL);
    g_io_channel_set_buffered (io, FALSE);

    /* Send and receive our message */
    if (g_io_channel_write_chars (io, (const gchar*)REQ_MESSAGE, sizeof (REQ_MESSAGE), NULL, 
                                  &err) == G_IO_STATUS_NORMAL) {
        g_io_channel_read_chars (io, (gchar*)buf, sizeof (buf), &bytes_read, &err);
    }
    
    if (!err) {
        if (bytes_read > RESP_INDEX && buf[RESP_INDEX] == RESP_VALUE)
            ret = SEAHORSE_AGENT_SEAHORSE;
        else
            ret = SEAHORSE_AGENT_OTHER;
    } else {
        g_warning ("couldn't check for SSH agent: %s", err ? err->message : "");
        ret = SEAHORSE_AGENT_UNKNOWN;
    }
    
    g_io_channel_unref (io);
    return ret;
}

/* -------------------------------------------------------------------------- */

/* Check if the agent is running */
SeahorseAgentType
seahorse_passphrase_detect_agent (GQuark ktype)
{
    if (ktype == SKEY_PGP)
        return gpg_detect_agent ();
    if (ktype == SKEY_SSH)
        return ssh_detect_agent ();
    g_return_val_if_reached (SEAHORSE_AGENT_UNKNOWN);
}
