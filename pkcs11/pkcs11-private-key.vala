/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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
 */

public class Seahorse.Pkcs11.PrivateKey : Gck.Object, Gck.ObjectCache {

    private Gck.Attributes? _attributes;
    public Gck.Attributes attributes {
        owned get { return this._attributes; }
        set {
            this._attributes = value;
            notify_property("attributes");
            check_certificate_request_capable();
        }
    }

    public bool exportable {
        get { return false; }
    }

    public bool certificate_request_capable { get; private set; default = false; }

    public PrivateKey.from_attributes(Gck.Attributes attributes) {
        fill(attributes);
    }

    public void fill(Gck.Attributes attributes) {
        var builder = new Gck.Builder(Gck.BuilderFlags.NONE);
        if (this._attributes != null)
            builder.add_all(this._attributes);
        builder.set_all(attributes);
        this._attributes = builder.end();
        notify_property("attributes");
    }

    public string? get_cka_label() {
        if (this._attributes != null) {
            string label;
            if (this._attributes.find_string(CKA.LABEL, out label))
                return label;
        }

        return null;
    }

    public string? get_key_type() {
        if (this._attributes == null)
            return null;

        ulong key_type;
        if (!this._attributes.find_ulong(CKA.KEY_TYPE, out key_type))
            return null;

        switch (key_type) {
            case CKK.RSA:
                return _("RSA");
            case CKK.DSA:
                return _("DSA");
            case CKK.DH:
                return _("DH");
            case CKK.ECDSA:
                return _("ECDSA");
        }
        return null;
    }

    private void check_certificate_request_capable() {
        Gcr.CertificateRequest.capable_async.begin(this, null, (obj, res) => {
            try {
                if (Gcr.CertificateRequest.capable_async.end(res)) {
                    this.certificate_request_capable = true;
                    notify_property("certificate-request-capable");
                }
            } catch (GLib.Error err) {
                message("couldn't check capabilities of private key: %s", err.message);
            }
        });
    }
}
