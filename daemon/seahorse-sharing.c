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

#include <config.h>

#include <glib/gi18n.h>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/error.h>

#include "seahorse-gtkstock.h"

#include "config.h"
#include "seahorse-daemon.h"
#include "seahorse-gconf.h"
#include "seahorse-util.h"

#define HKP_SERVICE_TYPE "_pgpkey-hkp._tcp."

/**
 * SECTION:seahorse-sharing
 * @short_description: Starts the HKP service and offers it using Avahi
 *
 **/

/* TODO: Need to be able to advertize in real DNS domains */

static void start_sharing ();
static void stop_sharing ();

/* For the docklet icon */
static guint gconf_notify_id = 0;

/* DNS-SD publishing -------------------------------------------------------- */

static AvahiEntryGroup *avahi_group = NULL;
static AvahiClient *avahi_client = NULL;

/* Name and 'alternate' integer (for collisions). */
static gchar *share_name = NULL;
static int share_alternate = 0;

/**
* errmsg: if TRUE an error message will be shown
*
* Stops the Avahi service
*
**/
static void
stop_publishing (gboolean errmsg)
{
    if (share_name)
        g_free (share_name);
    share_name = NULL;
    
    if (avahi_group)
        avahi_entry_group_free (avahi_group);
    avahi_group = NULL;
    
    if (avahi_client)
        avahi_client_free (avahi_client);
    avahi_client = NULL;

    if (errmsg) {
        seahorse_util_show_error (NULL, _("Couldn't share keys"), 
                                  _("Can't publish discovery information on the network."));
    }
}

/**
*
* Calcs a share name and stores it in share_name
*
* TODO: The share name must be < 63 characters. Or avahi_entry_group_add_service
* will fail. (happens only in EXTREME circumstances or never at all)
**/
static void
calc_share_name ()
{
    const gchar *user_name;
    gchar *t = NULL;

    
    user_name = g_get_real_name ();
    if (!user_name || g_str_equal (user_name, "Unknown"))
        user_name = t = seahorse_util_string_up_first (g_get_user_name ());

    /* Translators: The %s will get filled in with the user name
        of the user, to form a genitive. If this is difficult to
        translate correctly so that it will work correctly in your
        language, you may use something equivalent to
        "Shared keys of %s". */
    share_name = g_strdup_printf (_("%s's encryption keys"), user_name);
    g_free (t);
    
    if (share_alternate) {
        t = g_strdup_printf ("%s #%d", share_name, share_alternate); 
        g_free (share_name);
        share_name = t;
    }
}

/**
*
* Adds the HKP service to the avahi_group the port advertised is the one of
* the hkp server
*
**/
static void
add_service ()
{
    int r;

    r = avahi_entry_group_add_service (avahi_group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 
                                           0, share_name, HKP_SERVICE_TYPE, NULL, NULL, 
                                           seahorse_hkp_server_get_port (), NULL);
    if (r >= 0)
        r = avahi_entry_group_commit (avahi_group);

    if (r < 0) {
        g_warning ("failed to register _pgpkey-hkp._tcp service: %s", avahi_strerror (r));
        stop_publishing (TRUE);
        return;
    }
}

/**
* group: the group
* state: the new state the group is in
* userdata: ignored
*
* Callback called whenever the state of the group changes
*
* Acts on collisions and failure
**/
static void 
services_callback(AvahiEntryGroup *group, AvahiEntryGroupState state, 
                  AVAHI_GCC_UNUSED void *userdata) 
{
    
    g_assert (!avahi_group || group == avahi_group);
    
    switch (state) {
    case AVAHI_ENTRY_GROUP_COLLISION:
        /* Someone else has our registered name */
        share_alternate++;
        calc_share_name ();
        g_warning ("naming collision trying new name: %s", share_name);
        add_service ();
        break;
    
    case AVAHI_ENTRY_GROUP_FAILURE:
        g_warning ("avahi entry group failure: %s", 
                   avahi_strerror (avahi_client_errno (avahi_client)));
        stop_publishing (TRUE);
        break;
    
    default:
        break;
    };
}

