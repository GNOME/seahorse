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

namespace Seahorse {
namespace Ssh {

/**
 * Represents an SSH key, consisting of a public/private pair.
 */
public class Key : Seahorse.Object, Seahorse.Exportable, Seahorse.Deletable, Seahorse.Viewable {
    public const int SSH_IDENTIFIER_SIZE = 8;

    private KeyData? _keydata;
    public KeyData? key_data {
        get { return _keydata; }
        set { this._keydata = value; changed_key(); }
    }

    /**
     * Unique fingerprint for this key
     */
    public string? fingerprint {
        get { return (this.key_data != null) ? this.key_data.fingerprint : null; }
    }

    /**
     * Description
     */
    public string description {
        get { return this.usage == Seahorse.Usage.PRIVATE_KEY ? _("Personal SSH key") : _("SSH key"); }
    }

    /**
     * Validity of this key
     */
    public Seahorse.Validity validity {
        get {
            if (this.key_data != null && this.key_data.privfile != null)
                return Seahorse.Validity.ULTIMATE;
            return 0;
        }
    }

    /**
     * Trust in this key
     */
    public Seahorse.Validity trust {
        get {
            if (this.key_data != null && this.key_data.authorized)
                return Seahorse.Validity.FULL;
            return 0;
        }
    }

    /**
     * Date this key expires on (0 if never)
     */
    public ulong expires {
        get { return 0; }
    }

    /**
     * The length of this key
     */
    public uint length {
        get { return this.key_data != null ? this.key_data.length : 0; }
    }

    public bool deletable {
        get { return Seahorse.Flags.DELETABLE in this.flags; }
    }

    public bool exportable {
        get { return Seahorse.Flags.EXPORTABLE in this.flags; }
    }

    public Key(Source source, KeyData key_data) {
        GLib.Object(place: source, key_data: key_data);
    }

    private string? parse_first_word(string line) {
        string PARSE_CHARS = "\t \n@;,.\\?()[]{}+/";

        string[] words = line.split_set(PARSE_CHARS, 2);
        return (words.length == 2)? words[0] : null;
    }

    private void changed_key() {
        string display = null;
        string simple = null;

        // TODO - this is a stupid hack until we can set the properties normally
        Value labelValue = Value(typeof(string));
        Value actionsValue = Value(typeof(Gtk.ActionGroup));
        Value iconValue = Value(typeof(Icon?));
        Value usageValue = Value(typeof(Seahorse.Usage));
        Value nicknameValue = Value(typeof(string));
        Value flagsValue = Value(typeof(uint));
        Value markupValue = Value(typeof(string));
        Value identifierValue = Value(typeof(string));

        if (this.key_data != null) {
            // Try to make display and simple names
            if (this.key_data.comment != null) {
                display = this.key_data.comment;
                simple = parse_first_word(this.key_data.comment);

            // No names when not even the fingerpint loaded
            } else if (this.key_data.fingerprint == null) {
                display = _("(Unreadable Secure Shell Key)");
            // No comment, but loaded
            } else {
                display = _("Secure Shell Key");
            }

            if (simple == null)
                simple = _("Secure Shell Key");
        }

        if (this.key_data == null || this.key_data.fingerprint == null) {
            labelValue.set_string("");
            set_property("label", labelValue);
            iconValue.set_object(null);
            set_property("icon", iconValue);
            usageValue.set_enum(Seahorse.Usage.NONE);
            set_property("usage", usageValue);
            labelValue.set_string("");
            set_property("nickname", nicknameValue);
            usageValue.set_uint((uint) Seahorse.Flags.DISABLED);
            set_property("object-flags", flagsValue);
            return;
        }

        Seahorse.Usage usage;
        Seahorse.Flags flags = Seahorse.Flags.EXPORTABLE | Seahorse.Flags.DELETABLE;
        Icon icon;
        if (this.key_data.privfile != null) {
            usage = Seahorse.Usage.PRIVATE_KEY;
            flags |= Seahorse.Flags.PERSONAL | Seahorse.Flags.TRUSTED;
            icon = new ThemedIcon(Gcr.ICON_KEY_PAIR);
        } else {
            flags = 0;
            usage = Seahorse.Usage.PUBLIC_KEY;
            icon = new ThemedIcon(Gcr.ICON_KEY);
        }

        string filename = Path.get_basename(this.key_data.privfile ?? this.key_data.pubfile);

        string markup = Markup.printf_escaped("%s<span size='small' rise='0' foreground='#555555'>\n%s</span>",
                                              display, filename);

        string identifier = calc_identifier(this.key_data.fingerprint);
        Actions actions = Actions.instance();

        if (this.key_data.authorized)
            flags |= Seahorse.Flags.TRUSTED;

        actionsValue.set_object(actions);
        set_property("actions", actionsValue);
        markupValue.set_string(markup);
        set_property("markup", markupValue);
        labelValue.set_string(display);
        set_property("label", labelValue);
        iconValue.set_object(icon);
        set_property("icon", iconValue);
        usageValue.set_enum(usage);
        set_property("usage", usageValue);
        nicknameValue.set_string(simple);
        set_property("nickname", nicknameValue);
        identifierValue.set_string(identifier);
        set_property("identifier", identifierValue);
        flagsValue.set_uint((uint) flags);
        set_property("object-flags", flagsValue);
    }

