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

public class Seahorse.Gkr.KeyringProperties : Gtk.Dialog {
    public Keyring keyring { construct; get; }

    private Gtk.Builder _builder;
    private Gtk.HeaderBar header;
    private Gtk.Label name_label;
    private Gtk.Label created_label;
    private Gtk.Image keyring_image;
    private Gtk.Button set_default_button;
    private Gtk.Button change_pw_button;
    private Gtk.Button lock_unlock_button;
    private Gtk.Stack lock_unlock_stack;

    construct {
        this._builder = new Gtk.Builder();
        try {
            string path = "/org/gnome/Seahorse/seahorse-gkr-keyring.ui";
            this._builder.add_from_resource(path);
        } catch (GLib.Error err) {
            critical("%s", err.message);
        }

        var content = (Gtk.Widget)this._builder.get_object("gkr-keyring");
        ((Gtk.Container)this.get_content_area()).add(content);
        content.show();

        // The header
        this.use_header_bar = 1;
        this.header = (Gtk.HeaderBar) this._builder.get_object("titlebar");
        this.keyring.bind_property("label", this.header, "subtitle", GLib.BindingFlags.SYNC_CREATE);
        set_titlebar(this.header);

        // The label
        this.name_label = (Gtk.Label)this._builder.get_object("name_field");
        this.keyring.bind_property ("label", this.name_label, "label", GLib.BindingFlags.SYNC_CREATE);

        // The icon
        this.keyring_image = (Gtk.Image) this._builder.get_object("keyring_image");
        this.keyring.bind_property ("icon", this.keyring_image, "gicon", GLib.BindingFlags.SYNC_CREATE);

        // The date field
        this.created_label = (Gtk.Label)this._builder.get_object("created_field");
        set_created(this.keyring.created);
        this.keyring.notify["created"].connect((obj, pspec) => {
            set_created(this.keyring.created);
        });

        // The buttons
        this.change_pw_button = (Gtk.Button) this._builder.get_object("change_pw_button");
        this.change_pw_button.clicked.connect(on_change_pw_button_clicked);

        this.set_default_button = (Gtk.Button) this._builder.get_object("set_default_button");
        this.set_default_button.clicked.connect(on_set_default_button_clicked);
        this.keyring.bind_property("is-default", this.set_default_button, "sensitive",
                                   BindingFlags.SYNC_CREATE | BindingFlags.INVERT_BOOLEAN);

        this.lock_unlock_button = (Gtk.Button) this._builder.get_object("lock_unlock_button");
        this.lock_unlock_stack = (Gtk.Stack) this._builder.get_object("lock_unlock_stack");
        this.lock_unlock_button.clicked.connect(lock_unlock_button_clicked);
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

    private void on_set_default_button_clicked(Gtk.Button button) {
        this.keyring.on_keyring_default(null);
    }

    private void on_change_pw_button_clicked(Gtk.Button button) {
        this.keyring.on_keyring_password(null);
    }

    private void update_lock_unlock_button() {
        this.lock_unlock_stack.visible_child_name
            = this.keyring.locked? "lock_unlock_button_locked" : "lock_unlock_button_unlocked";
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
                this.keyring.locked? "lock" : "unlock",
                this.keyring.label);
        }

        update_lock_unlock_button();
        this.lock_unlock_button.sensitive = true;
    }

    private void lock_unlock_button_clicked(Gtk.Button button) {
        set_keyring_locked.begin();
    }
}
