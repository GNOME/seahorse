/*
 * Seahorse
 *
 * Copyright (C) 2013 Red Hat Inc.
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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Author: Stef Walter <stefw@redhat.com>
 */

public class Seahorse.Pkcs11.DeleteOperation : Seahorse.DeleteOperation {

    public DeleteOperation(CertKeyPair pair) {
        this.items.add(pair);
    }

    public override async bool execute(Cancellable? cancellable) throws GLib.Error {
        debug("Deleting %u PKCS#11 objects", this.items.length);
        foreach (unowned var item in this.items) {
            var pair = (CertKeyPair) item;
            unowned var token = (Token?) pair.token;

            try {
                if (pair.certificate != null) {
                    yield pair.certificate.destroy_async(cancellable);
                    if (token != null)
                        token.remove_object(pair.certificate);
                }
                if (pair.private_key != null) {
                    yield pair.private_key.destroy_async(cancellable);
                    if (token != null)
                        token.remove_object(pair.private_key);
                }

            } catch (GLib.Error e) {
                /* Ignore objects that have gone away */
                if (e.domain != Gck.Error.quark() ||
                    e.code != CKR.OBJECT_HANDLE_INVALID)
                    throw e;
            }
        }
        return true;
    }
}
