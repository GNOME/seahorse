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

public class Seahorse.AddKeyserverDialog : Gtk.Dialog {

    private string[]? keyserver_types;

    private Gtk.Entry keyserver_host;
    private Gtk.Entry keyserver_port;
    private Gtk.ComboBoxText keyserver_type;
    private Gtk.Box port_block;

    public AddKeyserverDialog(Gtk.Window? parent) {
        GLib.Object(
            title: _("Add Key Server"),
            transient_for: parent,
            modal: true,
            window_position: Gtk.WindowPosition.CENTER_ON_PARENT,
            default_width: 400,
            use_header_bar: 1
        );
        this.keyserver_types = Seahorse.Servers.get_types();

        // Load ui
        Gtk.Builder builder = new Gtk.Builder();
        try {
            string path = "/org/gnome/Seahorse/seahorse-add-keyserver.ui";
            builder.add_from_resource(path);
        } catch (GLib.Error err) {
            GLib.critical("%s", err.message);
        }
        Gtk.Box content = (Gtk.Box) builder.get_object("add-keyserver");
        get_content_area().add(content);
        this.keyserver_host = (Gtk.Entry) builder.get_object("keyserver-host");
        this.keyserver_port = (Gtk.Entry) builder.get_object("keyserver-port");
        this.keyserver_type = (Gtk.ComboBoxText) builder.get_object("keyserver-type");
        this.port_block = (Gtk.Box) builder.get_object("port-block");

        this.keyserver_type.changed.connect(() => on_prefs_add_keyserver_uri_changed());
        this.keyserver_host.changed.connect(() => on_prefs_add_keyserver_uri_changed());
        this.keyserver_port.changed.connect(() => on_prefs_add_keyserver_uri_changed());

        // The description for the key server types, plus custom
        foreach (string type in this.keyserver_types)
            this.keyserver_type.append_text(Seahorse.Servers.get_description(type));

        this.keyserver_type.append_text(_("Custom"));
        this.keyserver_type.set_active(0);

        // Buttons
        add_button(_("Cancel"), Gtk.ResponseType.CANCEL);
        Gtk.Button save_button = (Gtk.Button) add_button(_("Save"), Gtk.ResponseType.OK);
        save_button.get_style_context().add_class("suggested-action");

        show();
    }

    public string? calculate_keyserver_uri() {
        // Figure out the scheme
        string? scheme = null;
        int active = this.keyserver_type.get_active();
        int i;
        if (active >= 0 && this.keyserver_types != null) {
            for (i = 0; this.keyserver_types[i] != null && i < active; i++);
            if (i == active
                    && this.keyserver_types[active] != null
                    && this.keyserver_types[active] != "")
                scheme = this.keyserver_types[active];
        }

        string? host = this.keyserver_host.text;
        if (host == null || host == "")
            return null;

        if (scheme == null) // Custom URI?
            return Servers.is_valid_uri(host)? host : null;

        string? port = this.keyserver_port.text;
        if (port == "")
            port = null;

        // Mash 'em together into a uri
        string? uri = "%s://%s".printf(scheme, host);
        if (port != null)
            uri += ":%s".printf(port);

        // And check if it's valid
        if (!Servers.is_valid_uri(uri))
            uri = null;

        return uri;
    }

    private void on_prefs_add_keyserver_uri_changed() {
        set_response_sensitive(Gtk.ResponseType.OK, calculate_keyserver_uri() != null);

        // Show or hide the port section based on whether 'custom' is selected
        int active = this.keyserver_type.get_active();
        if (active > -1) {
            this.port_block.visible = this.keyserver_types != null
                                      && this.keyserver_types[active] != null
                                      && this.keyserver_types[active] != "";
        }
    }
}
