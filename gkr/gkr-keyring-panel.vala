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

[GtkTemplate (ui = "/org/gnome/Seahorse/seahorse-gkr-keyring-panel.ui")]
public class Seahorse.Gkr.KeyringPanel : Seahorse.Panel {

    public Keyring keyring { construct; get; }

    [GtkChild] private unowned Adw.ActionRow name_row;
    [GtkChild] private unowned Adw.ActionRow created_row;

    [GtkChild] private unowned Gtk.LockButton lock_button;

    private const ActionEntry[] ACTIONS = {
        { "set-default", action_set_default },
        { "change-password", action_change_password },
    };

    construct {
        this.actions.add_action_entries(ACTIONS, this);
        insert_action_group("panel", this.actions);

        this.keyring.bind_property("label", this, "subtitle", GLib.BindingFlags.SYNC_CREATE);
        this.keyring.bind_property("label", this.name_row, "subtitle", GLib.BindingFlags.SYNC_CREATE);

        // The date field
        set_created(this.keyring.created);
        this.keyring.notify["created"].connect((obj, pspec) => {
            set_created(this.keyring.created);
        });

        // The buttons
        this.keyring.bind_property("is-default",
                                   this.actions.lookup_action("set-default"), "enabled",
                                   BindingFlags.SYNC_CREATE | BindingFlags.INVERT_BOOLEAN);
    }

    public KeyringPanel(Keyring keyring) {
        GLib.Object(keyring: keyring);

        var perm = new KeyringPermission(this.keyring);
        this.lock_button.set_permission(perm);
    }

    public override void dispose() {
        unowned Gtk.Widget? child = null;
        while ((child = get_first_child()) != null)
            child.unparent();
        base.dispose();
    }

    private void set_created(uint64 timestamp) {
        if (timestamp == 0) {
            this.created_row.subtitle = _("Unknown date");
            return;
        }

        var datetime = new DateTime.from_unix_utc((int64) timestamp);
        this.created_row.subtitle = datetime.format("%x");
    }

    private void action_set_default(SimpleAction action, Variant? param) {
        this.keyring.set_as_default();
    }

    private void action_change_password(SimpleAction action, Variant? param) {
        this.keyring.change_password();
    }
}
