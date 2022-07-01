/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

public enum Seahorse.Validity {
    REVOKED   = -3,
    DISABLED  = -2,
    NEVER     = -1,
    UNKNOWN   =  0,
    MARGINAL  =  1,
    FULL      =  5,
    ULTIMATE  =  10;

    /**
     * @return A string describing the validity.
     */
    public unowned string? get_string () {
        switch (this) {
            case Seahorse.Validity.UNKNOWN:
                return C_("Validity", "Unknown");
            case Seahorse.Validity.NEVER:
                return C_("Validity", "Never");
            case Seahorse.Validity.MARGINAL:
                return C_("Validity", "Marginal");
            case Seahorse.Validity.FULL:
                return C_("Validity", "Full");
            case Seahorse.Validity.ULTIMATE:
                return C_("Validity", "Ultimate");
            case Seahorse.Validity.DISABLED:
                return C_("Validity", "Disabled");
            case Seahorse.Validity.REVOKED:
                return C_("Validity", "Revoked");
            default:
                return null;
        }
    }
}
