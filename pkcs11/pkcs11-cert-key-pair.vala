/*
 * Seahorse
 *
 * Copyright (C) 2024 Niels De Graef
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

public class Seahorse.Pkcs11.CertKeyPair : Object, Deletable, Exportable, Viewable {

    public Gck.Attribute* id { get; set; default = null; }

    private Certificate? _certificate;
    public Certificate? certificate {
        get { return this._certificate; }
        set {
            this._certificate = value;
            update_flags();
            update_label();
            update_icon();
            notify_property("certificate");
            notify_property("label");
            notify_property("icon");
            notify_property("description");
            notify_property("is-pair");
        }
    }

    private PrivateKey? _private_key;
    public PrivateKey? private_key {
        get { return this._private_key; }
        set {
            this._private_key = value;
            update_flags();
            update_label();
            update_icon();
            notify_property("private-key");
            notify_property("label");
            notify_property("icon");
            notify_property("description");
            notify_property("is-pair");
        }
    }

    public bool is_pair {
        get { return this._certificate != null && this._private_key != null; }
    }

    public bool deletable {
        get {
            unowned var token = (Token?) this.place;
            if (token == null || token.is_write_protected())
                return false;

            var cert_deletable = (this._certificate == null)
                || check_modifiable(this._certificate.attributes);
            var key_deletable = (this._private_key == null)
                || check_modifiable(this._private_key.attributes);
            return cert_deletable || key_deletable;
        }
    }

    public bool exportable {
        get {
            return ((this.certificate != null) && this.certificate.exportable)
                || ((this.private_key != null) && this.private_key.exportable);
        }
    }

    public string description {
        owned get {
            if (this.is_pair) {
                return _("Personal certificate and key");
            } else if (this._certificate != null) {
                if (Flags.PERSONAL in this.object_flags)
                    return _("Personal certificate");
                else
                    return _("Certificate");
            } else {
                return _("Private key");
            }
        }
    }

    public CertKeyPair.for_cert(Token? token, Certificate certificate) {
        GLib.Object(place: token, certificate: certificate);
    }

    public CertKeyPair.for_private_key(Token? token, PrivateKey key) {
        GLib.Object(place: token, private_key: key);
    }

    public Seahorse.Panel create_panel() {
        return new Pkcs11.CertKeyPairPanel(this);
    }

    private bool check_modifiable(Gck.Attributes? attributes) {
        if (attributes != null) {
            bool ret = true;
            attributes.find_boolean(CKA.MODIFIABLE, out ret);
            return ret;
        }

        return false;
    }

    public Seahorse.DeleteOperation create_delete_operation() {
        return new Pkcs11.DeleteOperation(this);
    }

    public ExportOperation create_export_operation() {
        // We only support exporting the certificate atm
        return new Pkcs11.CertificateDerExportOperation(this.certificate);
    }

    private void update_label() {
        if (this.certificate != null) {
            var sn = this.certificate.get_subject_name();
            if (sn != null)
                this.label = sn;
        } else if (this.private_key != null) {
            var cka_label = this.private_key.get_cka_label();
            if (cka_label != null)
                this.label = cka_label;
        } else { // fall back
            if (this.certificate != null) {
                this.label = _("Unnamed Certificate");
            } else {
                this.label = _("Unnamed Private Key");
            }
        }
    }

    private void update_flags() {
        // If a matching private key, then this is personal
        if (this._private_key != null) {
            this.object_flags = Flags.PERSONAL | Flags.TRUSTED;
            return;
        }

        var cert_attributes = this.certificate.attributes;
        ulong category = 0;
        if (cert_attributes != null &&
            cert_attributes.find_ulong(CKA.CERTIFICATE_CATEGORY, out category)) {

            if (category == 2) {
                this.object_flags = 0;
                return;
            }
            if (category == 1) {
                this.object_flags = Flags.PERSONAL;
                return;
            }
        }

        bool is_ca;
        if (this.certificate.get_basic_constraints(out is_ca, null)) {
            this.object_flags = is_ca ? Flags.NONE : Flags.PERSONAL;
            return;
        }

        this.object_flags = Flags.PERSONAL;
    }

    private void update_icon() {
        if (this.certificate != null) {
            var icon = new GLib.ThemedIcon("application-certificate-symbolic");
            if (this.private_key != null) {
                var eicon = new ThemedIcon("key-item-symbolic");
                var emblem = new GLib.Emblem(eicon);
                this.icon = new GLib.EmblemedIcon(icon, emblem);
            } else {
                this.icon = icon;
            }
        } else if (this.private_key != null) {
            this.icon = new ThemedIcon("key-item-symbolic");
        } else {
            warning("Can't update icon for cert/key pair without either set");
        }
    }
}
