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

    public AppSettings() {
        GLib.Object(schema_id: "org.gnome.seahorse");
    }

    private static AppSettings? _instance = null;
    public static AppSettings instance() {
        if (_instance == null)
            _instance = new AppSettings();
        return _instance;
    }
}
