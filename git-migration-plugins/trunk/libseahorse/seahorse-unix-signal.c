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

#include <sys/types.h>
#include <sys/signal.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>

#include <gnome.h>
#include "seahorse-unix-signal.h"

#define MAX_SIGNAL 32
static signal_handler signal_handlers[MAX_SIGNAL] = { NULL, };

static int signal_pipe[2] = { -1, -1 };
static GIOChannel *signal_channel = NULL;
static gint signal_watch_id = 0;


/* The unix signal handler. */
static void 
pipe_signals (int signal)
{
    gchar sig;

    if (signal > MAX_SIGNAL) {
        g_warning ("unix signal number too large: %d", signal);
        return;
    }

    sig = (char)signal;
    if (write (signal_pipe[1], &sig, sizeof (gchar)) != sizeof (gchar))
        g_critical ("unix signal %d lost", signal);
}
  
/* The event loop callback that handles the unix signals. Must be a GIOFunc. */
static gboolean 
deliver_signal (GIOChannel *source, GIOCondition cond, gpointer d)
{
    GError *err = NULL;
    GIOStatus st;
    gsize read;
    gchar sig;

    while ((st = g_io_channel_read_chars (source, &sig, sizeof (gchar), 
                                          &read, &err)) == G_IO_STATUS_NORMAL) {

        g_assert (err == NULL);
        if (read != 1)
            break;
        if (sig < MAX_SIGNAL) {
            if (signal_handlers[(int)sig])
                (signal_handlers[(int)sig]) (sig);
        }
    }
  
    if (err != NULL)
        g_warning ("reading signal pipe failed: %s", err->message);
    if (st == G_IO_STATUS_EOF) 
        g_critical ("signal pipe has been closed");

    return TRUE;
}

static void 
cleanup_signals ()
{
    if (signal_watch_id)
        g_source_remove (signal_watch_id);
    signal_watch_id = 0;

    if (signal_channel)
        g_io_channel_unref (signal_channel);
    signal_channel = NULL;

    if (signal_pipe[0] != -1) {
        close (signal_pipe[0]);
        close (signal_pipe[1]);
        signal_pipe[0] = signal_pipe[1] = -1;
    }
}

void 
seahorse_unix_signal_register (int sig, signal_handler handler)
{
    g_return_if_fail (sig < MAX_SIGNAL);
    g_return_if_fail (handler != NULL);

    /* Setup the signal channel */
    if (signal_channel == NULL) {

        memset (&signal_handlers, 0, sizeof (signal_handlers));
        
        if (pipe (signal_pipe)) {
            g_critical ("can't create signal pipe: %s", strerror (errno));
            return;
        }

        /* Non blocking to prevent deadlock */
        fcntl (signal_pipe[1], F_SETFL, fcntl (signal_pipe[1], F_GETFL) | O_NONBLOCK);

        /* convert the reading end of the pipe into a GIOChannel */
        signal_channel = g_io_channel_unix_new (signal_pipe[0]);
        g_io_channel_set_encoding (signal_channel, NULL, NULL);
        g_io_channel_set_flags (signal_channel, g_io_channel_get_flags (signal_channel) | G_IO_FLAG_NONBLOCK, NULL);

        /* register the reading end with the event loop */
        signal_watch_id = g_io_add_watch (signal_channel, G_IO_IN | G_IO_PRI | G_IO_HUP, deliver_signal, NULL);

        g_atexit (cleanup_signals);
    }

    /* Handle some signals */
    signal (sig, pipe_signals);

    signal_handlers[sig] = handler;
}

