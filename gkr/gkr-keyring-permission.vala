/*
 * Seahorse
 *
 * Copyright (C) 2018 Anukul Sangwan
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

public class Seahorse.Gkr.KeyringPermission : GLib.Permission {

    private Keyring keyring;

    public KeyringPermission(Keyring keyring) {
        this.keyring = keyring;
        refresh();
    }

    private void disable() {
        this.impl_update(false, false, false);
    }

    private void refresh() {
        this.impl_update(!this.keyring.locked, Lockable.can_unlock(this.keyring), Lockable.can_lock(this.keyring));
    }

    public override async bool acquire_async(GLib.Cancellable? cancellable = null) throws GLib.Error {
        disable();
        yield this.keyring.unlock(null, null);
        refresh();
        return true;
    }

    public override async bool release_async(GLib.Cancellable? cancellable = null) throws GLib.Error {
        disable();
        yield this.keyring.lock(null, null);
        refresh();
        return true;
    }

}
