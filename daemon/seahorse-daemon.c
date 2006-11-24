/*
 * Seahorse
 *
 * Copyright (C) 2005 Nate Nielsen
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

#include "config.h"
#include "seahorse-daemon.h"
#include "seahorse-gtkstock.h"
#include "seahorse-context.h"

#ifdef WITH_AGENT
#include "seahorse-agent.h"
#include "seahorse-secure-memory.h"
#endif

static gboolean g_daemonize = TRUE;
static gboolean g_quit = FALSE;

static const gchar *daemon_icons[] = {
    SEAHORSE_ICON_SHARING,
    NULL
};


static const struct poptOption options[] = {
	{ "no-daemonize", 'd', POPT_ARG_NONE | POPT_ARG_VAL, &g_daemonize, FALSE,
	  N_("Do not daemonize seahorse-agent"), NULL },

#ifdef WITH_AGENT
	{ "cshell", 'c', POPT_ARG_NONE | POPT_ARG_VAL, &seahorse_agent_cshell, TRUE,
	  N_("Print variables in for a C type shell"), NULL },

	{ "variables", 'v', POPT_ARG_NONE | POPT_ARG_VAL, &seahorse_agent_displayvars, TRUE,
	  N_("Display variables instead of editing conf files (gpg.conf, ssh agent socket)"), NULL },

	{ "execute", 'x', POPT_ARG_NONE | POPT_ARG_VAL, &seahorse_agent_execvars, TRUE,
	  N_("Execute other arguments on the command line"), NULL },
#endif
      
	POPT_AUTOHELP
	
	POPT_TABLEEND
};

static void
daemonize (const gchar **exec)
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
            
            /* Open stdin, stdout and stderr. GPGME doesn't work without this */
            open ("/dev/null", O_RDONLY, 0666);
            open ("/dev/null", O_WRONLY, 0666);
            open ("/dev/null", O_WRONLY, 0666);

            chdir ("/tmp");
            return; /* Child process returns */
        };
    }

    /* Not daemonizing */
    else {
        pid = getpid ();
    }

    /* The parent process or not daemonizing ... */

#ifdef WITH_AGENT
    /* Let the agent do it's thing */
    seahorse_agent_postfork (pid);
    seahorse_agent_ssh_postfork (pid);
#endif
    
    if (g_daemonize) {

        /* If we were asked to exec another program, do that here */
        if (!exec || !exec[0])
            exit (0);

        execvp (exec[0], (char**)exec);
	    g_critical ("couldn't exec %s: %s\n", exec[0], strerror (errno));
	    exit (1);

    } else {

        /* We can't overlay our process with the exec one if not daemonizing */
        if (exec && exec[0])
            g_warning ("cannot execute process when not daemonizing: %s", exec[0]);    

    }
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
    if (log_domain)
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
                
    openlog ("seahorse-daemon", LOG_PID, LOG_AUTH);
    
    g_log_set_handler (NULL, flags, log_handler, NULL);
    g_log_set_handler ("Glib", flags, log_handler, NULL);
    g_log_set_handler ("Gtk", flags, log_handler, NULL);
    g_log_set_handler ("Gnome", flags, log_handler, NULL);
}

static void
client_die ()
{
    gtk_main_quit ();
}

int main(int argc, char* argv[])
{
    SeahorseOperation *op;
    GnomeProgram *program = NULL;
    GnomeClient *client = NULL;
    const char **args = NULL;
    poptContext pctx;
    GValue value = { 0, };

    seahorse_secure_memory_init (65536);
    
    /* We need to drop privileges completely for security */
#if defined(HAVE_SETRESUID) && defined(HAVE_SETRESGID)

    /* Not in header files for all OSs, even where present */
    int setresuid(uid_t ruid, uid_t euid, uid_t suid);
    int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
  
    if (setresuid (getuid (), getuid (), getuid ()) == -1 ||
        setresgid (getgid (), getgid (), getgid ()) == -1)
#else
    if (setuid (getuid ()) == -1 || setgid (getgid ()) == -1)
#endif
        err (1, _("couldn't drop privileges properly"));
    
    program = gnome_program_init("seahorse-daemon", VERSION, LIBGNOMEUI_MODULE, argc, argv,
                    GNOME_PARAM_POPT_TABLE, options,
                    GNOME_PARAM_HUMAN_READABLE_NAME, _("Encryption Daemon (Seahorse)"),
                    GNOME_PARAM_APP_DATADIR, DATA_DIR, NULL);

#ifdef WITH_AGENT    
    seahorse_agent_prefork ();
    seahorse_agent_ssh_prefork ();
    
    /* If we need to run another program, then prepare that */
    if (seahorse_agent_execvars) {
        g_value_init (&value, G_TYPE_POINTER);
        g_object_get_property (G_OBJECT (program), GNOME_PARAM_POPT_CONTEXT, &value);
    
        pctx = g_value_get_pointer (&value);
        g_value_unset (&value);

        args = poptGetArgs(pctx);
    }
#endif

    /* 
     * All functions after this point have to print messages 
     * nicely and not just called exit() 
     */
    daemonize (args);

    /* Handle some signals */
    signal (SIGINT, on_quit);
    signal (SIGTERM, on_quit);
    
    client = gnome_master_client();
    g_signal_connect(client, "die", G_CALLBACK(client_die), NULL);
    
    /* We log to the syslog */
    prepare_logging ();

    /* Insert Icons into Stock */
    seahorse_gtkstock_init ();
    seahorse_gtkstock_add_icons (daemon_icons);
    
    /* Make the default SeahorseContext */
    seahorse_context_new (SEAHORSE_CONTEXT_APP | SEAHORSE_CONTEXT_DAEMON, 0);
    op = seahorse_context_load_local_keys (SCTX_APP ());
    g_object_unref (op);
    
    /* Initialize the various daemon components */
    seahorse_dbus_server_init ();

#ifdef WITH_AGENT
    if (!seahorse_agent_init ())
        seahorse_agent_uninit ();
#ifdef WITH_SSH
    if (!seahorse_agent_ssh_init ())
        seahorse_agent_ssh_uninit ();
#endif
#endif
    
#ifdef WITH_SHARING
    seahorse_sharing_init ();
#endif

    /* Sometimes we've already gotten a quit signal */
    if(!g_quit) {
        g_timeout_add (100, check_quit, NULL);
        gtk_main ();
        g_message ("left gtk_main\n");
    }

    /* And now clean them all up */
#ifdef WITH_SHARING
    seahorse_sharing_cleanup ();
#endif
    
#ifdef WITH_AGENT
    seahorse_agent_uninit ();
#ifdef WITH_SSH
    seahorse_agent_ssh_uninit ();
#endif
#endif
    
    seahorse_dbus_server_cleanup ();

    seahorse_context_destroy (SCTX_APP ());

    return 0;
}
