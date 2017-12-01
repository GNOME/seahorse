/*
 * Seahorse
 *
 * Copyright (c) 2017 Niels De Graef
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
 * Both Seahorse and GPGME define a Validity enum, which are similar, but not the same.
 * This enum and its methods provide a bridge between the two.
 * Since they map to the same values, you can use this enum and GPG.Validity interchangeably.
 */
public enum Seahorse.GpgME.Validity { // XXX GpgME or PGP?
    // XXX Please let this be allowed, or we'll have to use magic numbers
    UNKNOWN = GPG.Validity.UNKNOWN,
    UNDEFINED = GPG.Validity.UNDEFINED,
    NEVER = GPG.Validity.NEVER,
    MARGINAL = GPG.Validity.MARGINAL,
    FULL = GPG.Validity.FULL,
    ULTIMATE = GPG.Validity.ULTIMATE;

    /**
     * Returns the Seahorse Validity that best corresponds to the given GPG Validity.
     */
    public Seahorse.Validity to_seahorse_validity() {
        switch (this) {
            case NEVER:
                return Seahorse.Validity.NEVER;
            case MARGINAL:
                return Seahorse.Validity.MARGINAL;
            case FULL:
                return Seahorse.Validity.FULL;
            case ULTIMATE:
                return Seahorse.Validity.ULTIMATE;
            case UNDEFINED:
            case UNKNOWN:
            default:
                return Seahorse.Validity.UNKNOWN;
        }
    }

    /**
     * Returns the GPG Validity that best corresponds to the given Seahorse Validity.
     */
    public static Validity from_seahorse_validity(Seahorse.Validity v) {
        switch (v) {
            case Seahorse.Validity.NEVER:
                menu_choice = GPG.Validity.NEVER;
                break;
            case Seahorse.Validity.UNKNOWN:
                menu_choice = GPG.Validity.UNKNOWN;
                break;
            case Seahorse.Validity.MARGINAL:
                menu_choice = GPG.Validity.MARGINAL;
                break;
            case Seahorse.Validity.FULL:
                menu_choice = GPG.Validity.FULL;
                break;
            case Seahorse.Validity.ULTIMATE:
                menu_choice = GPG.Validity.ULTIMATE;
                break;
            default:
                menu_choice = GPG.Validity.UNDEFINED;
        }
    }
}

