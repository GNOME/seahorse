/*
 * Seahorse
 *
 * Copyright (c) 2016 Niels De Graef
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
 * Enumerates the possible algorithms in which an SSH key can be encrypted.
 * This is also known as the 'type' of a key.
 */
public enum Seahorse.Ssh.Algorithm {
    UNKNOWN,
    RSA,
    DSA,
    ECDSA,
    ED25519;

    /**
     * Returns a (non-localized) string representation.
     *
     * @return A string representation, or null if UNKNOWN.
     */
    public string? to_string() {
        switch(this) {
            case UNKNOWN:
                return "";
            case RSA:
                return "RSA";
            case DSA:
                return "DSA";
            case ECDSA:
                return "ECDSA";
            case ED25519:
                return "ED25519";
            default:
                assert_not_reached ();
        };
    }

    /** Returns whether the algorihm is considered deprecated */
    public bool is_deprecated() {
        switch(this) {
            case RSA:
            case ECDSA:
            case ED25519:
                return false;
            case DSA:
                return true;
            default:
                assert_not_reached();
        };
    }

    /**
     * Converts a string representation into a Algorithm.
     *
     * @return The algorithm (can be UNKNOWN).
     */
    public static Algorithm from_string(string? type) {
        if (type == null || type == "")
            return UNKNOWN;

        string _type = type.strip().down();
        switch (_type) {
            case "rsa":
                return RSA;
            case "dsa":
            case "dss":
                return DSA;
            case "ecdsa":
                return ECDSA;
            case "ed25519":
                return ED25519;
            default:
                return UNKNOWN;
        }
    }

    /**
     * Tries to make an educated guess for the algorithm type.
     * Basically it just checks if the given string contains the type.
     *
     * @return The guessed algorithm (can be UNKNOWN)
     */
    public static Algorithm guess_from_string(string? str) {
        if (str == null || str == "")
            return UNKNOWN;

        string str_down = str.down();
        if ("rsa" in str_down)
            return RSA;

        if (("dsa" in str_down) || ("dss" in str_down))
            return DSA;

        if ("ecdsa" in str_down)
            return ECDSA;

        if ("ed25519" in str_down)
            return ED25519;

        return UNKNOWN;
    }
}
