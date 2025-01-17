/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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
 */

/**
 * Represents an SSH key, consisting of a public/private pair.
 */
public class Seahorse.Ssh.Key : Object, Viewable, Seahorse.Item, Exportable, Deletable {

    // Note: a lot of properties rely on this one, so you want to keep it first
    private KeyData? _key_data;
    public KeyData key_data {
        get { return this._key_data; }
        construct set { this._key_data = value; }
    }

    private unowned Source? source;
    public Place? place {
        owned get { return this.source; }
        set { this.source = (Source) value; }
    }

    private static GLib.Icon PUBLIC_KEY_ICON = new ThemedIcon("key-item-symbolic");
    //XXX some emblem?
    private static GLib.Icon PRIVATE_KEY_ICON = new ThemedIcon("key-item-symbolic");
    public GLib.Icon? icon {
        get {
            if (this.usage == Usage.PUBLIC_KEY)
                return PUBLIC_KEY_ICON;
            return PRIVATE_KEY_ICON;
        }
    }

    public string title {
        get { return this.comment ?? _("Secure Shell Key"); }
    }

    public string? subtitle {
        owned get {
            return Path.get_basename(this.key_data.privfile ?? this.key_data.pubfile);
        }
    }

    public string description {
        get {
            if (this.usage == Seahorse.Usage.PRIVATE_KEY)
                return _("Personal SSH key");
            return _("SSH key");
        }
    }

    public Usage usage {
        get {
            if (this.key_data.privfile != null)
                return Seahorse.Usage.PRIVATE_KEY;
            return Seahorse.Usage.PUBLIC_KEY;
        }
    }

    public Flags item_flags {
        get {
            if (this.key_data.privfile != null)
                return Flags.PERSONAL | Flags.TRUSTED;
            return this.key_data.authorized? Flags.TRUSTED : Flags.NONE;
        }
    }

    /** Unique fingerprint for this key */
    public string? fingerprint {
        get { return this.key_data.fingerprint; }
    }

    public string? comment {
        get { return this.key_data.comment; }
    }

    /** Public data for this key */
    public string? pubkey {
        get { return this.key_data.rawdata; }
    }

    /** Validity of this key */
    public Seahorse.Validity validity {
        get {
            if (this.key_data.privfile != null)
                return Seahorse.Validity.ULTIMATE;
            return Validity.UNKNOWN;
        }
    }

    /** Trust in this key */
    public Seahorse.Validity trust {
        get {
            return this.key_data.authorized? Validity.FULL : Validity.UNKNOWN;
        }
    }

    /** The length of this key */
    public uint length {
        get { return this.key_data.length; }
    }

    public Algorithm algo {
        get { return this.key_data.algo; }
    }

    public string? location {
        get { return this.key_data.get_location(); }
    }

    public uint strength {
        get { return this.key_data.length; }
    }

    public bool deletable { get { return true; } }

    public bool exportable { get { return true; } }

    public Key(Source? source, KeyData key_data) {
        Object(key_data: key_data, place: source);
    }

    public void refresh() {
        // TODO: Need to work on key refreshing
    }

    public ExportOperation create_export_operation() {
        return new Ssh.KeyExportOperation(this, false);
    }

    public Seahorse.DeleteOperation create_delete_operation() {
        return new Ssh.KeyDeleteOperation(this);
    }

    public Seahorse.Panel create_panel() {
        return new KeyPanel(this);
    }

    /**
     * Sometimes keys loaded later on have more information (e.g. keys loaded
     * from authorized_keys), so propagate that up to the previously loaded key.
     */
    public void merge_keydata(KeyData keydata) {
        if (!this.key_data.authorized && keydata.authorized) {
            this.key_data.authorized = true;
            // Notify the 2 properties that can change based on authorized
            notify_property("item-flags");
            notify_property("trust");
        }
    }

    public struct KeyParseResult {
        public KeyData[] public_keys;
        public SecData[] secret_keys;
    }

    /**
     * Parses an input stream into public/private keys.
     *
     * @param input The input stream that needs to be parsed.
     * @param cancellable Can be used to cancel the parsing.
     */
    public static async KeyParseResult parse(GLib.InputStream input,
                                             Cancellable? cancellable = null)
                                             throws GLib.Error {
        var pubkeys = new GenericArray<KeyData>();
        var seckeys = new GenericArray<SecData>();

        // Fetch the data into a string
        var data = new DataInputStream(input);

        while (true) {
            // Read the next line, and remove leading whitespace
            var raw_line = yield data.read_line_utf8_async(Priority.DEFAULT, cancellable, null);
            if (raw_line == null)
                break;

            string line = raw_line.chug();

            // Ignore comments and empty lines (not a parse error, but no data)
            if (line == "" || line.has_prefix("#"))
                continue;

            // First of all, check for a private key, as it can span several lines
            if (SecData.contains_private_key(line)) {
                try {
                    var secdata = SecData.parse_data(data, line);
                    seckeys.add(secdata);
                    continue;
                } catch (GLib.Error e) {
                    warning(e.message);
                }
            }

            // See if we have a public key
            var keydata = KeyData.parse_line(line);
            pubkeys.add(keydata);
        }

        var result = KeyParseResult();
        result.public_keys = pubkeys.steal();
        result.secret_keys = seckeys.steal();
        return result;
    }

    /**
     * Parses the contents of the given file into public/private keys.
     *
     * @param data The file that will be parsed.
     * @param cancellable Can be used to cancel the parsing.
     */
    public static async KeyParseResult parse_file(string filename,
                                                  Cancellable? cancellable = null) throws GLib.Error {
        var file = GLib.File.new_for_path(filename);
        var file_stream = yield file.read_async();
        return yield parse(file_stream, cancellable);
    }
}
