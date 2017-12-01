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

public class Seahorse.GpgME.Photo : Pgp.Photo {

    /**
     * GPGME Public Key this photo is on
     */
    public GPG.Key pubkey { get; private set; }

    /**
     * Index of photo UID
     */
    public uint index { get; set; }

    public Photo(GPG.Key pubkey, Gdk.Pixbuf pixbuf, uint index) {
        GLib.Object(pubkey: pubkey,
                    pixbuf: pixbuf,
                    index: index);
    }
}
