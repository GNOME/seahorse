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
#include <sys/signal.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <syslog.h>

#include <gnome.h>

#include "config.h"
#include "seahorse-agent.h"
#include "seahorse-agent-secmem.h"
#include "seahorse-gpg-options.h"

/* -----------------------------------------------------------------------------
 *  GLOBALS
 */

gboolean g_daemonize = TRUE;
gboolean g_displayvars = FALSE;
gboolean g_cshell = FALSE;
gboolean g_quit = FALSE;

/* The GPG settings we modify */
static const gchar *confs[4] = {
    "gpg-agent-info",
    "use-agent",
    "no-agent",
    NULL
};
    
/* Previous gpg.conf settings */
static gchar *prev_values[4];

/* -----------------------------------------------------------------------------
 */

/* Print out the socket name info: <name>:<pid>:<protocol_version> */
static void
process_display (const char *sockname, pid_t pid)
{
    if (g_cshell) {
        fprintf (stdout, "setenv GPG_AGENT_INFO %s:%lu:1",
                 sockname, (long unsigned int) pid);
    } else {
        fprintf (stdout, "GPG_AGENT_INFO=%s:%lu:1; export GPG_AGENT_INFO",
                 sockname, (long unsigned int) pid);
    }

    fflush (stdout);
}

/* Remove our agent info from gpg.conf */
static void
unprocess_gpg_conf ()
{
    seahorse_gpg_options_change_vals (confs, prev_values, NULL);
}

/* Add our agent info to gpg.conf */
static void
process_gpg_conf (const char *sockname, pid_t pid)
{
    GError *error = NULL;
    gchar *agent_info;
    gchar *values[4];
    gboolean b;

    g_assert (sockname && sockname[0]);
    memset (prev_values, 0, sizeof (prev_values));

    /* Read in the current values for the options */
    if (!seahorse_gpg_options_find_vals (confs, prev_values, &error)) {
    
        /* Warn and put in defaults */
        warnx (_("couldn't read gpg configuration: %s"), 
            error ? error->message : "");
	  	g_clear_error (&error);
    
        prev_values[0] = NULL;  /* gpg-agent-info */
        prev_values[1] = NULL;  /* use-agent */
        prev_values[2] = NULL;  /* no-agent */
        prev_values[3] = NULL;  /* null terminate */
    }

    agent_info = g_strdup_printf ("%s:%lu:1", sockname, (unsigned long) pid);
    
    values[0] = agent_info;     /* gpg-agent-info */
    values[1] = "";             /* use-agent */
    values[2] = NULL;           /* no-use-agent */
    values[3] = NULL;           /* null teriminate */
    
    b = seahorse_gpg_options_change_vals (confs, values, &error);

    g_free (agent_info);

    if (!b) {
        g_assert (error);
        errx (1, _("couldn't modify gpg configuration: %s"),
              error ? error->message : "");
        g_clear_error (error);
    }
}

static void
daemonize (const char *sockname)
{
    /* 
     * We can't use the normal daemon call, because we have
     * special things to do in the parent after forking 
     */

    pid_t pid;
    int i;

    if (g_daemonize) {
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

            chdir ("/");
            return;
        };
    }

    /* Not daemonizing */
    else {
        pid = getpid ();
    }

    /* The parent, or not daemonized */

    if (g_displayvars)
        process_display (sockname, pid);
    else
        process_gpg_conf (sockname, pid);

    if (g_daemonize)
        exit (0);
}

static void
usage ()
{
    fprintf (stderr, _("usage: seahorse-agent [-cdv]\n"));
    exit (2);
}

static void
on_quit (int signal)
{
    g_quit = 1;
}

static gboolean
check_quit (gpointer data)
{
    if (g_quit) {
        gtk_main_quit ();
        return FALSE;
    }

    return TRUE;
}

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
    if(log_domain)
        syslog (level, "%s: %s", log_domain, message);
    else
        syslog (level, "%s", message);
 
    /* And then to default handler for aborting and stuff like that */
    g_log_default_handler (log_domain, log_level, message, user_data); 
}
                                             
static void
prepare_logging ()
{
    GLogLevelFlags flags = G_LOG_FLAG_FATAL | G_LOG_LEVEL_ERROR | 
                G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING | 
                G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO;
                
    openlog ("seahorse-agent", LOG_PID, LOG_AUTH);
    
    g_log_set_handler (NULL, flags, log_handler, NULL);
    g_log_set_handler ("Glib", flags, log_handler, NULL);
    g_log_set_handler ("Gtk", flags, log_handler, NULL);
    g_log_set_handler ("Gnome", flags, log_handler, NULL);
}                

int
main (int argc, char *argv[])
{
    int ch = 0;
    const char *sockname;

    secmem_init (65536);

    /* We need to drop privileges completely for security */
#if defined(HAVE_SETRESUID) && defined(HAVE_SETRESGID)
    if (setresuid (getuid (), getuid (), getuid ()) == -1 ||
        setresgid (getgid (), getgid (), getgid ()) == -1)
#else
    if (setuid (getuid ()) == -1 || setgid (getgid ()) == -1)
#endif
        err (1, _("couldn't drop privileges properly"));

    gtk_init (&argc, &argv);

    /* Parse the arguments nicely */
    while ((ch = getopt (argc, argv, "cdv")) != -1) {
        switch (ch) {
            /* Uses a C type shell */
        case 'c':
            g_cshell = TRUE;
            break;

            /* Don't daemonize */
        case 'd':
            g_daemonize = FALSE;
            break;

            /* Edit GPG conf instead of displaying vars */
        case 'v':
            g_displayvars = FALSE;
            break;

        case '?':
        default:
            usage ();
        };
    }

    argc -= optind;
    argv += optind;

    if (argc > 0)
        usage ();

    if (seahorse_agent_io_socket (&sockname) == -1)
        return 1;               /* message already printed */

    /* 
     * All functions after this point have to print messages 
     * nicely and not just called exit() 
     */
    daemonize (sockname);

    /* Handle some signals */
    signal (SIGINT, on_quit);
    signal (SIGTERM, on_quit);
    g_timeout_add (100, check_quit, NULL);
    
    /* We log to the syslog */
    prepare_logging ();

    /* Initialize our sub systems */
    seahorse_agent_actions_init ();
    seahorse_agent_cache_init ();

    if (seahorse_agent_io_init () == -1)
        return 1;               /* message already printed */

    gtk_main ();

    if (!g_displayvars)
        unprocess_gpg_conf ();

    /* If any windows are open this closes them */
    seahorse_agent_prompt_cleanup ();
    seahorse_agent_status_cleanup ();

    /* Uninitialize all subsystems */
    seahorse_agent_cache_uninit ();
    seahorse_agent_actions_uninit ();
    seahorse_agent_io_uninit ();

    return 0;
}
