/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2016 Niels De Graef
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

public class Seahorse.Ssh.KeyDeleteOperation : DeleteOperation {

    public KeyDeleteOperation(Ssh.Key key) {
        add_item(key);
    }

    public void add_item(Ssh.Key key) {
        if (contains(key))
            return;
        this.items.add(key);
    }

    public override async bool execute(Cancellable? cancellable = null) throws GLib.Error {
        debug("Deleting %u SSH keys", this.items.length);
        foreach (unowned var item in this.items) {
            var key = (Ssh.Key) item;
            delete_key(key);
        }
        return true;
    }

    /**
     * Deletes a given key.
     *
     * @param key The key that should be deleted.
     */
    public void delete_key(Key key) throws GLib.Error {
        KeyData? keydata = key.key_data;
        if (keydata == null)
            throw new Error.GENERAL("Can't delete key with empty KeyData.");

        if (keydata.partial) { // Just part of a file for this key
            if (keydata.pubfile != null) // Take just that line out of the file
                KeyData.filter_file(keydata.pubfile, null, keydata);

        } else { // A full file for this key
            if (keydata.pubfile != null) {
                if (FileUtils.unlink(keydata.pubfile) == -1)
                    throw new Error.GENERAL("Couldn't delete public key file '%s'".printf(keydata.pubfile));
            }

            if (keydata.privfile != null) {
                if (FileUtils.unlink(keydata.privfile) == -1) {
                    throw new Error.GENERAL("Couldn't delete private key file '%s'".printf(keydata.privfile));
                }
            }
        }

        Source source = (Source) key.place;
        source.remove_object(key);
    }
}
