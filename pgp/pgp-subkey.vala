/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

public class Seahorse.Pgp.SubKey : GLib.Object {

    // XXX notify properties?

    /**
     * PGP subkey index
     */
    public uint index { get; set; }

    /**
     * GPG Key ID
     */
    public string keyid { get; set; }

    /**
     * PGP subkey flags
     */
    public Seahorse.Flags flags { get; set; }

    /**
     * PGP key length
     */
    public uint length { get; set; }

    /**
     * GPG Algorithm
     */
    public string algorithm { get; set; }

    /**
     * Date this key was created on
     */
    public ulong created { get; set; }

    /**
     * Date this key expires on
     */
    public ulong expires { get; set; }

    /**
     * Key Description
     */
    public string description { get; set; }

    /**
     * PGP Key Fingerprint
     */
    public string fingerprint { get; set; }

    public SubKey() {
    }

    private struct FlagName {
        public uint flag;
        public string name;
    }

    private const FlagName[] flag_names = {
        { Seahorse.Flags.CAN_ENCRYPT,      _("Encrypt")      },
        { Seahorse.Flags.CAN_SIGN,         _("Sign")         },
        { Seahorse.Flags.CAN_CERTIFY,      _("Certify")      },
        { Seahorse.Flags.CAN_AUTHENTICATE, _("Authenticate") }
    };

    public string get_usage() {
        StringBuilder str = new StringBuilder();

        bool previous = false;
        foreach (FlagName flag_name in flag_names) {
            if (flag_name.flag in this.flags) {
                if (previous)
                    str.append(", ");

                previous = true;
                str.append(_(flag_name.name));
            }
        }

        return str.str;
    }

    public string calc_description(string? name, uint index) {
        if (name == null)
            name = _("Key");

        if (index == 0)
            return name;

        return _("Subkey %u of %s").printf(index, name);
    }

    /**
     * Takes runs of hexadecimal digits, possibly with whitespace among them, and
     * formats them nicely in groups of four digits.
     */
    public string? calc_fingerprint(string? raw_fingerprint) {
        if (raw_fingerprint == null)
            return null;

        StringBuilder result = new StringBuilder("");

        uint num_digits = 0;
        for (uint i = 0; i < raw_fingerprint.length; i++) {
            if (raw_fingerprint[i].isxdigit()) {
                result.append_c(raw_fingerprint[i].toupper());
                num_digits++;

                if (num_digits > 0 && (num_digits % 4 == 0))
                    result.append(" ");
            }
        }

        return result.str.chomp();
    }
}