    public void refresh() {
        // TODO: Need to work on key refreshing
    }

    public GLib.List<Seahorse.Exporter> create_exporters(ExporterType type) {
        List<Seahorse.Exporter> exporters = new List<Seahorse.Exporter>();
        exporters.append(new Exporter(this, false));
        return exporters;
    }

    public Seahorse.Deleter create_deleter() {
        return new Deleter(this);
    }

    public Gtk.Window? create_viewer(Gtk.Window? parent) {
        KeyProperties properties_dialog = new KeyProperties(this, parent);
        properties_dialog.show();
        return properties_dialog;
    }

    public Algorithm get_algo() {
        return this.key_data.algo;
    }

    public string? get_location() {
        if (this.key_data == null)
            return null;
        return this.key_data.get_location();
    }

    public uint get_strength() {
        return (this.key_data != null)? this.key_data.length : 0;
    }

    /**
     * Creates a valid identifier for an SSH key from a given string.
     *
     * @return A valid identifier, or null if the result is too short.
     */
    public static string? calc_identifier(string id) {
        // Strip out all non-alphanumeric chars and limit length to SSH_ID_SIZE
        try {
            Regex regex = new Regex("[^a-zA-Z0-9]");
            string result = regex.replace(id, id.length, 0, "");

            if (result.length >= SSH_IDENTIFIER_SIZE)
                return result.substring(0, result.length);
        } catch (RegexError e) {
            warning("Couldn't create regex for calc_identifier. Message: %s".printf(e.message));
        }

        return null;
    }

    /**
     * Sometimes keys loaded later on have more information (e.g. keys loaded
     * from authorized_keys), so propagate that up to the previously loaded key.
     */
    public void merge_keydata(KeyData keydata) {
        if (!this.key_data.authorized && keydata.authorized) {
            this.key_data.authorized = true;

            // Let the key know something's changed
            this.key_data = this.key_data;
        }
    }

    /**
     * Parses a string into public/private keys.
     *
     * @param data The string that needs to be parsed.
     * @param pub_handler Will be called anytime a public key has been parsed.
     *                    If null, nothing will be done if a public key is parsed.
     * @param priv_handler Will be called anytime a private key has been parsed.
     *                     If null, nothing will be done if a private key is parsed.
     * @param cancellable Can be used to cancel the parsing.
     */
    public static async void parse(string data,
                                   PubParsedHandler? pub_handler,
                                   PrivParsedHandler? priv_handler = null,
                                   Cancellable? cancellable = null) throws GLib.Error {
        if (data == null || data == "")
            return;

        StringBuilder toParse = new StringBuilder(data.chug());
        while (toParse.str.length > 0) {
            // First of all, check for a private key, as it can span several lines
            if (SecData.contains_private_key(toParse.str)) {
                try {
                    SecData secdata = SecData.parse_data(toParse);
                    if (priv_handler != null)
                        priv_handler(secdata);
                    continue;
                } catch (GLib.Error e) {
                    warning(e.message);
                }
            }

            // We're sure we'll have at least 1 element
            string[] lines = toParse.str.split("\n", 2);
            string line = lines[0];
            toParse.erase(0, line.length);
            if (lines.length == 2) // There was a \n, so don't forget to erase it as well
                toParse.erase(0, 1);

            // Comments and empty lines, not a parse error, but no data
            if (line.strip() == "" || line.has_prefix("#"))
                continue;

            // See if we have a public key
            try {
                KeyData keydata = KeyData.parse_line(line);
                if (pub_handler != null)
                    pub_handler(keydata);
            } catch (GLib.Error e) {
                warning(e.message);
            }
        }
    }

    /**
     * Parses the contents of the given file into public/private keys.
     *
     * @param data The file that will be parsed.
     * @param pub_handler Will be called anytime a public key has been parsed.
     *                    If null, nothing will be done if a public key is parsed.
     * @param priv_handler Will be called anytime a private key has been parsed.
     *                     If null, nothing will be done if a private key is parsed.
     * @param cancellable Can be used to cancel the parsing.
     */
    public static async void parse_file(string filename,
                                        PubParsedHandler? pub_handler,
                                        PrivParsedHandler? priv_handler = null,
                                        Cancellable? cancellable = null) throws GLib.Error {
        string contents;
        FileUtils.get_contents(filename, out contents);

        yield parse(contents, pub_handler, priv_handler, cancellable);
    }

    /**
     * Takes care of the public key that has been found in a string while parsing.
     */
    public delegate bool PubParsedHandler(KeyData data);

    /**
     * Takes care of the private key that has been found in a string while parsing.
     */
    public delegate bool PrivParsedHandler(SecData data);
}

}
}
