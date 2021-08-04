/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2018 Niels De Graef
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

[GtkTemplate (ui = "/org/gnome/Seahorse/seahorse-gkr-item-properties.ui")]
public class Seahorse.Gkr.ItemProperties : Gtk.Dialog {
    public Item item { construct; get; }

    [GtkChild]
    private unowned Gtk.Entry description_field;
    private bool description_has_changed;
    [GtkChild]
    private unowned Gtk.Label use_field;
    [GtkChild]
    private unowned Gtk.Label type_field;
    [GtkChild]
    private unowned Hdy.PreferencesGroup details_group;
    [GtkChild]
    private unowned Gtk.ListBox details_box;
    [GtkChild]
    private unowned Hdy.ActionRow server_row;
    [GtkChild]
    private unowned Gtk.Label server_field;
    [GtkChild]
    private unowned Hdy.ActionRow login_row;
    [GtkChild]
    private unowned Gtk.Label login_field;
    [GtkChild]
    private unowned Gtk.Box password_box_area;
    private PasswordEntry password_entry;

    construct {
        // Setup the label properly
        this.item.bind_property("label", this.description_field, "text",
                                GLib.BindingFlags.SYNC_CREATE);
        this.description_field.changed.connect(() => {
            this.description_has_changed = true;
        });

        /* Window title */
        var headerbar = (Gtk.HeaderBar) this.get_header_bar();
        this.item.bind_property("label", headerbar, "subtitle",
                                GLib.BindingFlags.SYNC_CREATE);

        /* Update as appropriate */
        this.item.notify.connect((pspec) => {
            switch(pspec.name) {
            case "use":
                update_use();
                update_type();
                update_visibility();
                break;
            case "attributes":
                update_details();
                update_server();
                update_user();
                break;
            case "has-secret":
                fetch_password();
                break;
            }
        });

        // Create the password entry
        this.password_entry = new PasswordEntry();
        this.password_box_area.pack_start(this.password_entry, true, true, 0);
        fetch_password();

        // Sensitivity of the password entry
        this.item.bind_property("has-secret", this.password_entry, "sensitive");
    }

    public ItemProperties(Item item, Gtk.Window? parent) {
        GLib.Object (
            item: item,
            transient_for: parent,
            use_header_bar: 1
        );
        item.refresh();
    }

    public override void response(int response) {
        // In case of changes: ask for confirmation
        if (!this.password_entry.has_changed && !this.description_has_changed) {
            destroy();
            return;
        }

        var dialog = new Gtk.MessageDialog(this, Gtk.DialogFlags.MODAL, Gtk.MessageType.WARNING,
                                           Gtk.ButtonsType.OK_CANCEL, _("Save changes for this item?"));
        dialog.response.connect((resp) => {
            if (resp == Gtk.ResponseType.OK) {
                if (this.password_entry.has_changed)
                    save_password.begin();
                if (this.description_has_changed)
                    save_description.begin();
            }

            dialog.destroy();
        });
        dialog.run();
    }

    private void update_use() {
        switch (this.item.use) {
        case Use.NETWORK:
            this.use_field.label = _("Access a network share or resource");
            break;
        case Use.WEB:
            this.use_field.label = _("Access a website");
            break;
        case Use.PGP:
            this.use_field.label = _("Unlocks a PGP key");
            break;
        case Use.SSH:
            this.use_field.label = _("Unlocks a Secure Shell key");
            break;
        case Use.OTHER:
            this.use_field.label = _("Saved password or login");
            break;
        default:
            this.use_field.label = "";
            break;
        };
    }

    private void update_type() {
        switch (this.item.use) {
        case Use.NETWORK:
        case Use.WEB:
            this.type_field.label = _("Network Credentials");
            break;
        case Use.PGP:
        case Use.SSH:
        case Use.OTHER:
            this.type_field.label = _("Password");
            break;
        default:
            this.type_field.label = "";
            break;
        };
    }

    private void update_visibility() {
        var use = this.item.use;
        this.server_row.visible =
            this.login_row.visible = (use == Use.NETWORK || use == Use.WEB);
    }

    private void update_server() {
        var value = this.item.get_attribute("server");
        if (value == null)
            value = "";
        this.server_field.label = value;
    }

    private void update_user() {
        var value = this.item.get_attribute("user");
        if (value == null)
            value = "";
        this.login_field.label = value;
    }

    private void update_details() {
        var attrs = this.item.attributes;
        var iter = GLib.HashTableIter<string, string>(attrs);

        bool any_details = false;
        string key, value;
        while (iter.next(out key, out value)) {
            if (key.has_prefix("gkr:") || key.has_prefix("xdg:"))
                continue;

            any_details = true;

            var row = new Hdy.ActionRow();
            row.title = key;
            row.can_focus = false;

            var label = new Gtk.Label (value);
            label.xalign = 1;
            label.selectable = true;
            label.wrap = true;
            label.wrap_mode = Pango.WrapMode.WORD_CHAR;
            label.max_width_chars = 32;
            row.add(label);

            row.show_all();
            this.details_box.insert(row, -1);
        }

        this.details_group.visible = any_details;
    }

    private async void save_password() {
        var pw = new Secret.Value(this.password_entry.text, -1, "text/plain");
        try {
            yield this.item.set_secret(pw, null);
        } catch (GLib.Error err) {
            DBusError.strip_remote_error(err);
            Util.show_error (this, _("Couldn’t change password."), err.message);
        }
        fetch_password();
    }

    private void fetch_password() {
        var secret = this.item.get_secret();
        if (secret != null) {
            unowned string? password = secret.get_text();
            if (password != null) {
                this.password_entry.set_initial_password(password);
                return;
            }
        }

        this.password_entry.set_initial_password("");
    }

    private async void save_description() {
        try {
            yield this.item.set_label(this.description_field.text, null);
        } catch (GLib.Error err) {
            this.description_field.text = this.item.label;
            DBusError.strip_remote_error(err);
            Util.show_error (this, _("Couldn’t set description."), err.message);
        }
    }

    [GtkCallback]
    private void on_copy_button_clicked() {
        var clipboard = Gtk.Clipboard.get_default(this.get_display());
        this.item.copy_secret_to_clipboard.begin(clipboard);
    }

    [GtkCallback]
    private void on_delete_button_clicked() {
        var deleter = this.item.create_deleter();
        var ret = deleter.prompt(this);

        if (!ret)
            return;

        deleter.delete.begin(null, (obj, res) => {
            try {
                deleter.delete.end(res);
                this.destroy();
            } catch (GLib.Error e) {
                var dialog = new Gtk.MessageDialog(this,
                    Gtk.DialogFlags.MODAL,
                    Gtk.MessageType.ERROR,
                    Gtk.ButtonsType.OK,
                    _("Error deleting the password."));
                dialog.run();
                dialog.destroy();
            }
        });
    }
}
