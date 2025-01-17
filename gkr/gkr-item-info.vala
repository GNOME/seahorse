/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2020 Niels De Graef
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

namespace Seahorse.Gkr {

/**
 * The generic base class that holds extra info for a {@link Gkr.Item}.
 */
public class ItemInfo : GLib.Object {

    public string label { get; protected set; default = ""; }

    public string description { get; protected set; default = _("Password or Secret"); }

    public string details { get; protected set; default = ""; }

    public GLib.Icon? icon { get; protected set; default = new GLib.ThemedIcon("secret-item-symbolic"); }

    public ItemInfo(string? label,
                    HashTable<string, string>? attrs,
                    string? description = null) {
        GLib.Object(label: label);

        if (label != null)
            this.label = label;

        if (description != null)
            this.description = description;

        this.details = guess_username(attrs);
    }

    private unowned string? guess_username(HashTable<string, string>? attrs) {
        unowned string? username;

        if (attrs == null || this.details != "")
            return this.details;

        username = attrs.lookup("username");
        if (username != null && username != "")
            return username;

        username = attrs.lookup("user");
        if (username != null && username != "")
            return username;

        // Used by e.g. fractal
        username = attrs.lookup("uid");
        if (username != null && username != "")
            return username;

        // Used by e.g. Geary
        username = attrs.lookup("login");
        if (username != null && username != "")
            return username;

        // Used by e.g. Skype
        username = attrs.lookup("account");
        if (username != null && username != "")
            return username;

        return null;
    }

    public void query_installed_apps(string item_type) {
        DesktopAppInfo? app_info = new DesktopAppInfo("%s.desktop".printf(item_type));
        if (app_info == null)
            return;

        this.icon = app_info.get_icon();
    }

    // Hlper methods to get attributes
    protected unowned string? get_attribute_string(HashTable<string, string>? attrs,
                                                   string name) {
        if (attrs == null)
            return null;
        return attrs.lookup(name);
    }

    protected int get_attribute_int(HashTable<string, string>? attrs,
                                    string name) {
        if (attrs == null)
            return 0;

        var value = attrs.lookup(name);
        if (value != null)
            return int.parse(value);

        return 0;
    }
}

/**
 * Used for NetworkManager connections, with xdg:schema
 * "org.freedesktop.NetworkManager.Connection"
 */
public class NmConnectionInfo : ItemInfo {

    public NmConnectionInfo(string label, HashTable<string, string>? attrs) {
        base(label, attrs, _("Network connection secret"));

        this.icon = new ThemedIcon("network-wired-symbolic");

        unowned string? setting_name = get_attribute_string(attrs,
                                                            "setting-name");

        if (setting_name == "802-11-wireless-security") {
            this.icon = new ThemedIcon("network-wireless-symbolic");
            this.description = _("Wi-Fi password");
        }

        // TODO: are we sure this isn't translated by NM?
        if (label.has_prefix("Network secret for ")) {
            this.label = this.label.substring("Network secret for ".length);
        }
    }
}

/**
 * Used for entries in gnome-keyring with xdg:schema
 * "org.gnome.keyring.NetworkPassword"
 */
public class GkrNetworkPassInfo : ItemInfo {

    public GkrNetworkPassInfo(string label, HashTable<string, string>? attrs) {
        base(label, attrs, _("Network password"));

        this.icon = new ThemedIcon("network-server-symbolic");

        unowned string? server = get_attribute_string(attrs, "server");
        unowned string? protocol = get_attribute_string(attrs, "protocol");
        unowned string? object = get_attribute_string(attrs, "object");
        unowned string? user = get_attribute_string(attrs, "user");
        var port = get_attribute_int (attrs, "port");

        if (protocol == null)
            return;

        /* If it's customized by the application or user then display that */
        if (is_custom_network_label (server, user, object, port, label))
            this.label = label;
        else
            this.label = calc_network_label (attrs, true);

        unowned string symbol = "@";
        if (user == null) {
            user = "";
            symbol = "";
        }

        if (object == null)
            object = "";

        if (server != null && protocol != null) {
            var full_url = GLib.Markup.printf_escaped("%s://%s%s%s/%s",
                                                      protocol, user, symbol,
                                                      server, object);
            if (this.label == null || this.label == "")
                this.label = full_url;
            else
                this.details = full_url;
        }
    }

    /* For network passwords gnome-keyring generates in a funky looking display
     * name that's generated from login credentials. We simulate that generating
     * here and return FALSE if it matches. This allows us to display user
     * customized display names and ignore the generated ones. */
    private bool is_custom_network_label(string? server,
                                         string? user,
                                         string? object,
                                         int port,
                                         string? label) {

        if (server == null)
            return true;

        var generated = new GLib.StringBuilder("");
        if (user != null)
            generated.append_printf("%s@", user);
        generated.append(server);
        if (port != 0)
            generated.append_printf(":%d", port);
        if (object != null)
            generated.append_printf ("/%s", object);

        return (generated.str != label);
    }

