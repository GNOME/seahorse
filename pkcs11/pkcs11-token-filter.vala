/*
 * Seahorse
 *
 * Copyright (C) 2022 Niels De Graef
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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

public class Seahorse.Pkcs11.TokenFilter : Gtk.Filter {

    /** If set, only match tokens that are writable  */
    public bool only_writable {
        get { return this._only_writable; }
        set {
            if (this._only_writable != value) {
                this._only_writable = value;
                changed(Gtk.FilterChange.DIFFERENT);
            }
        }
    }
    private bool _only_writable = false;

    /** If not MAXUINT, only match token that have the mechanism  */
    public ulong mechanism {
        get { return this._mechanism; }
        set {
            if (this._mechanism == value)
                return;

            this._mechanism = value;
            changed(Gtk.FilterChange.DIFFERENT);
        }
    }
    public ulong _mechanism = ulong.MAX;

    public override bool match (GLib.Object? item) {
        var token = (Pkcs11.Token) item;
        if (this.only_writable && (CKF.WRITE_PROTECTED & token.info.flags) != 0)
            return false;

        if (this.mechanism != uint.MAX && !token.has_mechanism(this.mechanism))
            return false;

        return true;
    }

    public override Gtk.FilterMatch get_strictness () {
        return Gtk.FilterMatch.SOME;
    }
}
