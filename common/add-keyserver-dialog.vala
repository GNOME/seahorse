/*
 * Seahorse
 *
 * Copyright (C) 2004-2005 Stefan Walter
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

[GtkTemplate (ui = "/org/gnome/Seahorse/seahorse-add-keyserver.ui")]
public class Seahorse.AddKeyserverDialog : Gtk.ApplicationWindow {

    [GtkChild] private unowned Adw.ComboRow type_row;
    // For now we base the model on the string description and keep the schemes
    // in a synced list of strings. Probably should move this to ServerCategorys
    private string?[] server_types;
    [GtkChild] private unowned Gtk.StringList types_model;

    [GtkChild] private unowned Adw.EntryRow url_row;

    private const GLib.ActionEntry[] action_entries = {
         { "save", action_save },
    };

    construct {
        add_action_entries(action_entries, this);

        string[] types = {};
        foreach (unowned string type in ServerCategory.get_types())
            types += type;
        types += ""; // Special case the "Custom" key server
        this.server_types = types;

        // The description for the key server types, plus the custom type
        foreach (string type in this.server_types) {
            if (type == "") {
                this.types_model.append(_("Custom"));
                continue;
            }
            unowned var category = ServerCategory.find_category(type);
            this.types_model.append(category.description);
        }

        // Connect signals
        this.type_row.notify["selected"].connect((obj, pspec) => validate_input());
        this.url_row.changed.connect(() => validate_input());
    }

    public AddKeyserverDialog(Gtk.Window? parent) {
        GLib.Object(transient_for: parent);
    }

    public string? calculate_keyserver_uri() {
        var url = this.url_row.text;
        if (url == null || url == "")
            return null;

        var selected = this.type_row.selected;
        var scheme = this.server_types[selected];

        if (scheme == "") // Custom URI?
            return ServerCategory.is_valid_uri(url)? url : null;

        // Mash it together with the scheme
        var full_url = "%s://%s".printf(scheme, url);

        // And check if it's valid
        if (!ServerCategory.is_valid_uri(full_url))
            return null;

        return full_url;
    }

    private void validate_input() {
        bool valid = (calculate_keyserver_uri() != null);
        ((SimpleAction) lookup_action("save")).set_enabled (valid);
    }

    private void action_save(SimpleAction action, Variant? param) {
        string? result = calculate_keyserver_uri();
        if (result == null) {
            warning("Got invalid URL");
            return;
        }

        Pgp.Backend.get().add_remote(result, true);
    }
}
