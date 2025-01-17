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

public class Seahorse.Ssh.KeyExportOperation : ExportOperation {

    public Ssh.Key key { get; construct set; }

    /* Whether to export the secret (or the public) key */
    public bool secret { get; set; }

    public KeyExportOperation(Key key, bool secret)
            requires (!secret || key.usage == Seahorse.Usage.PRIVATE_KEY) {
        GLib.Object(key: key, secret: secret);
    }

    public override Gtk.FileDialog create_file_dialog(Gtk.Window? parent) {
        var dialog = new Gtk.FileDialog();
        dialog.title = _("Export SSH key");
        dialog.accept_label = _("Export");
        dialog.initial_name = get_suggested_filename();

        var file_filters = new GLib.ListStore(typeof(Gtk.FileFilter));
        var filter = new Gtk.FileFilter();
        if (this.secret) {
            filter.name = _("Secret SSH keys");
            filter.add_mime_type("application/x-pem-key");
            filter.add_pattern("id_*");
        } else {
            filter.name = _("Public SSH keys");
            filter.add_mime_type("application/x-ssh-key");
            filter.add_pattern("*.pub");
        }
        file_filters.append(filter);
        dialog.filters = file_filters;

        return dialog;
    }

    public const string BAD_FILENAME_CHARS = "/\\<>|:?;";

    private string? get_suggested_filename() {
        unowned var data = this.key.key_data;
        if (data != null && !data.partial) {
            string? location = null;
            if (this.secret && data.privfile != null)
                location = data.privfile;
            else if (!this.secret && data.pubfile != null)
                location = data.pubfile;
            if (location != null)
                return Path.get_basename(location);
        }

        unowned var basename = parse_first_word(this.key.comment) ?? this.key.algo.to_string();
        if (this.secret) {
            var filename = "id_%s".printf(basename).strip();
            filename.delimit(BAD_FILENAME_CHARS, '_');
            return filename;
        } else {
            var filename = "%s.pub".printf(basename).strip();
            filename.delimit(BAD_FILENAME_CHARS, '_');
            return filename;
        }
    }

    private string? parse_first_word(string? line) {
        if (line == null)
            return null;

        const string PARSE_CHARS = "\t \n@;,.\\?()[]{}+/";

        string[] words = line.split_set(PARSE_CHARS, 2);
        return (words.length == 2)? words[0] : null;
    }

    public override async bool execute(Cancellable? cancellable = null)
                                       throws GLib.Error {
        if (this.secret)
            return yield export_secret(cancellable);
        else
            return yield export_public(cancellable);
    }

    private async bool export_public(Cancellable? cancellable) throws GLib.Error {
        unowned var keydata = this.key.key_data;
        assert(keydata.rawdata != null);

        if (keydata.pubfile == null)
            throw new Error.GENERAL(_("No public key file is available for this key."));

        size_t written;
        yield this.output.write_all_async (keydata.rawdata.data,
                                           Priority.DEFAULT,
                                           cancellable,
                                           out written);
        yield this.output.write_all_async ("\n".data,
                                           Priority.DEFAULT,
                                           cancellable,
                                           out written);

        return true;
    }

    private async bool export_secret(Cancellable? cancellable) throws GLib.Error {
        unowned var keydata = this.key.key_data;
        assert(keydata.rawdata != null);

        if (keydata.privfile == null)
            throw new Error.GENERAL(_("No private key file is available for this key."));

        // Make an inputstream for the secret file
        var file = File.new_for_path(keydata.privfile);
        var input = yield file.read_async(Priority.DEFAULT, cancellable);

        // And funnel it to the output
        yield this.output.splice_async(input,
                                       OutputStreamSpliceFlags.CLOSE_SOURCE
                                       | OutputStreamSpliceFlags.CLOSE_TARGET,
                                       Priority.DEFAULT,
                                       cancellable);
        return true;
    }
}
