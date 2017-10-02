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

public class Seahorse.Ssh.Deleter : Seahorse.Deleter {

    private bool have_private;
    private List<Key> keys;

    public Deleter(Key key) {
        if (!add_object(key))
            assert_not_reached ();
    }

    public override Gtk.Dialog create_confirm(Gtk.Window? parent) {
        uint num = this.keys.length();

        string confirm, prompt;
        if (this.have_private) {
            assert(num == 1);

            prompt = _("Are you sure you want to delete the secure shell key “%s”?")
                         .printf(this.keys.data.label);
            confirm = _("I understand that this secret key will be permanently deleted.");

        } else if (num == 1) {
            prompt = _("Are you sure you want to delete the secure shell key “%s”?")
                         .printf(this.keys.data.label);
            confirm = null;

        } else {
            prompt = ngettext("Are you sure you want to delete %u secure shell key?",
                              "Are you sure you want to delete %u secure shell keys?",
                              num).printf(num);
            confirm = null;
        }

        Seahorse.DeleteDialog dialog = new Seahorse.DeleteDialog(parent, "%s", prompt);

        if (confirm != null) {
            dialog.check_label = confirm;
            dialog.check_require = true;
        }

        return dialog;
    }

    public override unowned List<weak GLib.Object> get_objects () {
        return this.keys;
    }

    public override bool add_object(GLib.Object object) {
        Key key = object as Key;
        if (this.have_private || key == null)
            return false;

        if (key.usage == Seahorse.Usage.PRIVATE_KEY) {
            if (this.keys != null)
                return false;
            this.have_private = true;
        }

        this.keys.append(key);
        return true;
    }

    public override async bool delete(GLib.Cancellable? cancellable) throws GLib.Error {
        foreach (Key key in this.keys)
            delete_key(key);

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
