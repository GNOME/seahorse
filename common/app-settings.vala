/*
 * Seahorse
 *
 * Copyright (C) 2018 Niels De Graef
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

public class Seahorse.AppSettings : GLib.Settings {

    public string default_key {
        owned get { return get_string("default-key"); }
        set { set_string("default-key", value); }
    }

    /**
     * Returns a list of keyservers. Each entry is of either the format "uri"
     * or "uri name", where `name` is a display name of the given keyserver.
     */
    public string[] keyservers {
        owned get { return get_strv("keyservers"); }
        set { set_strv("keyservers", value); }
    }

    [CCode (array_null_terminated = true, array_length = false)]
    public string[] get_uris() {
        string[] uris = {};
        foreach (unowned string server in this.keyservers)
            uris += get_uri_for_keyserver_entry (server);
        return uris;
    }

    [CCode (array_null_terminated = true, array_length = false)]
    public string[] last_search_servers {
        owned get { return get_strv("last-search-servers"); }
        set { set_strv("last-search-servers", value); }
    }

    public string last_search_text {
        owned get { return get_string("last-search-text"); }
        set { set_string("last-search-text", value); }
    }

    public bool server_auto_publish {
        get { return get_boolean("server-auto-publish"); }
        set { set_boolean("server-auto-publish", value); }
    }

    public bool server_auto_retrieve {
        get { return get_boolean("server-auto-retrieve"); }
        set { set_boolean("server-auto-retrieve", value); }
    }

    public string server_publish_to {
        owned get { return get_string("server-publish-to"); }
        set {
            if (this.server_publish_to == value)
                return;
            set_string("server-publish-to", value);
        }
    }

    public void add_keyserver(string uri, string? name) {
        string[] servers = {};

        if (uri in this.keyservers)
            return;

        debug("Adding key server URL '%s'", uri);
        foreach (unowned string keyserver in this.keyservers)
            servers += keyserver;

        if (name != null)
            servers += "%s %s".printf(uri, name);
        else
            servers += uri;
        servers += null;

        this.keyservers = servers;
    }

    public void remove_keyserver(string uri) {
        string[] servers = {};

        if (!(uri in this.keyservers))
            return;

        debug("Removing key server URL '%s'", uri);
        foreach (unowned string keyserver in this.keyservers)
            if (get_uri_for_keyserver_entry(keyserver) != uri)
                servers += keyserver;
        servers += null;

        this.keyservers = servers;
    }

    public AppSettings() {
        GLib.Object(schema_id: "org.gnome.seahorse");

        // Make sure ServerCategories are known before people try to read them
        ServerCategory.init();
    }

    private static AppSettings? _instance = null;
    public static AppSettings instance() {
        if (_instance == null)
            _instance = new AppSettings();
        return _instance;
    }

    private string? get_uri_for_keyserver_entry(string keyserver) {
        // The values are "uri" or "uri name", so remove the name part (if any)
        return keyserver.strip().split(" ", 2)[0];
    }
}
