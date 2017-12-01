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

public class Seahorse.GpgME.Uid : Pgp.Uid {

    /**
     * GPGME Public Key that this uid is on
     */
    public GPG.Key? pubkey {
        get { return this._pubkey; }
        set { set_pubkey(value); }
    }
    private GPG.Key? _pubkey = null;

    /**
     * GPGME User ID
     */
    public GPG.UserID? userid {
        get { return this._userid; }
        set { set_userid(value); }
    }
    private GPG.UserID? _userid = null;

    /**
     * GPGME User ID Index
     */
    public uint gpgme_index { get; private set; default = 0; }

    /**
     * Actual GPG Index
     */
    public int actual_index {
        get { return (this._actual_index > 0)? this._actual_index : this.gpgme_index + 1; }
        set { this._actual_index = value; }
    }
    private int _actual_index = -1;


    public Uid(Key parent, GPG.UserID userid) {
        GLib.Object(parent: parent,
                    pubkey: parent.pubkey,
                    userid: userid);
    }

    public void set_pubkey(GPG.Key? pubkey) {
        if (this._pubkey == pubkey || compare_pubkeys(this._pubkey, pubkey))
            return;

        this.pubkey = pubkey;

        // This is expected to be set shortly along with pubkey
        this.userid = null;
    }

    public void set_userid(GPG.UserID userid) {
        if (userid == null || (this._userid != null && is_same(userid)))
            return;

        // Make sure that this userid is in the pubkey and get the index
        int index = -1, i = 0;
        for (GPG.UserID? uid = this._pubkey.uids; uid != null; ++i, uid = uid.next) {
            if (userid == uid) {
                index = i;
                break;
            }
        }

        if (index < 0)
            return;

        this._userid = userid;
        this.gpgme_index = index;

        notify_property("userid");
        notify_property("gpgme_index");

        this.comment = convert_string(userid.comment);
        this.email = convert_string(userid.email);
        this.name = convert_string(userid.name);

        realize_signatures();

        this.validity = userid.validity.to_seahorse_validity();
    }

    public string? calc_label(GPG.UserID? userid) {
        if (userid == null)
            return null;

        return convert_string(userid.uid);
    }

    public string? calc_name(GPG.UserID? userid) {
        if (userid == null)
            return null;

        return convert_string(userid.name);
    }

    public string? calc_markup(GPG.UserID? userid, Seahorse.Flags flags) {
        if (userid == null)
            return null;

        ret = base.calc_markup(convert_string(userid.name),
                               convert_string(userid.email),
                               convert_string(userid.comment),
                               flags);

        return ret;
    }

    public bool is_same(GPG.UserID? userid) {
        if (userid == null)
            return false;

        return compare_strings(this.userid.uid, userid.uid);
    }

    protected string convert_string(string? str) {
        if (str == null)
            return null;

        // If not utf8 valid, assume latin 1
        if (!str.validate())
            return g_convert (str, -1, "UTF-8", "ISO-8859-1", null, null, null);

        return str;
    }

    private void realize_signatures() {
        if (this.pubkey == null || this.userid == null)
            return;

        // If this key was loaded without signatures, then leave them as is
        if ((this.pubkey.keylist_mode & GPG.KeyListMode.SIGS) == 0)
            return;

        List<Pgp.Signature> sigs = new List<Pgp.Signature>();
        for (GPG.KeySig gsig = this.userid.signatures; gsig != null; gsig = gsig.next) {
            Pgp.Signature sig = new Pgp.Signature(gsig.keyid);

            // Order of parsing these flags is important
            Seahorse.Flags flags = 0;
            if (gsig.revoked)
                flags |= Seahorse.Flags.REVOKED;
            if (gsig.expired)
                flags |= Seahorse.Flags.EXPIRED;
            if (flags == 0 && !gsig.invalid)
                flags = Seahorse.Flags.IS_VALID;
            if (gsig.exportable)
                flags |= Seahorse.Flags.EXPORTABLE;

            sig.set_flags(flags);
            sigs.prepend(sig);
        }

        this.signatures = sigs;
    }
}
