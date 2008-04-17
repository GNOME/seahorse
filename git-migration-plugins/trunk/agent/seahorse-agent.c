/*
 * Seahorse
 *
 * Copyright (C) 2004 Stefan Walter
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

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>

#include <gnome.h>

#include "seahorse-agent.h"
#include "seahorse-gpg-options.h"
#include "seahorse-passphrase.h"
#include "seahorse-pgp-key.h"

gboolean seahorse_agent_cshell = FALSE;
gboolean seahorse_agent_execvars = FALSE;

static gboolean seahorse_agent_enabled = FALSE;

/* PUBLISHING AGENT INFO ---------------------------------------------------- */

/* Print out the socket name info: <name>:<pid>:<protocol_version> */
static void
process_display (const gchar *socket, pid_t pid)
{
    if (seahorse_agent_cshell) {
        fprintf (stdout, "setenv GPG_AGENT_INFO %s:%lu:1\n",
                 socket, (long unsigned int) pid);
    } else {
        fprintf (stdout, "GPG_AGENT_INFO=%s:%lu:1; export GPG_AGENT_INFO\n",
                 socket, (long unsigned int) pid);
    }

    fflush (stdout);
}

static void 
process_setenv (const gchar *socket, pid_t pid)
{
    gchar *var;

    /* Memory doesn't need to be freed */
    var = g_strdup_printf ("%s:%lu:1", socket, (long unsigned int) pid);
    g_setenv ("GPG_AGENT_INFO", var, TRUE);
}

/* 
 * Called before forking as a daemon, creates the GPG agent 
 * socket. This socket path needs to be present and decided
 * on before the fork.
 */
void
seahorse_agent_prefork ()
{
    /* Detect and see if there's an agent */
    switch (seahorse_passphrase_detect_agent (SKEY_PGP)) {
    case SEAHORSE_AGENT_NONE:
        break;
    default:
        g_message ("Another GPG agent already running\n");
        return;
    }
    
    seahorse_agent_enabled = TRUE;
    if (seahorse_agent_io_socket () == -1)
        _exit (1); /* Message already printed */
}

/* 
 * Called after forking off the agent daemon child. At this 
 * point we communicate the socket path to the environment
 * as requested.
 */
void
seahorse_agent_postfork (pid_t child)
{
    const gchar *socket;

    if(!seahorse_agent_enabled)
        return;
    
    socket = seahorse_agent_io_get_socket ();
    g_return_if_fail (socket != NULL);
    
    /* If any of these fail, they simply exit */
    if(seahorse_agent_execvars)
        process_setenv (socket, child);
    else 
        process_display (socket, child);
}

/* 
 * Called when we run another program (such as seahorse-preferences)
 * to setup the environment appropriately for that process.
 */
void 
seahorse_agent_childsetup ()
{
    const gchar *socket = seahorse_agent_io_get_socket ();
    g_return_if_fail (socket != NULL);

    process_setenv (socket, getpid ());
}

gboolean
seahorse_agent_init ()
{
    if(!seahorse_agent_enabled)
        return TRUE;
    
    if (seahorse_agent_io_init () == -1)
        return FALSE;           /* message already printed */
    
    /* Initialize our sub systems */
    seahorse_agent_actions_init ();
    seahorse_agent_cache_init ();
    seahorse_agent_status_init ();
    
    return TRUE;
}

void
seahorse_agent_uninit ()
{
    if(!seahorse_agent_enabled)
        return;

    /* If any windows are open this closes them */
    seahorse_agent_prompt_cleanup ();
    seahorse_agent_status_cleanup ();

    /* Uninitialize all subsystems */
    seahorse_agent_cache_uninit ();
    seahorse_agent_actions_uninit ();
    seahorse_agent_io_uninit ();
}

