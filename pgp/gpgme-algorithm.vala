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

namespace Seahorse {
namespace GpgME { // XXX GpgME or PGP?

/**
 * Enumerates the supported algorithms in which a PGP key can be encrypted.
 * This is also known as the 'type' of a key.
 */
public enum Algorithm {
    UNKNOWN,
    RSA,
    RSA_SIGN,
    DSA_ELGAMAL,
    DSA_SIGN;

    /**
     * Returns a (non-localized) string representation.
     */
    public string to_localized_string() {
        switch (this) {
            case UNKNOWN:
                return _("");
            case RSA:
                return _("RSA");
            case RSA_SIGN:
                return _("RSA (sign only)");
            case DSA_ELGAMAL:
                return _("DSA ElGamal");
            case DSA_SIGN:
                return _("DSA (sign only)");
            default:
                assert_not_reached();
        };
    }

    /**
     * Returns the default bits used for a given algorithm, or 0 if unknown.
     */
    public uint get_default_bits() {
        if (this == UNKNOWN)
            return 0;

        return 2048;
    }

    /**
     * Returns the maximum supported bits for a given algorithm, or 0 if unknown.
     */
    public uint get_max_bits() {
        switch (this) {
            case UNKNOWN:
                return 0;
            case RSA:
            case RSA_SIGN:
            case DSA_ELGAMAL:
                return 4096;
            case DSA_SIGN:
                // XXX Depends onds GPG Version, see key-op.h
                return 3072;
            default:
                assert_not_reached();
        }
    }

    /**
     * Returns the minimum supported bits for a given algorithm, or 0 if unknown.
     */
    public uint get_min_bits() {
        switch (this) {
            case UNKNOWN:
                return 0;
            case RSA:
            case RSA_SIGN:
                return 1024;
            case DSA_ELGAMAL:
                return 768;
            case DSA_SIGN:
                return 768;
            default:
                assert_not_reached();
        }
    }

    public bool bits_supported(uint bits) {
        return (get_min_bits() < bits) && (bits < get_max_bits());
    }
}

}
}

