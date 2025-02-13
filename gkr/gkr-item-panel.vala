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

[GtkTemplate (ui = "/org/gnome/Seahorse/seahorse-gkr-item-panel.ui")]
public class Seahorse.Gkr.ItemPanel : Seahorse.Panel {

    public Gkr.Item item { construct; get; }

    [GtkChild] private unowned Adw.EntryRow description_field;
    [GtkChild] private unowned Adw.ActionRow use_row;
    [GtkChild] private unowned Adw.ActionRow type_row;
    [GtkChild] private unowned Adw.ActionRow created_row;
    [GtkChild] private unowned Adw.ActionRow modified_row;

    [GtkChild] private unowned Adw.PreferencesGroup details_group;
    private GenericArray<unowned Gtk.Widget> details_rows = new GenericArray<unowned Gtk.Widget>();
    [GtkChild] private unowned Adw.ActionRow server_row;
    [GtkChild] private unowned Adw.ActionRow login_row;
    [GtkChild] private unowned Adw.PasswordEntryRow password_row;
    private string original_password = "";

    private const ActionEntry[] ACTIONS = {
        { "delete-secret", action_delete_secret },
        { "copy-secret", action_copy_secret },
    };

    construct {
        this.actions.add_action_entries(ACTIONS, this);
        insert_action_group("panel", this.actions);

        // Setup the label properly
        this.item.bind_property("label", this.description_field, "text",
                                GLib.BindingFlags.SYNC_CREATE);

        // Window title
        this.item.bind_property("label", this, "subtitle",
                                GLib.BindingFlags.SYNC_CREATE);

        // Created/Modified
        var created = new DateTime.from_unix_utc((int64) this.item.get_created());
        this.created_row.subtitle = created.to_local().format("%c");
        update_use();
        update_type();
        update_visibility();
        update_details();
        update_server();
        update_user();
        update_modified();

        // Update as appropriate
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
            update_modified();
        });

        // fill the password entry
        fetch_password();

        // Sensitivity of the password entry
        this.item.bind_property("has-secret", this.password_row, "sensitive");

        item.refresh();
    }

    public ItemPanel(Item item) {
        GLib.Object(item: item);
    }

    public override void dispose() {
        unowned Gtk.Widget? child = null;
        while ((child = get_first_child()) != null)
            child.unparent();
        base.dispose();
    }

    private void update_modified() {
        var modified = new DateTime.from_unix_utc((int64) this.item.get_modified());
        this.modified_row.subtitle = modified.to_local().format("%c");
    }

    private void update_use() {
        switch (this.item.use) {
        case Use.NETWORK:
            this.use_row.subtitle = _("Access a network share or resource");
            break;
        case Use.WEB:
            this.use_row.subtitle = _("Access a website");
            break;
        case Use.PGP:
            this.use_row.subtitle = _("Unlocks a PGP key");
            break;
        case Use.SSH:
            this.use_row.subtitle = _("Unlocks a Secure Shell key");
            break;
        case Use.OTHER:
            this.use_row.subtitle = _("Saved password or login");
            break;
        default:
            this.use_row.subtitle = "";
            break;
        };
    }

    private void update_type() {
        switch (this.item.use) {
        case Use.NETWORK:
        case Use.WEB:
            this.type_row.subtitle = _("Network Credentials");
            break;
        case Use.PGP:
        case Use.SSH:
        case Use.OTHER:
            this.type_row.subtitle = _("Password");
            break;
        default:
            this.type_row.subtitle = "";
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
        this.server_row.subtitle = value;
    }

    private void update_user() {
        var value = this.item.get_attribute("user");
        if (value == null)
            value = "";
        this.login_row.subtitle = value;
    }

    private void update_details() {
        var attrs = this.item.attributes;
        var iter = GLib.HashTableIter<string, string>(attrs);

        foreach (unowned var row in this.details_rows)
            this.details_group.remove(row);
        this.details_rows.remove_range(0, this.details_rows.length);

        bool any_details = false;
        string key, value;
        while (iter.next(out key, out value)) {
            if (key.has_prefix("gkr:") || key.has_prefix("xdg:"))
                continue;

            any_details = true;

            var row = new Adw.ActionRow();
            row.title = key;
            row.subtitle = value;
            row.subtitle_selectable = true;
            row.add_css_class("property");

            this.details_rows.add(row);
            this.details_group.add(row);
        }

        this.details_group.visible = any_details;
    }

    private async void save_password() {
        var pw = new Secret.Value(this.password_row.text, -1, "text/plain");
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
            this.original_password = secret.get_text() ?? "";
            this.password_row.text = this.original_password;
        }
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
    private void on_description_field_apply(Adw.EntryRow row) {
        save_description.begin();
    }

    [GtkCallback]
    private void on_password_row_apply(Adw.EntryRow row) {
        save_password.begin();
    }

    private void action_copy_secret(SimpleAction action, Variant? param) {
        this.item.copy_secret_to_clipboard.begin(get_clipboard());
        notification(new Adw.Toast(_("Copied secret to clipboard")));
    }

    private void action_delete_secret(SimpleAction action, Variant? param) {
        unowned var toplevel = get_root() as Gtk.Window;

        var delete_op = this.item.create_delete_operation();
        delete_op.execute_interactively.begin(toplevel, null, (obj, res) => {
            try {
                delete_op.execute_interactively.end(res);
            } catch (GLib.IOError.CANCELLED e) {
                debug("Deletion of secret cancelled by user");
            } catch (GLib.Error e) {
                Util.show_error(toplevel, _("Couldn’t delete secret"), e.message);
            }
        });
    }

    [GtkCallback]
    private bool string_is_not_empty(string val) {
        return (val != null) && (val != "");
    }
}
