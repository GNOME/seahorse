/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2017 Niels De Graef
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

public class Seahorse.GpgME.SecretDeleter : Seahorse.Deleter {

    private Key key;
    private List<Key> keys;

    public SecretDeleter(Key key) {
        this.keys = new List<Key>();
        if (!add_object(key))
            assert_not_reached();
    }

    public override Gtk.Dialog create_confirm(Gtk.Window? parent) {
        string prompt = _("Are you sure you want to permanently delete %s?")
                        .printf(this.key.label);

        Seahorse.DeleteDialog dialog = new Seahorse.DeleteDialog(parent, "%s", prompt);
        dialog.set_check_label(_("I understand that this secret key will be permanently deleted."));
        dialog.set_check_require(true);

        return dialog;
    }

    public override unowned List<weak GLib.Object> get_objects() {
        return this.keys;
    }

    public override bool add_object(GLib.Object object) {
        Key? key = object as Key;
        if (key == null || key.usage == Seahorse.Usage.PRIVATE_KEY || this.key != null)
            return false;

        this.key = key;
        this.keys.append(key);
        return true;
    }

    public override async bool delete(GLib.Cancellable? cancellable) throws GLib.Error {
        GPG.Error error = KeyOperation.delete_pair(key);
        if (seahorse_gpgme_propagate_error (gerr, &error)) {
            g_simple_async_result_take_error (res, error);
            return false;
        }

        return true;
    }
}
