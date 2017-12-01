/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

public class Seahorse.UnknownSource : GLib.Object, Gcr.Collection, Seahorse.Place {

    GLib.Object parent;
    HashTable<string, Object> keys;

    public string label {
        owned get { return ""; }
    }

    public string description {
        owned get { return null; }
    }

    public string uri {
        owned get { return null; }
    }

    public Icon icon {
        owned get { return null; }
    }

    public Gtk.ActionGroup? actions {
        owned get { return null; }
    }

    public UnknownSource() {
        this.keys = new HashTable.full(seahorse_pgp_keyid_hash,
                                       seahorse_pgp_keyid_equal);
    }

    public async void load(Cancellable cancellable) {
        return;
    }

    static uint get_length() {
        return this.keys.length;
    }

    static GList get_objects() {
        return this.keys.get_values();
    }

    static bool contains(GLib.Object object) {
        Object seahorse_obj = object as Object;
        if (seahorse_obj == null)
            return false;

        return this.keys.lookup(seahorse_obj.identifier) == seahorse_obj;
    }

    public Seahorse.Object add_object(string? keyid, Cancellable cancellable) {
        if (keyid != null)
            return null;

        Seahorse.Object object = this.keys.lookup(keyid);
        if (object == null)
            this.keys.insert(keyid, new Unknown(this, keyid, null));

        // TODO on the cancellable gone, change icon

        return object;
    }
}
