/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

[GtkTemplate (ui = "/org/gnome/Seahorse/seahorse-gkr-keyring.ui")]
public class Seahorse.Gkr.KeyringProperties : Gtk.Dialog {
    public Keyring keyring { construct; get; }

    [GtkChild]
    private unowned Gtk.HeaderBar header;
    [GtkChild]
    private unowned Gtk.Label name_label;
    [GtkChild]
    private unowned Gtk.Label created_label;
    [GtkChild]
    private unowned Gtk.Image keyring_image;
    [GtkChild]
    private unowned Gtk.Button set_default_button;
    [GtkChild]
    private unowned Gtk.LockButton lock_button;

    construct {
        this.use_header_bar = 1;

        this.keyring.bind_property("label", this.header, "subtitle", GLib.BindingFlags.SYNC_CREATE);
        this.keyring.bind_property("label", this.name_label, "label", GLib.BindingFlags.SYNC_CREATE);
        this.keyring.bind_property("icon", this.keyring_image, "gicon", GLib.BindingFlags.SYNC_CREATE);

        // The date field
        set_created(this.keyring.created);
        this.keyring.notify["created"].connect((obj, pspec) => {
            set_created(this.keyring.created);
        });

        // The buttons
        this.keyring.bind_property("is-default", this.set_default_button, "sensitive",
                                   BindingFlags.SYNC_CREATE | BindingFlags.INVERT_BOOLEAN);
    }

    public KeyringProperties(Keyring keyring, Gtk.Window? parent) {
        GLib.Object(
            keyring: keyring,
            transient_for: parent
        );
        var perm = new KeyringPermission(this.keyring);
        this.lock_button.set_permission(perm);
    }

    private void set_created(uint64 timestamp) {
        if (timestamp == 0) {
            this.created_label.label = _("Unknown date");
            return;
        }

        var datetime = new DateTime.from_unix_utc((int64) timestamp);
        this.created_label.label = datetime.format("%x");
    }

    [GtkCallback]
    private void on_set_default_button_clicked(Gtk.Button button) {
        this.keyring.set_as_default();
    }

    [GtkCallback]
    private void on_change_pw_button_clicked(Gtk.Button button) {
        this.keyring.change_password();
    }
}
