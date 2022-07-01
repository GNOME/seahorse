/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

[GtkTemplate (ui = "/org/gnome/Seahorse/gkr-add-keyring-dialog.ui")]
public class Seahorse.Gkr.AddKeyringDialog : Adw.Dialog {

    [GtkChild] private unowned Adw.EntryRow name_row;

    // Fired if an item was succesffully added
    public signal void keyring_added(Secret.Collection keyring);

    static construct {
        install_action("add-keyring", null, (Gtk.WidgetActionActivateFunc) action_add_keyring);
    }

    construct {
        action_set_enabled("add-keyring", false);

        this.name_row.grab_focus();
    }

    public AddKeyringDialog() {
    }

    private void action_add_keyring(string action_name, Variant? param) {
        var cancellable = Dialog.begin_request(this);
        var service = Backend.instance().service;
        Secret.Collection.create.begin(service, this.name_row.text, null, 0,
                                       cancellable, (obj, res) => {
            /* Clear the operation without cancelling it since it is complete */
            Dialog.complete_request(this, false);

            try {
                var keyring = Secret.Collection.create.end(res);
                keyring_added(keyring);
            } catch (GLib.Error err) {
                Util.show_error(this, _("Couldn’t add keyring"), err.message);
            } finally {
                close();
            }
        });
    }

    [GtkCallback]
    private void on_name_row_changed(Gtk.Editable editable) {
        action_set_enabled("add-keyring", this.name_row.text != "");
    }
}
