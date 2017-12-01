/*
 * Seahorse
 *
 * Copyright (C) 2004 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

namespace Seahorse {
// XXX correct namespace?

/**
 * Objects for handling of internet sources of keys (hkp/ldap)
 */
public abstract class ServerSource : GLib.Object, Gcr.Collection, Seahorse.Place {

    /**
     * Key Server to search on
     */
    public string server { get; construct; }

    /**
     * Key Server full URI
     */
    public string uri { get; set; }

    public Icon icon {
        owned get { return new ThemedIcon(Gtk.STOCK.DIALOG_QUESTION); }
    }

    public Gtk.ActionGroup? actions {
        owned get { return null; }
    }

    /**
     * Creates a #SeahorseServerSource object out of @server. Depending
     * on the defines at compilation time other sources are supported
     * (ldap, hkp)
     *
     * XXX This is a factory method, not a constructor
     *
     * @param server The server uri to create an object for
     */
    public ServerSource? create(string? server) {
        if(server != null && server != "")
            return null;


        string scheme, host;
        if (!parse_keyserver_uri (server, out scheme, out host)) {
            warning ("invalid uri passed: %s", server);
            return null;
        }

        switch (scheme.down()) {

            case "ldap":
                if (Config.WITH_LDAP)
                    return new LdapSource(server, host);
                break;

            case "hkp":
                if (Config.WITH_HKP)
                    return new HkpSource(server, host);
                break;

            case "http":
            case "https":
                if (Config.WITH_HKP) {
                    // If there's already a port, use that
                    if (':' in server)
                        return new HkpSource(server, host);

                    // Otherwise, use default ports
                    int port = 80 : 443;
                    return new HkpSource("%s:%d".printf(server, port), host);
                }
                break;

            default:
                message("Unsupported key server uri scheme: %s", scheme);
        }

        return null;
    }

    /* public async bool load(GLib.Cancellable? cancellable) throws GLib.Error { */
    /*     return false; */
    /* } */

    public uint get_length() {
        return 0;
    }

    public List<GLib.Object> get_objects() {
        return null;
    }

    public bool contains(GLib.Object object) {
        return false;
    }

    /* --------------------------------------------------------------------------
     * METHODS
     */

    /**
     * Code adapted from GnuPG (file g10/keyserver.c)
     *
     * @param uri the uri to parse
     * @param scheme the scheme ("http") of this uri
     * @param host the host part of the uri
     *
     * @return false if the separation failed
     */
    private bool parse_keyserver_uri(string uri, out string scheme, out string host) {
        assert(uri != null);
        assert(scheme != null && host != null);

        int assume_ldap = 0;

        *scheme = NULL;
        *host = NULL;

        /* Get the scheme */
        *scheme = strsep(&uri, ":");
        if (uri == NULL) {
            /* Assume LDAP if there is no scheme */
            assume_ldap = 1;
            uri = (char*)*scheme;
            *scheme = "ldap";
        }

        if (assume_ldap || (uri[0] == '/' && uri[1] == '/')) {
            // Two slashes means network path.

            // Skip over the "//", if any
            if (!assume_ldap)
                uri += 2;

            // Get the host
            *host = strsep (&uri, "/");
            if (*host[0] == '\0')
                return false;
        }

        if (*scheme[0] == '\0')
            return false;

        return true;
    }

    public abstract async bool search_async(string match, Gcr.SimpleCollection results, Cancellable cancellable) throws GLib.Error;

    public abstract async void* export_async(string[] keyids, Cancellable cancellable) throws GLib.Error;

    public abstract async List import_async(GLib.InputStream input, Cancellable cancellable) throws GLib.Error;
}

}