    private string? calc_network_label(HashTable<string, string>? attrs,
                                       bool always) {
        /* HTTP usually has a the realm as the "object" display that */
        if (is_network_item (attrs, "http") && attrs != null) {
            unowned string? val = get_attribute_string(attrs, "object");
            if (val != null && val != "")
                return val;

            /* Display the server name as a last resort */
            if (always) {
                val = get_attribute_string(attrs, "server");
                if (val != null && val != "")
                    return val;
            }
        }

        return null;
    }

    private bool is_network_item(HashTable<string, string>? attrs,
                                 string match) {
        unowned string? protocol = get_attribute_string(attrs, "protocol");
        return protocol != null && protocol.ascii_casecmp(match) == 0;
    }
}

/** Used for Evolution-Data-Server (E-D-S) passwordss */
public class EdsPassInfo : ItemInfo {

    public EdsPassInfo(string label, HashTable<string, string>? attrs) {
        base(label, attrs, null);

        // Set the Epiphany icon
        query_installed_apps("org.gnome.Evolution");

        unowned string? uri = get_attribute_string(attrs, "e-source-uid");
        if (uri != null)
            this.details = uri;
    }
}

/** Used for GNOME Web (epiphany) passwordss */
public class EpiphanyPassInfo : ItemInfo {

    public EpiphanyPassInfo(string label, HashTable<string, string>? attrs) {
        base(label, attrs, _("GNOME Web password"));

        // Set the Epiphany icon
        query_installed_apps("org.gnome.Epiphany");

        unowned string? uri = get_attribute_string(attrs, "uri");
        if (uri != null)
            this.label = uri;

        unowned string? username = get_attribute_string(attrs, "username");
        if (username != null)
            this.details = Markup.escape_text(username);
    }
}

/** Used for passwords saved in Chrome/Chromium */
public class ChromePassInfo : ItemInfo {

    public ChromePassInfo(string label, HashTable<string, string>? attrs) {
        base(label, attrs, _("Google Chrome password"));


        unowned string? origin = get_attribute_string(attrs, "origin_url");
        /* A brain dead url, parse */
        if (label != null && label == origin) {
            try {
                var regex = new GLib.Regex("[a-z]+://([^/]+)/", GLib.RegexCompileFlags.CASELESS, 0);
                GLib.MatchInfo match;
                if (regex.match(label, GLib.RegexMatchFlags.ANCHORED, out match)) {
                    if (match.matches())
                        this.label = match.fetch(1);
                }
            } catch (GLib.RegexError err) {
                GLib.critical("%s", err.message);
                return;
            }

        }

        if (this.label == "")
            this.label = label;
        if (origin != null)
            this.details = GLib.Markup.escape_text(origin);
        else
            this.details = "";
    }
}

/** Used for Empathy */
public class EmpathyInfo : ItemInfo {

    public EmpathyInfo(string label, HashTable<string, string>? attrs) {
        base(label, attrs, _("Instant messaging password"));

        unowned string? account = get_attribute_string(attrs, "account-id");

        /* Translators: This should be the same as the string in empathy */
        unowned string prefix = _("IM account password for ");

        if (label != null && label.has_prefix(prefix)) {
            var len = prefix.length;
            var pos = label.index_of_char ('(', (int)len);
            if (pos != -1)
                this.label = label.slice(len, pos);

            try {
                var regex = new GLib.Regex("^.+/.+/(.+)$", GLib.RegexCompileFlags.CASELESS, 0);
                GLib.MatchInfo match;
                if (regex.match(account, GLib.RegexMatchFlags.ANCHORED, out match)) {
                    if (match.matches())
                        this.details = TelepathyInfo.decode_id(match.fetch(1));
                }
            } catch (GLib.RegexError err) {
                GLib.critical("%s", err.message);
                return;
            }
        }

        if (this.label == "")
            this.label = label;
        if (this.details == null)
            this.details = GLib.Markup.escape_text (account);
    }
}

/** Used for Telepathy */
public class TelepathyInfo : ItemInfo {

    public TelepathyInfo(string label, HashTable<string, string>? attrs) {
        base(label, attrs, _("Telepathy password"));

        unowned string? account = get_attribute_string (attrs, "account");
        if (account != null && label != null && label.index_of(account) != -1) {
            try {
                var regex = new GLib.Regex("^.+/.+/(.+)$", GLib.RegexCompileFlags.CASELESS, 0);
                GLib.MatchInfo match;
                if (regex.match(account, GLib.RegexMatchFlags.ANCHORED, out match)) {
                    if (match.matches()) {
                        var identifier = match.fetch(1);
                        this.label = decode_id(identifier);
                    }
                }
            } catch (GLib.RegexError err) {
                GLib.critical("%s", err.message);
                return;
            }
        }

        if (this.label == "")
            this.label = label;
        if (account != null)
            this.details = GLib.Markup.escape_text(account);
    }

    public static string? decode_id(string account) {
        account.replace("_", "%");
        return GLib.Uri.unescape_string(account);
    }
}

/** Used for GNOME Online Accounts */
public class GoaInfo : ItemInfo {

    public GoaInfo(string label, HashTable<string, string>? attrs) {
        base(label, attrs, _("GNOME Online Accounts password"));
    }
}
}