/**
* client: the Avahi client
* state: the state in which the client is now
* userdata: ignored
*
* Callback called whenever the state of the Avahi client changes
*
* Adds a group on startup and a service, acts on collisions and failures
**/
static void 
client_callback (AvahiClient *client, AvahiClientState state, 
                 AVAHI_GCC_UNUSED void * userdata) 
{
    g_assert (!avahi_client || client == avahi_client);
    
    switch (state) {
    case AVAHI_CLIENT_S_RUNNING:
        /* Create a new entry group if necessary */
        if (!avahi_group) {
            avahi_group = avahi_entry_group_new (client, services_callback, NULL);
            if (!avahi_group) {
                g_warning ("couldn't create avahi services group: %s", 
                           avahi_strerror (avahi_client_errno (client)));
                stop_publishing (TRUE);
                return;
            }
        }

        /* Add add the good stuff */
        add_service ();
        break;
    
    case AVAHI_CLIENT_S_COLLISION:
        /* Drop our published services */
        if (avahi_group)
            avahi_entry_group_reset (avahi_group);
        break;
    
    case AVAHI_CLIENT_FAILURE:
        g_warning ("failure talking with avahi: %s", 
                   avahi_strerror (avahi_client_errno (client)));
        stop_publishing (TRUE);
        break;
    
    default:
        break;
    };
}

/**
*
* Calcs a new share name and starts a new Avahi client
*
* Returns FALSE on error
**/
static gboolean
start_publishing ()
{
    int aerr;

    /* The share name is important */
    share_alternate = 0;
    calc_share_name ();
    
    avahi_client = avahi_client_new (seahorse_util_dns_sd_get_poll (),
                                     0, client_callback, NULL, &aerr);
    if (!avahi_client)
        return FALSE;
    
    return TRUE;
}

/* -------------------------------------------------------------------------- */

/**
*
* starts the hkp server and the Avahi client
*
**/
static void
start_sharing ()
{
    GError *err = NULL;
    
    if (!seahorse_hkp_server_is_running ()) {
        
        if (!seahorse_hkp_server_start (&err)) {
            seahorse_util_handle_error (err, _("Couldn't share keys"));
            g_clear_error (&err);
            return;
        }

        if (!start_publishing ()) {
            seahorse_hkp_server_stop ();

            stop_publishing (TRUE);
            return;
        }
    }
}

/**
*
* Stops the hkp server and the Avahi publishing of the service
*
**/
static void
stop_sharing ()
{
    stop_publishing (FALSE);
    
    if (seahorse_hkp_server_is_running ())
        seahorse_hkp_server_stop ();
}

/**
* client: ignored
* id: ignored
* entry: The KEYSHARING_KEY GConf entry
* data: user data, ignored
*
* A callback function to listen for GConf changes. Starts and stops HKP and
* Avahi depending on the settings.
*
**/
static void
gconf_notify (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
    if (g_str_equal (KEYSHARING_KEY, gconf_entry_get_key (entry))) {
        if (gconf_value_get_bool (gconf_entry_get_value (entry)))
            start_sharing ();
        else
            stop_sharing ();
    }
}

/**
 * seahorse_sharing_init:
 *
 * Starts Avahi depending on the GConf settings. Also adds a callback to listen
 * for GConf changes. Also starts the HKP server.
 *
 */
void 
seahorse_sharing_init ()
{
    gconf_notify_id = seahorse_gconf_notify (KEYSHARING_KEY, gconf_notify, NULL);
    
    if (seahorse_gconf_get_boolean (KEYSHARING_KEY))
        start_sharing();
}

/**
 * seahorse_sharing_cleanup:
 *
 * Removes the gconf callback and stops Avahi
 *
 */
void 
seahorse_sharing_cleanup ()
{
    if (gconf_notify_id)
        seahorse_gconf_unnotify (gconf_notify_id);
    gconf_notify_id = 0;

    stop_sharing ();    
}
