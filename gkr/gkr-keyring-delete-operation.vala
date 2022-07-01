/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

public class Seahorse.Gkr.KeyringDeleteOperation : DeleteOperation {

    public KeyringDeleteOperation(Keyring keyring) {
        add_keyring(keyring);
    }

    protected override Adw.AlertDialog create_confirm_dialog() {
        var dialog = base.create_confirm_dialog();
        dialog.body = _("All items in the keyring will also be permanently deleted!");
        return dialog;
    }

    public void add_keyring(Gkr.Keyring keyring) {
        if (contains(keyring))
            return;
        this.items.add(keyring);
    }

    public override async bool execute(Cancellable? cancellable) throws GLib.Error {
        debug("Deleting %u keyrings", this.items.length);
        foreach (unowned var item in this.items) {
            var keyring = (Gkr.Keyring) item;
            yield keyring.delete(cancellable);
        }
        return true;
    }
}
