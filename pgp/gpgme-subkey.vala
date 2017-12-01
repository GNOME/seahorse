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

public class Seahorse.GpgME.SubKey : Pgp.SubKey {

    /**
     * GPGME Public Key that this subkey is on
     */
    public GPG.Key pubkey { get; set; }

    /**
     * GPGME Subkey
     */
    public GPG.SubKey subkey {
        get { return this._subkey; }
        set { set_subkey(value); }
    }
    private GPG.SubKey _subkey;

    public SubKey(GPG.Key pubkey, GPG.SubKey subkey) {
        GLib.Object(pubkey: pubkey,
                    subkey: subkey);
    }

    public void set_subkey(GPG.SubKey subkey) {
        /* Make sure that this subkey is in the pubkey */
        int index = -1, i = 0;
        for (GPG.SubKey sub = this.pubkey.subkeys; sub != null; sub = sub.next) {
            if (sub == subkey) {
                index = i;
                break;
            }
            i++;
        }

        if (index < 0)
            return;

        /* Calculate the algorithm */
        string? algo_type = subkey.pubkey_algo.get_name();
        if (algo_type == null)
            algo_type = C_("Algorithm", "Unknown");
        else if ("Elg" == algo_type || "ELG-E" == algo_type)
            algo_type = _("ElGamal");

        /* Additional properties */
        this._subkey = subkey;

        this.index = index;
        this.keyid = subkey.keyid;
        this.algorithm = algo_type;
        this.length = subkey.length;
        this.description = calc_description(GpgME.Uid.calc_name(self.pubkey.uids), index);
        this.fingerprint = calc_fingerprint(subkey.fpr);
        this.created = subkey.timestamp;
        this.expires = subkey.expires;

        /* The order below is significant */
        Seahorse.Flags flags = 0;
        if (subkey.revoked)
            flags |= Seahorse.Flags.REVOKED;
        if (subkey.expired)
            flags |= Seahorse.Flags.EXPIRED;
        if (subkey.disabled)
            flags |= Seahorse.Flags.DISABLED;
        if (flags == 0 && !subkey.invalid)
            flags |= Seahorse.Flags.IS_VALID;
        if (subkey.can_encrypt)
            flags |= Seahorse.Flags.CAN_ENCRYPT;
        if (subkey.can_sign)
            flags |= Seahorse.Flags.CAN_SIGN;
        if (subkey.can_certify)
            flags |= Seahorse.Flags.CAN_CERTIFY;
        if (subkey.can_authenticate)
            flags |= Seahorse.Flags.CAN_AUTHENTICATE;

        this.flags = flags;

        notify_property("subkey");
    }
}
