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

[GtkTemplate (ui = "/org/gnome/Seahorse/seahorse-gkr-add-keyring.ui")]
public class Seahorse.Gkr.KeyringAdd : Gtk.Dialog {
    [GtkChild]
    private unowned Gtk.Entry name_entry;

    construct {
        set_response_sensitive(Gtk.ResponseType.ACCEPT, false);
    }

    public KeyringAdd(Gtk.Window? parent) {
        GLib.Object(
            transient_for: parent,
            use_header_bar: 1
        );
    }

    public override void response(int resp) {
        if (resp != Gtk.ResponseType.ACCEPT) {
            destroy();
            return;
        }

        var cancellable = Dialog.begin_request(this);
        var service = Backend.instance().service;
        Secret.Collection.create.begin(service, this.name_entry.text, null, 0,
                                       cancellable, (obj, res) => {
            /* Clear the operation without cancelling it since it is complete */
            Dialog.complete_request(this, false);

            try {
                Secret.Collection.create.end(res);
            } catch (GLib.Error err) {
                Util.show_error(this, _("Couldn’t add keyring"), err.message);
            }

            destroy();
        });
    }

    [GtkCallback]
    private void on_name_entry_changed(Gtk.Editable editable) {
        set_response_sensitive(Gtk.ResponseType.ACCEPT, this.name_entry.text != "");
    }
}
