/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2016 Niels De Graef
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

public class Seahorse.Ssh.Exporter : GLib.Object, Seahorse.Exporter {

    private Key? key;
    private List<Key> objects;

    private bool _secret;
    public bool secret {
        get { return this._secret; }
        set {
            this._secret = value;
            notify_property("filename");
            notify_property("file-filter");
            notify_property("content-type");
        }
    }

    public string filename { owned get { return get_filename(); } }

    public string content_type {
        get {
            return this.secret ? "application/x-pem-key" : "application/x-ssh-key";
        }
    }

    public Gtk.FileFilter file_filter {
        owned get {
            Gtk.FileFilter filter = new Gtk.FileFilter();

            if (this.secret) {
                filter.set_name(_("Secret SSH keys"));
                filter.add_mime_type("application/x-pem-key");
                filter.add_pattern("id_*");
            } else {
                filter.set_name(_("Public SSH keys"));
                filter.add_mime_type("application/x-ssh-key");
                filter.add_pattern("*.pub");
            }

            return filter;
        }
    }

    public Exporter(Key key, bool secret) {
        this.secret = secret;

        if (!add_object(key))
            assert_not_reached ();
    }

    private string? get_filename() {
        if (this.key == null)
            return null;

        KeyData? data = this.key.key_data;
        if (data != null && !data.partial) {
            string? location = null;
            if (this.secret && data.privfile != null)
                location = data.privfile;
            else if (!this.secret && data.pubfile != null)
                location = data.pubfile;
            if (location != null)
                return Path.get_basename(location);
        }

        string basename = this.key.nickname ?? _("SSH Key");

        if (this.secret) {
            string filename = "id_%s".printf(basename).strip();
            filename.delimit(BAD_FILENAME_CHARS, '_');
            return filename;
        } else {
            string filename = "%s.pub".printf(basename).strip();
            filename.delimit(BAD_FILENAME_CHARS, '_');
            return filename;
        }
    }

    public unowned GLib.List<weak GLib.Object> get_objects() {
        return this.objects;
    }

    public bool add_object(GLib.Object object) {
        Key key = object as Key;
        if (key == null)
            return false;

        if (this.secret && key.usage != Seahorse.Usage.PRIVATE_KEY)
            return false;

        this.key = key;
        this.objects.append(this.key);
        notify_property("filename");
        return true;
    }

    public async uint8[] export(GLib.Cancellable? cancellable) throws GLib.Error {
        KeyData keydata = this.key.key_data;

        if (this.secret)
            return ((Source) this.key.place).export_private(this.key).data;

        if (keydata.pubfile == null)
            throw new Error.GENERAL(_("No public key file is available for this key."));

        assert(keydata.rawdata != null);
        return "%s\n".printf(keydata.rawdata).data;
    }
}
