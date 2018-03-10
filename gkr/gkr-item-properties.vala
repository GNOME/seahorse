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

public class Seahorse.Gkr.ItemProperties : Gtk.Dialog {
    public Item item { construct; get; }

    private Gtk.Builder _builder;
    private PasswordEntry password_entry;
    private Gtk.Entry description;
    private bool description_has_changed;

    construct {
        this._builder = new Gtk.Builder();
        try {
            string path = "/org/gnome/Seahorse/seahorse-gkr-item-properties.ui";
            this._builder.add_from_resource(path);
        } catch (GLib.Error err) {
            GLib.critical ("%s", err.message);
        }

        var content = (Gtk.Widget)this._builder.get_object("gkr-item-properties");
        ((Gtk.Container)this.get_content_area()).add(content);
        content.show();

        // Setup the image properly
        this.item.bind_property ("icon", this._builder.get_object("key-image"), "gicon",
                                 GLib.BindingFlags.SYNC_CREATE);

        // Setup the label properly
        this.description = (Gtk.Entry)this._builder.get_object("description-field");
        this.item.bind_property("label", this.description, "text", GLib.BindingFlags.SYNC_CREATE);
        this.description.changed.connect(() => {
            this.description_has_changed = true;
        });

        /* Window title */
        this.item.bind_property("label", this, "title", GLib.BindingFlags.SYNC_CREATE);

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
        Gtk.Box box = (Gtk.Box)this._builder.get_object("password-box-area");
        box.add(this.password_entry);
        fetch_password();

        // Sensitivity of the password entry
        this.item.bind_property("has-secret", this.password_entry, "sensitive");
    }

    public ItemProperties(Item item, Gtk.Window? parent) {
        GLib.Object (
            item: item,
            transient_for: parent
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
        Gtk.Label use = (Gtk.Label)this._builder.get_object("use-field");
        switch (this.item.use) {
        case Use.NETWORK:
            use.label = _("Access a network share or resource");
            break;
        case Use.WEB:
            use.label = _("Access a website");
            break;
        case Use.PGP:
            use.label = _("Unlocks a PGP key");
            break;
        case Use.SSH:
            use.label = _("Unlocks a Secure Shell key");
            break;
        case Use.OTHER:
            use.label = _("Saved password or login");
            break;
        default:
            use.label = "";
            break;
        };
    }

    private void update_type() {
        Gtk.Label type = (Gtk.Label)this._builder.get_object("type-field");
        switch (item.use) {
        case Use.NETWORK:
        case Use.WEB:
            type.label = _("Network Credentials");
            break;
        case Use.PGP:
        case Use.SSH:
        case Use.OTHER:
            type.label = _("Password");
            break;
        default:
            type.label = "";
            break;
        };
    }

    private void update_visibility() {
        var use = this.item.use;
        bool visible = use == Use.NETWORK || use == Use.WEB;
        this._builder.get_object("server-label").set("visible", visible);
        this._builder.get_object("server-field").set("visible", visible);
        this._builder.get_object("login-label").set("visible", visible);
        this._builder.get_object("login-field").set("visible", visible);
    }

    private void update_server() {
        Gtk.Label server = (Gtk.Label)this._builder.get_object("server-label");
        var value = this.item.get_attribute("server");
        if (value == null)
            value = "";
        server.label = value;
    }

    private void update_user() {
        Gtk.Label login = (Gtk.Label)this._builder.get_object("login-label");
        var value = this.item.get_attribute("user");
        if (value == null)
            value = "";
        login.label = value;
    }

    private void update_details() {
        var contents = new GLib.StringBuilder();
        var attrs = this.item.attributes;
        var iter = GLib.HashTableIter<string, string>(attrs);
        string key, value;
        while (iter.next(out key, out value)) {
            if (key.has_prefix("gkr:") || key.has_prefix("xdg:"))
                continue;
            contents.append_printf("<b>%s</b>: %s\n",
                                   GLib.Markup.escape_text(key),
                                   GLib.Markup.escape_text(value));
        }
        Gtk.Label details = (Gtk.Label)this._builder.get_object("details-box");
        details.use_markup = true;
        details.label = contents.str;
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
            yield this.item.set_label(this.description.text, null);
        } catch (GLib.Error err) {
            this.description.text = this.item.label;
            DBusError.strip_remote_error(err);
            Util.show_error (this, _("Couldn’t set description."), err.message);
        }
    }
}
