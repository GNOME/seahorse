/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

public class Seahorse.Pgp.Signature : GLib.Object {

    /**
     * GPG Key ID
     */
    [Notify]
    public string keyid { get; set; }

    /**
     * PGP signature type
     */
    public uint sigtype {
        get {
            Pgp.Key? key = Backend.get_default_keyring(null).lookup(this.keyid);

            if (key != null) {
                if (key.usage == Seahorse.Usage.PRIVATE_KEY)
                    return SKEY_PGPSIG_TRUSTED | SKEY_PGPSIG_PERSONAL;
                if (Seahorse.Flags.TRUSTED in key.object_flags)
                    return SKEY_PGPSIG_TRUSTED;
            }

            return 0;
        }
    }


    public Signature(string keyid) {
        this.keyid = keyid;

        this.notify["keyid"].connect(() => notify_property("sigtype"));
        this.notify["flags"].connect(() => notify_property("sigtype"));
    }
}
