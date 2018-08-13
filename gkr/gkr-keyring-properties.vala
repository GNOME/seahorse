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
    private Gtk.HeaderBar header;
    [GtkChild]
    private Gtk.Label name_label;
    [GtkChild]
    private Gtk.Label created_label;
    [GtkChild]
    private Gtk.Image keyring_image;
    [GtkChild]
    private Gtk.Button set_default_button;
    [GtkChild]
    private Gtk.Button lock_unlock_button;
    [GtkChild]
    private Gtk.Stack lock_unlock_stack;

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
        update_lock_unlock_button();
    }

    public KeyringProperties(Keyring keyring, Gtk.Window? parent) {
        GLib.Object(
            keyring: keyring,
            transient_for: parent
        );
    }

    private void set_created(uint64 timestamp) {
        this.created_label.label = (timestamp != 0)? Util.get_display_date_string((long) timestamp)
                                                   : _("Unknown date");
    }

    [GtkCallback]
    private void on_set_default_button_clicked(Gtk.Button button) {
        this.keyring.on_keyring_default(null);
    }

    [GtkCallback]
    private void on_change_pw_button_clicked(Gtk.Button button) {
        this.keyring.on_keyring_password(null);
    }

    private void update_lock_unlock_button() {
        this.lock_unlock_stack.visible_child_name
            = this.keyring.locked? "lock_unlock_button_unlocked" : "lock_unlock_button_locked";
    }

    private async void set_keyring_locked() {
        this.lock_unlock_button.sensitive = false;
        try {
            if (this.keyring.locked)
                yield this.keyring.unlock(null, null);
            else
                yield this.keyring.lock(null, null);
        } catch (Error e) {
            warning("Couldn't %s keyring <%s>",
                this.keyring.locked? "unlock" : "lock",
                this.keyring.label);
        }

        update_lock_unlock_button();
        this.lock_unlock_button.sensitive = true;
    }

    [GtkCallback]
    private void on_lock_unlock_button_clicked(Gtk.Button button) {
        set_keyring_locked.begin();
    }
}
