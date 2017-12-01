/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
 * 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *  
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

namespace Seahorse {

public Gcr.Collection seahorse_keyset_pgp_signers_new() {
	Seahorse.Predicate predicate = Seahorse.Predicate() {
        type = typeof(GpgME.Key),
        usage = Seahorse.Usage.PRIVATE_KEY,
        flags = Seahorse.Flags.CAN_SIGN,
        nflags = Seahorse.Flags.EXPIRED | Seahorse.Flags.REVOKED | Seahorse.Flags.DISABLED,
        custom = pgp_signers_match
    };

	Keyring keyring = Pgp.Backend.get().get_default_keyring();
	Seahorse.Collection collection = new Seahorse.Collection.for_predicate(keyring, predicate, g_free);

	Application.pgp_settings().changed["default-key"].connect(() => {
        collection.refresh();
    });

	return collection;
}

private bool pgp_signers_match(GLib.Object obj, void* data) {
	if (!SEAHORSE_IS_PGP_KEY (obj))
		return false;

	// Default key overrides all, and becomes the only signer available
	Pgp.Key? defkey = Pgp.Backend.get().get_default_key();
	if (defkey != null && (defkey.keyid != (Pgp.Key) obj))
		return false;

	return true;
}

}
