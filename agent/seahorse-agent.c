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
#include "seahorse-agent.h"
#include "seahorse-gpg-options.h"
#include "seahorse-passphrase.h"
#include "seahorse-pgp-key.h"

gboolean seahorse_agent_displayvars = FALSE;
gboolean seahorse_agent_cshell = FALSE;
gboolean seahorse_agent_execvars = FALSE;

static gboolean seahorse_agent_enabled = FALSE;

/* PUBLISHING AGENT INFO ---------------------------------------------------- */

/* The GPG settings we modify */
static const gchar *confs[2] = {
    "gpg-agent-info",
    NULL
};
    
/* Previous gpg.conf settings */
static gchar *prev_values[2];

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

/* Add our agent info to gpg.conf */
static void
process_gpg_conf (const gchar *socket, pid_t pid)
{
    GError *error = NULL;
    gchar *agent_info;
    gchar *values[2];
    gboolean b;

    g_assert (socket && socket[0]);
    memset (prev_values, 0, sizeof (prev_values));

    /* Read in the current values for the options */
    if (!seahorse_gpg_options_find_vals (confs, prev_values, &error)) {
    
        /* Warn and put in defaults */
        warnx (_("couldn't read gpg configuration, will try to create"));
	  	g_clear_error (&error);
    
        prev_values[0] = NULL;  /* gpg-agent-info */
        prev_values[1] = NULL;  /* null terminate */
    }

    agent_info = g_strdup_printf ("%s:%lu:1", socket, (unsigned long) pid);
    
    values[0] = agent_info;     /* gpg-agent-info */
    values[1] = NULL;           /* null teriminate */
    
    b = seahorse_gpg_options_change_vals (confs, values, &error);

    g_free (agent_info);

    if (!b) {
        g_assert (error);
        errx (1, _("couldn't modify gpg configuration: %s"),
              error ? error->message : "");
        g_clear_error (&error);
    }
}

/* Remove our agent info from gpg.conf */
static void
unprocess_gpg_conf ()
{
    seahorse_gpg_options_change_vals (confs, prev_values, NULL);
}

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

void
seahorse_agent_postfork (pid_t child)
{
    const gchar *socket;

    if(!seahorse_agent_enabled)
        return;
    
    socket = seahorse_agent_io_get_socket ();
    g_return_if_fail (socket != NULL);
    
    /* If any of these fail, they simply exit */
    if (seahorse_agent_displayvars)
        process_display (socket, child);
    else if(seahorse_agent_execvars)
        process_setenv (socket, child);
    else
        process_gpg_conf (socket, child);    
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
    
    return TRUE;
}

void
seahorse_agent_uninit ()
{
    if(!seahorse_agent_enabled)
        return;

    if (!seahorse_agent_displayvars)
        unprocess_gpg_conf ();
        
    /* If any windows are open this closes them */
    seahorse_agent_prompt_cleanup ();
    seahorse_agent_status_cleanup ();

    /* Uninitialize all subsystems */
    seahorse_agent_cache_uninit ();
    seahorse_agent_actions_uninit ();
    seahorse_agent_io_uninit ();
}

