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

#ifdef WITH_SHARING 
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/error.h>
#endif /* WITH_SHARING */

#include "seahorse-util.h"
#include "seahorse-dns-sd.h"
#include "seahorse-context.h"

#ifdef WITH_PGP
#include "pgp/seahorse-server-source.h"
#endif

#define DEBUG_FLAG SEAHORSE_DEBUG_DNSSD
#include "seahorse-debug.h"

#define HKP_SERVICE_TYPE "_pgpkey-hkp._tcp."

/* 
 * DEBUG: Define if we want to show discoveries from this
 * host. Normally these are suppressed 
 */
/* #define DISCOVER_THIS_HOST 1 */


/**
 * SECTION:seahorse-dns-sd
 * @short_description: Seahorse Service Discovery handles (finds, removes,...) Avahi services
 * @include:libseahorse/seahorse-dns-sd.h
 *
 **/


struct _SeahorseServiceDiscoveryPriv {
#ifdef WITH_SHARING
    AvahiClient *client;
    AvahiServiceBrowser *browser;
#else
    char no_use;
#endif
};

enum {
    ADDED,
    REMOVED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (SeahorseServiceDiscovery, seahorse_service_discovery, G_TYPE_OBJECT);

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

#ifdef WITH_SHARING

/**
* ssd: A SeahorseServiceDiscovery object
*
* Disconnects avahi
*
**/
static void
disconnect (SeahorseServiceDiscovery *ssd)
{
    if (ssd->priv->browser && ssd->priv->client)
        avahi_service_browser_free (ssd->priv->browser);
    ssd->priv->browser = NULL;
    
    if (ssd->priv->client)
        avahi_client_free (ssd->priv->client);
    ssd->priv->client = NULL;
}

/**
* resolver: The avahi resolver
* iface: The avahi interface
* proto: The avahi protocol
* event: the avahi event
* name: name of the service
* type: must be HKP_SERVICE_TYPE
* domain: ignored
* host_name: ignored
* address: address of the service
* port: port of the service
* txt: ignored
* flags:
* data: userdata
*
* Resolves the avahi callback (AvahiServiceResolverCallback)
*
**/
static void 
resolve_callback (AvahiServiceResolver *resolver, AvahiIfIndex iface, AvahiProtocol proto, 
                  AvahiResolverEvent event, const char *name, const char *type, const char *domain,
                  const char *host_name, const AvahiAddress *address, uint16_t port, 
                  AvahiStringList *txt, AvahiLookupResultFlags flags, void *data)
{
    SeahorseServiceDiscovery *ssd = SEAHORSE_SERVICE_DISCOVERY (data);
    gchar *service_name;
    gchar *service_uri;
    gchar *ipname;
    
    g_assert (SEAHORSE_IS_SERVICE_DISCOVERY (ssd));


    switch(event) {
    case AVAHI_RESOLVER_FAILURE:
        g_warning ("couldn't resolve service '%s': %s", name, 
                   avahi_strerror (avahi_client_errno (ssd->priv->client)));
        break;
    
    
    case AVAHI_RESOLVER_FOUND:
        
        /* Make sure it's our type ... */
        /* XXX Is the service always guaranteed to be ascii? */
        if (g_ascii_strcasecmp (HKP_SERVICE_TYPE, type) != 0)
            break;
        
#ifndef DISCOVER_THIS_HOST
        /* And that it's local */
        if (flags & AVAHI_LOOKUP_RESULT_LOCAL)
            break;
#endif
        
        ipname = g_new0(gchar, AVAHI_ADDRESS_STR_MAX);
        avahi_address_snprint (ipname, AVAHI_ADDRESS_STR_MAX, address);
        
        service_uri = g_strdup_printf ("hkp://%s:%d", ipname, (int)port);
        service_name = g_strdup (name);
        
        g_hash_table_replace (ssd->services, service_name, service_uri);
        g_signal_emit (ssd, signals[ADDED], 0, service_name);
    
        /* Add it to the context */
        if (!seahorse_context_remote_source (SCTX_APP (), service_uri)) {
            SeahorseServerSource *ssrc = seahorse_server_source_new (service_uri);
            g_return_if_fail (ssrc != NULL);
            seahorse_context_add_source (SCTX_APP (), SEAHORSE_SOURCE (ssrc));
        }
    
        seahorse_debug ("added: %s %s\n", service_name, service_uri);
        break;
        
    default:
        break;
    };

    /* One result is enough for us */
    avahi_service_resolver_free (resolver);
}

/**
* browser: The avahi browser
* iface: the interface
* proto: the protocol
* event: the event that happened while browsing (failure, new, remove)
* name: name of the service
* type: HKP_SERVICE_TYPE is expected
* domain: domain of the service
* flags: ignored
* data: userdata, type SeahorseServiceDiscovery
*
* Called when a service is discovered while browsing.
* It is a AvahiServiceBrowserCallback
*
**/
static void 
browse_callback(AvahiServiceBrowser *browser, AvahiIfIndex iface, AvahiProtocol proto,
                AvahiBrowserEvent event, const char *name, const char *type, 
                const char *domain, AvahiLookupResultFlags flags, void* data) 
{
    SeahorseServiceDiscovery *ssd = SEAHORSE_SERVICE_DISCOVERY (data);
    const gchar *uri;
    
    g_assert (SEAHORSE_IS_SERVICE_DISCOVERY (ssd));
    g_assert (browser == ssd->priv->browser);
    
     /* XXX Is the service always guaranteed to be ascii? */
    if (g_ascii_strcasecmp (HKP_SERVICE_TYPE, type) != 0)
        return;

    switch (event) {
    case AVAHI_BROWSER_FAILURE:
        g_warning ("failure browsing for services: %s", 
                   avahi_strerror (avahi_client_errno (ssd->priv->client)));
        disconnect (ssd);
        break;
    
    
    case AVAHI_BROWSER_NEW:
        if (!avahi_service_resolver_new (ssd->priv->client, iface, proto, name, type, 
                                         domain, AVAHI_PROTO_UNSPEC, 0, resolve_callback, ssd))
            g_warning ("couldn't start resolver for service '%s': %s\n", name, 
                       avahi_strerror (avahi_client_errno (ssd->priv->client)));
        break;
        
        
    case AVAHI_BROWSER_REMOVE:
        uri = g_hash_table_lookup (ssd->services, name);
        if (uri != NULL) {
            /* Remove it from the main context */
            SeahorseSource *sksrc = seahorse_context_remote_source (SCTX_APP(), uri);
            seahorse_context_remove_source (SCTX_APP(), sksrc);
        }
        
        /* And remove it from our tables */
        g_hash_table_remove (ssd->services, name);
        g_signal_emit (ssd, signals[REMOVED], 0, name);
        seahorse_debug ("removed: %s\n", name);
        break;
        
    
    default:
        break;
    }
}

/**
* client: Will be part of a potential error message
* state: the state of the avahi connection
* data: A SeahorseServiceDiscovery object
*
* Handles AVAHI_CLIENT_FAILURE errors in state
*
**/
static void 
client_callback (AvahiClient *client, AvahiClientState state, void *data) 
{
    SeahorseServiceDiscovery *ssd = SEAHORSE_SERVICE_DISCOVERY (data);
    
    g_assert (SEAHORSE_IS_SERVICE_DISCOVERY (ssd));
    
    /* Disconnect when failed */
    if (state == AVAHI_CLIENT_FAILURE) {
        g_return_if_fail (client == ssd->priv->client);
        g_warning ("failure communicating with to avahi: %s", 
                   avahi_strerror (avahi_client_errno (client)));
        disconnect (ssd);
    }
}

#endif /* WITH_SHARING */

/* -----------------------------------------------------------------------------
 * OBJECT 
 */


/**
* ssd: The SeahorseServiceDiscovery to init
*
* When compiled WITH_SHARING avahi will also be initialised and it will browse
* for avahi services.
**/
static void
seahorse_service_discovery_init (SeahorseServiceDiscovery *ssd)
{
    ssd->priv = g_new0 (SeahorseServiceDiscoveryPriv, 1);
    ssd->services = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    
#ifdef WITH_SHARING
	{
		int aerr;
		ssd->priv->client = avahi_client_new (seahorse_util_dns_sd_get_poll (), 
		                                      0, client_callback, ssd, &aerr);
		if (!ssd->priv->client) {
			g_message ("DNS-SD initialization failed: %s", avahi_strerror (aerr));
			return;
		}
    
		ssd->priv->browser = avahi_service_browser_new (ssd->priv->client, AVAHI_IF_UNSPEC, 
		                                                AVAHI_PROTO_UNSPEC, HKP_SERVICE_TYPE, NULL, 0, 
		                                                browse_callback, ssd);
		if (!ssd->priv->browser) {
			g_message ("Browsing for DNS-SD services failed: %s",
			           avahi_strerror (avahi_client_errno (ssd->priv->client)));
			return;
		}
	}
#endif /* WITH_SHARING */
}

/**
* gobject: A SeahorseServiceDiscovery object
*
* Disposes the object
*
**/
static void
seahorse_service_discovery_dispose (GObject *gobject)
{
#ifdef WITH_SHARING
    SeahorseServiceDiscovery *ssd = SEAHORSE_SERVICE_DISCOVERY (gobject);
    disconnect (ssd);
#endif 
    
    G_OBJECT_CLASS (seahorse_service_discovery_parent_class)->dispose (gobject);
}


/**
* gobject: A SeahorseServiceDiscovery object
*
* free private vars
*
**/
static void
seahorse_service_discovery_finalize (GObject *gobject)
{
    SeahorseServiceDiscovery *ssd = SEAHORSE_SERVICE_DISCOVERY (gobject);
  
#ifdef WITH_SHARING
    g_assert (ssd->priv->browser == NULL);
    g_assert (ssd->priv->client == NULL);
#endif     

    if (ssd->services)    
        g_hash_table_destroy (ssd->services);
    ssd->services = NULL;
    
    g_free (ssd->priv);
    ssd->priv = NULL;
    
    G_OBJECT_CLASS (seahorse_service_discovery_parent_class)->finalize (gobject);
}

/**
* klass: The SeahorseServiceDiscoveryClass to initialise
*
* The class will use the signals "added" and "removed"
*
**/
static void
seahorse_service_discovery_class_init (SeahorseServiceDiscoveryClass *klass)
{
    GObjectClass *gclass;
   
    seahorse_service_discovery_parent_class = g_type_class_peek_parent (klass);
    gclass = G_OBJECT_CLASS (klass);
    
    gclass->dispose = seahorse_service_discovery_dispose;
    gclass->finalize = seahorse_service_discovery_finalize;

    signals[ADDED] = g_signal_new ("added", SEAHORSE_TYPE_SERVICE_DISCOVERY, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseServiceDiscoveryClass, added),
                NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
    signals[REMOVED] = g_signal_new ("removed", SEAHORSE_TYPE_SERVICE_DISCOVERY, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseServiceDiscoveryClass, removed),
                NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

/**
 * seahorse_service_discovery_new:
 *
 *
 *
 * Returns: A new #SeahorseServiceDiscovery object
 */
SeahorseServiceDiscovery*   
seahorse_service_discovery_new (void)
{
    return g_object_new (SEAHORSE_TYPE_SERVICE_DISCOVERY, NULL);
}

/**
 * seahorse_service_discovery_list:
 * @ssd:  A #SeahorseServiceDiscovery. It's services will be added to the list
 *
 *
 *
 * Returns: the services in @ssd
 */
gchar **
seahorse_service_discovery_list (SeahorseServiceDiscovery *ssd)
{
	GHashTableIter iter;
	GPtrArray *result = g_ptr_array_new ();
	gpointer key;

	g_return_val_if_fail (SEAHORSE_IS_SERVICE_DISCOVERY (ssd), NULL);
	g_hash_table_iter_init (&iter, ssd->services);
	while (g_hash_table_iter_next (&iter, &key, NULL))
		g_ptr_array_add (result, g_strdup (key));

	return (gchar **)g_ptr_array_free (result, FALSE);
}

/**
 * seahorse_service_discovery_get_uri:
 * @ssd: Service discovery object
 * @service: The service to get the uri for
 *
 *
 *
 * Returns: The URI of the service @service in @ssd
 */
const gchar*                
seahorse_service_discovery_get_uri (SeahorseServiceDiscovery *ssd, const gchar *service)
{
    g_return_val_if_fail (SEAHORSE_IS_SERVICE_DISCOVERY (ssd), NULL);
    return (const gchar*)g_hash_table_lookup (ssd->services, service);
}

/**
 * seahorse_service_discovery_get_uris:
 * @ssd: The service discovery object
 * @services: A list of services
 *
 * The returned uris in the list are copied and must be freed with g_free.
 *
 * Returns: uris for the services
 */
gchar **
seahorse_service_discovery_get_uris (SeahorseServiceDiscovery *ssd, const gchar **services)
{
	GPtrArray *result = g_ptr_array_new ();
	const gchar *uri;
	guint i;

	for (i = 0; services && services[i] != NULL; i++) {
		uri = g_hash_table_lookup (ssd->services, services[i]);
		g_ptr_array_add (result, g_strdup (uri));
	}

	g_ptr_array_add (result, NULL);
	return (gchar **)g_ptr_array_free (result, FALSE);
}
