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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "seahorse-discovery.h"
#include "seahorse-pgp-backend.h"
#include "seahorse-server-source.h"

#ifdef WITH_SHARING
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/error.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>
#endif /* WITH_SHARING */

#include "libseahorse/seahorse-util.h"

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


struct _SeahorseDiscoveryPriv {
#ifdef WITH_SHARING
	AvahiClient *client;
	AvahiServiceBrowser *browser;
	AvahiGLibPoll *poll;
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

G_DEFINE_TYPE (SeahorseDiscovery, seahorse_discovery, G_TYPE_OBJECT);

/* -----------------------------------------------------------------------------
 * INTERNAL
 */

#ifdef WITH_SHARING

/**
* self: A SeahorseDiscovery object
*
* Disconnects avahi
*
**/
static void
disconnect (SeahorseDiscovery *self)
{
	if (self->priv->browser && self->priv->client)
		avahi_service_browser_free (self->priv->browser);
	self->priv->browser = NULL;

	if (self->priv->client)
		avahi_client_free (self->priv->client);
	self->priv->client = NULL;

	if (self->priv->poll)
	        avahi_glib_poll_free (self->priv->poll);
	self->priv->poll = NULL;
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
	SeahorseDiscovery *self = SEAHORSE_DISCOVERY (data);
	gchar *service_name;
	gchar *service_uri;
	gchar *ipname;

	g_assert (SEAHORSE_IS_DISCOVERY (self));

	switch(event) {
	case AVAHI_RESOLVER_FAILURE:
		g_warning ("couldn't resolve service '%s': %s", name,
		           avahi_strerror (avahi_client_errno (self->priv->client)));
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

		g_hash_table_replace (self->services, service_name, service_uri);
		g_signal_emit (self, signals[ADDED], 0, service_name);

        /* Add it to the context */
        if (!seahorse_pgp_backend_lookup_remote (NULL, service_uri)) {
            seahorse_pgp_backend_add_remote (NULL, service_uri, FALSE);
        }

		g_debug ("added: %s %s\n", service_name, service_uri);
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
* data: userdata, type SeahorseDiscovery
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
	SeahorseDiscovery *self = SEAHORSE_DISCOVERY (data);
	const gchar *uri;

	g_assert (SEAHORSE_IS_DISCOVERY (self));
	g_assert (browser == self->priv->browser);

	/* XXX Is the service always guaranteed to be ascii? */
	if (g_ascii_strcasecmp (HKP_SERVICE_TYPE, type) != 0)
		return;

	switch (event) {
	case AVAHI_BROWSER_FAILURE:
		g_warning ("failure browsing for services: %s",
		           avahi_strerror (avahi_client_errno (self->priv->client)));
		disconnect (self);
		break;

	case AVAHI_BROWSER_NEW:
		if (!avahi_service_resolver_new (self->priv->client, iface, proto, name, type,
		                                 domain, AVAHI_PROTO_UNSPEC, 0, resolve_callback, self))
			g_warning ("couldn't start resolver for service '%s': %s\n", name,
			           avahi_strerror (avahi_client_errno (self->priv->client)));
		break;

	case AVAHI_BROWSER_REMOVE:
		uri = g_hash_table_lookup (self->services, name);
		if (uri != NULL) {
			/* Remove it from the main context */
			seahorse_pgp_backend_remove_remote (NULL, uri);
		}

		/* And remove it from our tables */
		g_hash_table_remove (self->services, name);
		g_signal_emit (self, signals[REMOVED], 0, name);
		g_debug ("removed: %s\n", name);
		break;

	default:
		break;
	}
}

/**
* client: Will be part of a potential error message
* state: the state of the avahi connection
* data: A SeahorseDiscovery object
*
* Handles AVAHI_CLIENT_FAILURE errors in state
*
**/
static void
client_callback (AvahiClient *client, AvahiClientState state, void *data)
{
	SeahorseDiscovery *self = SEAHORSE_DISCOVERY (data);

	g_assert (SEAHORSE_IS_DISCOVERY (self));

	/* Disconnect when failed */
	if (state == AVAHI_CLIENT_FAILURE) {
		g_return_if_fail (client == self->priv->client);
		g_warning ("failure communicating with to avahi: %s",
		           avahi_strerror (avahi_client_errno (client)));
		disconnect (self);
	}
}

#endif /* WITH_SHARING */

/* -----------------------------------------------------------------------------
 * OBJECT
 */


/**
* self: The SeahorseDiscovery to init
*
* When compiled WITH_SHARING avahi will also be initialised and it will browse
* for avahi services.
**/
static void
seahorse_discovery_init (SeahorseDiscovery *self)
{
	self->priv = g_new0 (SeahorseDiscoveryPriv, 1);
	self->services = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

#ifdef WITH_SHARING
	{
		int aerr;
		avahi_set_allocator (avahi_glib_allocator ());

		self->priv->poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);
		if (!self->priv->poll) {
			g_warning ("couldn't initialize avahi glib poll integration");
			return;
		}

		self->priv->client = avahi_client_new (avahi_glib_poll_get (self->priv->poll),
		                                       0, client_callback, self, &aerr);
		if (!self->priv->client) {
			g_message ("DNS-SD initialization failed: %s", avahi_strerror (aerr));
			return;
		}

		self->priv->browser = avahi_service_browser_new (self->priv->client, AVAHI_IF_UNSPEC,
		                                                 AVAHI_PROTO_UNSPEC, HKP_SERVICE_TYPE, NULL, 0,
		                                                 browse_callback, self);
		if (!self->priv->browser) {
			g_message ("Browsing for DNS-SD services failed: %s",
			           avahi_strerror (avahi_client_errno (self->priv->client)));
			return;
		}
	}
#endif /* WITH_SHARING */
}

/**
* gobject: A SeahorseDiscovery object
*
* Disposes the object
*
**/
static void
seahorse_discovery_dispose (GObject *gobject)
{
#ifdef WITH_SHARING
	SeahorseDiscovery *self = SEAHORSE_DISCOVERY (gobject);
	disconnect (self);
#endif

	G_OBJECT_CLASS (seahorse_discovery_parent_class)->dispose (gobject);
}


/**
* gobject: A SeahorseDiscovery object
*
* free private vars
*
**/
static void
seahorse_discovery_finalize (GObject *gobject)
{
	SeahorseDiscovery *self = SEAHORSE_DISCOVERY (gobject);

#ifdef WITH_SHARING
	g_assert (self->priv->browser == NULL);
	g_assert (self->priv->client == NULL);
#endif

	if (self->services)
		g_hash_table_destroy (self->services);
	self->services = NULL;

	g_free (self->priv);
	self->priv = NULL;

	G_OBJECT_CLASS (seahorse_discovery_parent_class)->finalize (gobject);
}

/**
* klass: The SeahorseDiscoveryClass to initialise
*
* The class will use the signals "added" and "removed"
*
**/
static void
seahorse_discovery_class_init (SeahorseDiscoveryClass *klass)
{
	GObjectClass *gclass;

	seahorse_discovery_parent_class = g_type_class_peek_parent (klass);
	gclass = G_OBJECT_CLASS (klass);

	gclass->dispose = seahorse_discovery_dispose;
	gclass->finalize = seahorse_discovery_finalize;

	signals[ADDED] = g_signal_new ("added", SEAHORSE_TYPE_DISCOVERY,
	                               G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseDiscoveryClass, added),
	                               NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
	signals[REMOVED] = g_signal_new ("removed", SEAHORSE_TYPE_DISCOVERY,
	                                 G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseDiscoveryClass, removed),
	                                 NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
}

/* -----------------------------------------------------------------------------
 * PUBLIC
 */

/**
 * seahorse_discovery_new:
 *
 *
 *
 * Returns: A new #SeahorseDiscovery object
 */
SeahorseDiscovery*
seahorse_discovery_new (void)
{
	return g_object_new (SEAHORSE_TYPE_DISCOVERY, NULL);
}

/**
 * seahorse_discovery_list:
 * @self:  A #SeahorseDiscovery. It's services will be added to the list
 *
 *
 *
 * Returns: the services in @self
 */
gchar **
seahorse_discovery_list (SeahorseDiscovery *self)
{
	GHashTableIter iter;
	GPtrArray *result = g_ptr_array_new ();
	gpointer key;

	g_return_val_if_fail (SEAHORSE_IS_DISCOVERY (self), NULL);
	g_hash_table_iter_init (&iter, self->services);
	while (g_hash_table_iter_next (&iter, &key, NULL))
		g_ptr_array_add (result, g_strdup (key));

	return (gchar **)g_ptr_array_free (result, FALSE);
}

/**
 * seahorse_discovery_get_uri:
 * @self: Service discovery object
 * @service: The service to get the uri for
 *
 *
 *
 * Returns: The URI of the service @service in @self
 */
const gchar*
seahorse_discovery_get_uri (SeahorseDiscovery *self,
                            const gchar *service)
{
	g_return_val_if_fail (SEAHORSE_IS_DISCOVERY (self), NULL);
	return (const gchar*)g_hash_table_lookup (self->services, service);
}

/**
 * seahorse_discovery_get_uris:
 * @self: The service discovery object
 * @services: A list of services
 *
 * The returned uris in the list are copied and must be freed with g_free.
 *
 * Returns: uris for the services
 */
gchar **
seahorse_discovery_get_uris (SeahorseDiscovery *self,
                             const gchar **services)
{
	GPtrArray *result = g_ptr_array_new ();
	const gchar *uri;
	guint i;

	for (i = 0; services && services[i] != NULL; i++) {
		uri = g_hash_table_lookup (self->services, services[i]);
		g_ptr_array_add (result, g_strdup (uri));
	}

	g_ptr_array_add (result, NULL);
	return (gchar **)g_ptr_array_free (result, FALSE);
}
