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
#include <stdlib.h>
#include <string.h>

#include <gnome.h>
#include <eel/eel.h>

#include "seahorse-agent.h"

/*
 * Implements a queue of SeahorseAgentPassReq items. We can only show one dialog
 * to the user at a time, due to keyboard grabbing issues, but mainly 
 * for the users sanity. 
 */

static GQueue *g_queue = NULL;          /* The queue of SeahorseAgentPassReq items */
static GMemChunk *g_memory = NULL;      /* Allocator for SeahorseAgentPassReq items */

/* Whether to prompt authorize accesses to the cache or not */
#define SETTING_AUTH "/apps/seahorse/agent/cache_authorize"

/* -----------------------------------------------------------------------------
 * IMPLEMENTATION
 */

void
seahorse_agent_actions_init ()
{
    /* The main memory queue */
    g_memory = g_mem_chunk_create (SeahorseAgentPassReq, 128, G_ALLOC_AND_FREE);
    g_queue = g_queue_new ();
}

void
seahorse_agent_actions_uninit ()
{
    if (g_queue);
    {
        /* All memory for elements freed below */
        g_queue_free (g_queue);
        g_queue = NULL;
    }

    if (g_memory) {
        g_mem_chunk_destroy (g_memory);
        g_memory = NULL;
    }
}

/* Called for the assuan GET_PASSPHRASE command */
void
seahorse_agent_actions_getpass (SeahorseAgentConn * rq, gchar * id,
                                gchar * errmsg, gchar * prompt, gchar * desc)
{
    SeahorseAgentPassReq *pr;
    const gchar *pass;

    g_assert (rq != NULL);

    if (id && !eel_gconf_get_boolean (SETTING_AUTH)) {
        /* 
         * We don't need authorization, so if we have the password
         * just reply now, without going to the queue.
         */
        if ((pass = seahorse_agent_cache_get (id)) != NULL) {
            seahorse_agent_io_reply (rq, TRUE, pass);
            return;
        }
    }

    /* A new queue item */
    pr = g_chunk_new (SeahorseAgentPassReq, g_memory);
    memset (pr, 0, sizeof (*pr));
    pr->id = id ? g_strdup (id) : NULL;
    pr->errmsg = errmsg ? g_strdup (errmsg) : NULL;
    pr->prompt = g_strdup (prompt ? prompt : _("Passphrase:"));
    pr->description = g_strdup (desc ? desc : _("Please enter a passphrase to use."));
    pr->request = rq;
    g_queue_push_head (g_queue, pr);

    /* Process the queue */
    seahorse_agent_actions_nextgui ();
}

static void
free_passreq (SeahorseAgentPassReq * pr)
{
    if (pr->id)
        g_free ((gpointer) pr->id);
    if (pr->errmsg)
        g_free ((gpointer) pr->errmsg);
    if (pr->prompt)
        g_free ((gpointer) pr->prompt);
    if (pr->description)
        g_free ((gpointer) pr->description);

    /* Just in case, should already be popped */
    g_queue_remove (g_queue, pr);

    g_chunk_free (pr, g_memory);
}

/* Called when a authorize prompt completes (send back the cached password) */
void
seahorse_agent_actions_doneauth (SeahorseAgentPassReq * pr, gboolean authorized)
{
    const gchar *pass = NULL;

    g_assert (pr != NULL);

    if (authorized) {
        /* 
         * The password will have been locked in the cache by the 
         * time we arrive here. The code that checks that the password
         * exists also locks it into the cache.
         */
        g_assert (pr->id);
        pass = seahorse_agent_cache_get (pr->id);
        g_assert (pass != NULL);
    }

    seahorse_agent_actions_donepass (pr, pass);
}

/* Called when a password prompt completes (send back new passord) */
void
seahorse_agent_actions_donepass (SeahorseAgentPassReq * pr, const gchar * pass)
{
    if (pass == NULL)
        seahorse_agent_io_reply (pr->request, FALSE, "111 cancelled");
    else
        seahorse_agent_io_reply (pr->request, TRUE, pass);

    free_passreq (pr);
    seahorse_agent_actions_nextgui ();
}

/* Prompts are queued. This is called to display the next if any */
void
seahorse_agent_actions_nextgui ()
{
    SeahorseAgentPassReq *pr;

    g_assert (g_queue != NULL);

    /* If we already have some gui thing going on, then wait */
    if (seahorse_agent_prompt_have ())
        return;

    if (g_queue_is_empty (g_queue))
        return;

    pr = g_queue_pop_tail (g_queue);
    if (pr != NULL) {
        /*
         * Always prompt when we have an error message. If we already
         * have something in the cache, however, we can just authorize
         */
        if (!pr->errmsg && pr->id && seahorse_agent_cache_has (pr->id, TRUE)) {
            /* Do we need to authorize with the user? */
            if (eel_gconf_get_boolean (SETTING_AUTH))
                seahorse_agent_prompt_auth (pr);

            /* Simple auto-authorize */
            else
                seahorse_agent_actions_doneauth (pr, TRUE);
        }

        /* Prompt for the password */
        else {
            seahorse_agent_prompt_pass (pr);
        }

        /* 
         * Once either prompt is done it'll call the 
         * seahorse_agent_actions_donexxxx functions above
         */
    }

    seahorse_agent_actions_nextgui ();
}

/* Called for the assuan CLEAR_PASSPHRASE request */
void
seahorse_agent_actions_clrpass (SeahorseAgentConn * rq, gchar * id)
{
    if (id != NULL) {
        seahorse_agent_cache_clear (id);
        seahorse_agent_io_reply (rq, TRUE, NULL);
    }
}
