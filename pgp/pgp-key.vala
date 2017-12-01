/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
 * Copyright (C) 2017 Niels De Graef
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

public class Seahorse.Pgp.Key : Object, Viewable {
    private string keyid;

    /**
     * PGP User Ids
     */
    public List<Uid> uids { get; set; }

    /**
     * PGP subkeys
     */
    public List<SubKey> subkeys {
        get { return this._subkeys; }
        set { set_subkeys(value); }
    }
    private List<SubKey> _subkeys;

    /**
     * Photos for the key
     */
    public List<Photo> photos { get; set; }

    /**
     * Description for key
     */
    public string description {
        get { return this.usage == Seahorse.Usage.PRIVATE_KEY ? _("Personal PGP key") : _("PGP key"); }
    }

    /**
     * Validity of this key
     */
    public Seahorse.Validity validity {
        get { return Seahorse.Validity.UNKNOWN; }
    }

    /**
     * Trust in this key
     */
    public Seahorse.Validity trust {
        get { return Seahorse.Validity.UNKNOWN; }
    }

    /**
     * Unique fingerprint for this key
     */
    public string? fingerprint {
        get { return (this._subkeys != null)? this._subkeys.data.fingerprint : ""; }
    }

    /**
     * Date this key expires on (0 if never)
     */
    public ulong expires {
        get { return (this._subkeys != null)? this._subkeys.data.expires : 0; }
    }

    /**
     * The length of this key
     */
    public uint length {
        get { return (this._subkeys != null)? this._subkeys.data.length : 0; }
    }

    /**
     * The algorithm of this key
     */
    public string algo {
        get { return (this._subkeys != null)? this._subkeys.data.algorithm : 0; }
    }

    private void set_subkeys(List<SubKey> subkeys) {
        if (subkeys == null)
            return;

        string? keyid = this.subkeys.data.keyid;
        if (keyid == null)
            return;

        // The keyid can't change
        if (this.keyid != null) {
            if (this.keyid != keyid) {
                warning("The keyid of a PgpKey changed by setting a different subkey on it: %s != %s",
                        this.keyid, keyid);
                return;
            }

        } else {
            this.keyid = keyid;
        }

        this._subkeys = subkeys.copy();

        notify_property("subkeys");
    }

    public void realize() {
        string identifier;
        string identifier = has_subkeys()? Key.calc_identifier(this.subkeys.data.keyid) : "";

        // The type
        string icon_name;
        if (this.usage == Seahorse.Usage.PRIVATE_KEY) {
            icon_name = GCR_ICON_KEY_PAIR;
        } else {
            icon_name = GCR_ICON_KEY;
            if (this.usage == Seahorse.Usage.NONE)
                this.usage = Seahorse.Usage.PUBLIC_KEY;
        }

        this.label = calc_name();
        this.markup = calc_markup();
        this.nickname = calc_short_name();
        this.identifier = identifier;
        this.icon = new ThemedIcon(icon_name);
    }

    public Gtk.Window create_viewer(Gtk.Window parent) {
        return KeyProperties.show(this, parent);
    }

    string calc_identifier(string keyid) {
        size_t len = keyid.length;
        if (len > 8)
            keyid += len - 8;

        return keyid;
    }

    public string? get_keyid() {
        if (!has_subkeys())
            return null;

        return this.subkeys.data.keyid;
    }

    public bool has_keyid(string match) {
        if (match == null || this.subkeys == null)
            return false;

        uint n_match = match.length;
        foreach (SubKey subkey in this.subkeys) {
            string? keyid = subkey.get_keyid();
            if (keyid == null)
                return false;

            uint n_keyid = keyid.length;
            if (n_match <= n_keyid) {
                keyid += (n_keyid - n_match);
                if (strncmp (keyid, match, n_match) == 0)
                    return true;
            }
        }

        return false;
    }

    /*
     * PGP key ids can be of varying lengths. Shorter keyids are the last
     * characters of the longer ones. When hashing, match on the last 8
     * characters.
     */
    public static uint hash(void* v) {
        string keyid = v;
        size_t len = keyid.length;
        if (len > 8)
            keyid += len - 8;
        return keyid.hash();
    }

    public static bool keyid_equal(void* v1, void* v2) {
        string keyid_1 = (string) v1;
        string keyid_2 = (string) v2;
        size_t len_1 = keyid_1.length;
        size_t len_2 = keyid_2.length;

        if (len_1 != len_2 && len_1 >= 8 && len_2 >= 8) {
            keyid_1 += len_1 - 8;
            keyid_2 += len_2 - 8;
        }
        return keyid_1 == keyid_2;
    }

    public bool has_subkeys() {
        return this.subkeys != null;
    }

    public bool has_uids() {
        return this.uids != null;
    }

    private string calc_short_name() {
        return has_uids()? this.uids.data.name : null;
    }

    private string calc_name() {
        if (!has_uids())
            return "";

        return this.uids.data.calc_label(this.uids.data.name,
                                         this.uids.data.email,
                                         this.uids.data.comment);
    }

    private string calc_markup() {
        StringBuilder result = new StringBuilder("<span");

        if (Seahorse.Flags.EXPIRED in this.object_flags
            || Seahorse.Flags.REVOKED in this.object_flags
            || Seahorse.Flags.DISABLED in this.object_flags)
            result.append(" strikethrough='true'");

        if (!(Seahorse.Flags.TRUSTED in this.object_flags))
            result.append("  foreground='#555555'");
        result.append_c('>');

        // The first name is the key name
        string? primary = null, name = null;
        if (has_uids()) {
            primary = name = this.uids.data.name;
            result.append(Markup.escape_text(name));
        }

        result.append("<span size='small' rise='0'>");

        if (this.uids == null || this.uids.data == null) {
            result.append("</span></span>");
            return result.str;
        }

        string? email = uids.data.email;
        if (email == "")
            email = null;
        string? comment = uids.data.comment;
        if (comment == "")
            comment = null;
        string text = Markup.printf_escaped("\n%s%s%s%s%s",
                                            email ?? "",
                                            (email != null)? " " : "",
                                            (comment != null)? "'" : "",
                                            comment ?? "",
                                            (comment != null)? "'" : "");
        result.append(text);

        if (uids.next != null) {
            foreach(Uid uid in uids.next) {
                result.append_c('\n');
                // XXX Markup_printf_escaped()

                string? name = uid.name;
                if (name != null && name != "" && name != primary)
                    result.append(name + ": ");

                string? email = uid.email;
                if (email != null && email != "")
                    result.append(email + " ");

                string? comment = uid.comment;
                if (comment != null && comment == "")
                    result.append("'" + comment + "'");
            }
        }

        result.append("</span></span>");

        return result.str;
    }
}
