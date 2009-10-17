/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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

#include "seahorse-context.h"
#include "seahorse-daemon.h"
#include "seahorse-gconf.h"
#include "seahorse-gtkstock.h"
#include "seahorse-secure-memory.h"
#include "seahorse-unix-signal.h"

#include "common/seahorse-cleanup.h"
#include "common/seahorse-registry.h"

#ifdef WITH_PGP
#include "pgp/seahorse-pgp-module.h"
#endif

#ifdef WITH_SSH
#include "ssh/seahorse-ssh-module.h"
#endif

#include "libegg/eggsmclient.h"
#include "libegg/eggdesktopfile.h"

#include <glib/gi18n.h>

#include <sys/types.h>
#include <sys/signal.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>

static gboolean daemon_no_daemonize = FALSE;
static gboolean daemon_running = FALSE;
static gboolean daemon_quit = FALSE;

static const gchar *daemon_icons[] = {
    SEAHORSE_ICON_SHARING,
    NULL
};

static const GOptionEntry options[] = {
    { "no-daemonize", 'd', 0, G_OPTION_ARG_NONE, &daemon_no_daemonize, 
        N_("Do not run seahorse-daemon as a daemon"), NULL },
    { NULL }
};

/**
 * SECTION:seahorse-daemon
 * @short_description: The seahorse-daemon. Offering services like DBus, HKP and
 * Avahi
 *
 **/

/**
*
* Daemonizing and some special handling for GPGME
*
**/
static void
daemonize ()
{
    /* 
     * We can't use the normal daemon call, because we have
     * special things to do in the parent after forking 
     */

    pid_t pid;
    int i;

    if (!daemon_no_daemonize) {
        switch ((pid = fork ())) {
        case -1:
            err (1, _("couldn't fork process"));
            break;

        /* The child */
        case 0:
            if (setsid () == -1)
                err (1, _("couldn't create new process group"));

            /* Close std descriptors */
            for (i = 0; i <= 2; i++)
                close (i);
            
            /* Open stdin, stdout and stderr. GPGME doesn't work without this */
            open ("/dev/null", O_RDONLY, 0666);
            open ("/dev/null", O_WRONLY, 0666);
            open ("/dev/null", O_WRONLY, 0666);

            if (chdir ("/tmp") < 0)
                warn ("couldn't change to /tmp directory");
            
            return; /* Child process returns */
        };
    }

    /* Not daemonizing */
    else {
        pid = getpid ();
    }

    /* The parent process or not daemonizing ... */
    if (!daemon_no_daemonize)
        exit (0);
}

/**
*
* The signal handler for termination
*
**/
static void
unix_signal (int signal)
{
    daemon_quit = TRUE;
    if (daemon_running)
        gtk_main_quit ();
}

/**
* smclient: ignored
* data: ignored
*
* The handler for the "quit" signal
*
**/
static void
smclient_quit (EggSMClient *smclient, gpointer data)
{
    daemon_quit = TRUE;
    if (daemon_running)
        gtk_main_quit ();    
}

/**
* log_domain: will be added to the message. Can be NULL
* log_level: the log level
* message: the log message
* user_data: passed to g_log_default_handler, nothing else
*
* Writes log messages
*
**/
static void
log_handler (const gchar *log_domain, GLogLevelFlags log_level, 
             const gchar *message, gpointer user_data)
{
    int level;

    /* Note that crit and err are the other way around in syslog */
        
    switch (G_LOG_LEVEL_MASK & log_level) {
    case G_LOG_LEVEL_ERROR:
        level = LOG_CRIT;
        break;
    case G_LOG_LEVEL_CRITICAL:
        level = LOG_ERR;
        break;
    case G_LOG_LEVEL_WARNING:
        level = LOG_WARNING;
        break;
    case G_LOG_LEVEL_MESSAGE:
        level = LOG_NOTICE;
        break;
    case G_LOG_LEVEL_INFO:
        level = LOG_INFO;
        break;
    case G_LOG_LEVEL_DEBUG:
        level = LOG_DEBUG;
        break;
    default:
        level = LOG_ERR;
        break;
    }
    
    /* Log to syslog first */
    if (log_domain)
        syslog (level, "%s: %s", log_domain, message);
    else
        syslog (level, "%s", message);
 
    /* And then to default handler for aborting and stuff like that */
    g_log_default_handler (log_domain, log_level, message, user_data); 
}

