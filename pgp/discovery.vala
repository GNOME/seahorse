/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
 * Copyright (C) 2017 Niels De Graef
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

/**
 * Seahorse Service Discovery handles (finds, removes,...) Avahi services
 */
public class Seahorse.Discovery : GLib.Object {

    public HashTable<string, string> services;

#if WITH_SHARING
        Avahi.Client client;
        Avahi.ServiceBrowser browser;
        Avahi.GLibPoll poll;
#else
        char no_use;
#endif

    public const string HKP_SERVICE_TYPE = "_pgpkey-hkp._tcp.";

    /*
     * DEBUG: Define if we want to show discoveries from this
     * host. Normally these are suppressed
     */
    /* #define DISCOVER_THIS_HOST 1 */

    /**
     * Fired when a relevant service appeared on the network
     */
    public signal void added(string service);

    /**
     * Fired when a relevant service disappeared
     */
    public signal void removed(string service);

    /**
     * When compiled WITH_SHARING avahi will also be initialised and it will browse
     * for avahi services.
     */
    public Discovery() {
        this.services = new HashTable<string, string>();

#if WITH_SHARING
        /* avahi_set_allocator (avahi_glib_allocator ()); */

        /* this.poll = avahi_glib_poll_new (null, G_PRIORITY_DEFAULT); */
        /* if (!this.poll) { */
        /*     warning ("couldn't initialize avahi glib poll integration"); */
        /*     return; */
        /* } */

        this.client = new Avahi.Client();
        // XXX does the start() function take care of the glib poll?
        //this.client = new Avahi.Client(avahi_glib_poll_get (this.poll));
        try {
            this.client.start();
        } catch (Avahi.Error e) {
            critical("DNS-SD initialization failed: %s", e.message);
            return;
        }

        this.browser = new Avahi.ServiceBrowser.full(Avahi.Interface.UNSPEC, Avahi.Protocol.UNSPEC, HKP_SERVICE_TYPE);
        this.browser.new_service.connect(browse_callback_new);
        this.browser.remove_service.connect(browse_callback_remove);
        this.browser.failure.connect((err) => {
            warning("failure browsing for services: %s", err.message);
            disconnect();
        });
        try {
            this.browser.attach_client(this.client);
        } catch (Avahi.Error e) {
            critical("Browsing for DNS-SD services failed: %s", e.message);
            return;
        }
#endif
    }

#if WITH_SHARING
    ~Discovery() {
        disconnect ();
    }
#endif

    /**
     * Lists all services in this discovery.
     */
    public string[] list() {
        return this.services.get_keys_as_array();
    }

    /**
     * Returns: The URI of the service @service in @self
     *
     * @param service The service to get the uri for
     */
    public string? get_uri(string service) {
        return this.services.lookup(service);
    }

    /**
     * The returned uris in the list are copied and must be freed with g_free.
     *
     * @param services A list of services
     *
     * @return uris for the services
     */
    public string?[] get_uris(string[] services) {
        string[] uris = new string[services.length];

        for (int i = 0; i < services.length; i++)
            uris[i] = this.services.lookup(services[i]);

        return uris;
    }

    /* -----------------------------------------------------------------------------
     * INTERNAL
     */

#if WITH_SHARING

    /**
     * Disconnects avahi
     */
    private void disconnect() {
        if (this.browser != null && this.client != null)
            avahi_service_browser_free (this.browser);
        this.browser = null;

        if (this.client != null)
            avahi_client_free (this.client);
        this.client = null;

        if (this.poll != null)
            avahi_glib_poll_free (this.poll);
        this.poll = null;
    }

    private void resolve_callback(Avahi.Interface iface, Avahi.Protocol proto, string name,
                                  string type, string domain, string host_name,
                                  Avahi.Address? address, uint16 port, Avahi.StringList? txt,
                                  Avahi.LookupResultFlags flags) {
        // Make sure it's our type ...
        // XXX Is the service always guaranteed to be ascii?
        if (HKP_SERVICE_TYPE.ascii_strcasecmp (type) != 0)
            break;

#if ! DISCOVER_THIS_HOST
        // And that it's local
        if (Avahi.LookupResultFlags.LOCAL in flags)
            return;
#endif

        string service_uri = "hkp://%s:%d".printf(address.to_string(), (int)port);

        this.services.replace(name, service_uri);
        added(0, name);

        // Add it to the context
        if (Pgp.Backend.get().lookup_remote(service_uri) == null) {
            SeahorseServerSource *ssrc = seahorse_server_source_new (service_uri);
            if (src == null)
                return;
            Pgp.Backend.get().add_remote(service_uri, ssrc);
        }

        debug("added: %s %s\n", name, service_uri);
    }

    private void browse_callback_new(Avahi.Interface iface, Avahi.Protocol proto, string name,
                                 string type, string domain, Avahi.LookupResultFlags flags) {
        // XXX Is the service always guaranteed to be ascii?
        if (HKP_SERVICE_TYPE.ascii_strcasecmp(type) != 0)
            return;

        Avahi.ServiceResolver resolver = new Avahi.ServiceResolver(iface, proto, name, type,
                                                                   domain, Avahi.Protocol.UNSPEC);
        resolver.found.connect(resolve_callback);
        resolver.found.connect((err) => warning ("couldn't resolve service '%s': %s", name, err.message));
        try {
            resolver.attach(this.client);
        } catch (Error e) {
            warning("couldn't start resolver for service '%s': %s\n", name, e.message);
        }
    }

    private void browse_callback_remove(Avahi.Interface iface, Avahi.Protocol proto, string name,
                                 string type, string domain, Avahi.LookupResultFlags flags) {
        // XXX Is the service always guaranteed to be ascii?
        if (HKP_SERVICE_TYPE.ascii_strcasecmp(type) != 0)
            return;

        string? uri = this.services.lookup(name);

        // Remove it from the main context
        if (uri != null)
            Pgp.Backend.remove_remote(uri);

        // And remove it from our tables
        this.services.remove(name);
        removed(0, name);
        debug("removed: %s\n", name);
    }

    /**
     * Handles AVAHI_CLIENT_FAILURE errors in state
     *
     * @param client Will be part of a potential error message
     * @param state the state of the avahi connection
     */
    private void client_callback (AvahiClient *client, AvahiClientState state) {
        // Disconnect when failed
        if (state == AVAHI_CLIENT_FAILURE) {
            if (client != this.client)
                return;

            warning("failure communicating with to avahi: %s",
                    avahi_strerror (avahi_client_errno (client)));
            disconnect();
        }
    }

#endif /* WITH_SHARING */
}