/**
*
* Prepares the logging, sets the log handler to the function "log_handler"
*
**/
static void
prepare_logging ()
{
    GLogLevelFlags flags = G_LOG_FLAG_FATAL | G_LOG_LEVEL_ERROR | 
                G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING | 
                G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO;
                
    openlog ("seahorse-daemon", LOG_PID, LOG_AUTH);
    
    g_log_set_handler (NULL, flags, log_handler, NULL);
    g_log_set_handler ("Glib", flags, log_handler, NULL);
    g_log_set_handler ("Gtk", flags, log_handler, NULL);
    g_log_set_handler ("Gnome", flags, log_handler, NULL);
}


/**
 * main:
 * @argc: default arguments
 * @argv: default arguments
 *
 * Parses the options, daemonizes, starts HKP, DBus and Avahi
 *
 * Returns: 0 on success
 */
int main(int argc, char* argv[])
{
    GOptionContext *octx = NULL;
    GError *error = NULL;

    g_thread_init (NULL);
    
    seahorse_secure_memory_init ();
    
    octx = g_option_context_new ("");
    g_option_context_add_main_entries (octx, options, GETTEXT_PACKAGE);
    g_option_context_add_group (octx, egg_sm_client_get_option_group ());

    if (!gtk_init_with_args (&argc, &argv, _("Encryption Daemon (Seahorse)"), 
                             (GOptionEntry *)options, GETTEXT_PACKAGE, &error)) {
	    g_printerr ("seahorse-daemon: %s\n", error->message);
	    g_error_free (error);
	    exit (1);
    }
     	 
	g_signal_connect (egg_sm_client_get (), "quit", G_CALLBACK (smclient_quit), NULL);
    egg_set_desktop_file(AUTOSTARTDIR "/seahorse-daemon.desktop");
    
    /*
     * All functions after this point have to print messages
     * nicely and not just called exit()
     */
    daemonize ();

    /* Handle some signals */
    seahorse_unix_signal_register (SIGINT, unix_signal);
    seahorse_unix_signal_register (SIGTERM, unix_signal);

    /* Force gconf to reconnect after daemonizing */
    if (!daemon_no_daemonize)
        seahorse_gconf_disconnect ();

    /* We log to the syslog */
    prepare_logging ();

    /* Insert Icons into Stock */
    seahorse_gtkstock_init ();
    seahorse_gtkstock_add_icons (daemon_icons); 
   
    /* Make the default SeahorseContext */
    seahorse_context_new (SEAHORSE_CONTEXT_APP | SEAHORSE_CONTEXT_DAEMON);

    /* Load the various components */
#ifdef WITH_PGP
    seahorse_pgp_module_init ();
#endif
#ifdef WITH_SSH
    seahorse_ssh_module_init ();
#endif
    
    seahorse_context_refresh_auto (NULL);
    
    /* Initialize the various daemon components */
    seahorse_dbus_server_init ();

#ifdef WITH_SHARING
    seahorse_sharing_init ();
#endif

    /* Sometimes we've already gotten a quit signal */
    if(!daemon_quit) {
        daemon_running = TRUE;
        gtk_main ();
        g_message ("left gtk_main\n");
    }

    /* And now clean them all up */
#ifdef WITH_SHARING
    seahorse_sharing_cleanup ();
#endif
    
    seahorse_dbus_server_cleanup ();

    g_option_context_free (octx);
    seahorse_context_destroy (SCTX_APP ());
    seahorse_cleanup_perform ();

    return 0;
}
