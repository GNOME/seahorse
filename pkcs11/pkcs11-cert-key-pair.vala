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

public class Seahorse.Pkcs11.CertKeyPair : GLib.Object,
                                           Deletable, Exportable, Viewable,
                                           Seahorse.Item {

    public Gck.Attribute* id { get; set; default = null; }

    private Certificate? _certificate;
    public Certificate? certificate {
        get { return this._certificate; }
        set {
            if (this._certificate == value)
                return;
            this._certificate = value;
            notify_property("certificate");
            notify_property("is-pair");
            update_details();
        }
    }

    private PrivateKey? _private_key;
    public PrivateKey? private_key {
        get { return this._private_key; }
        set {
            if (this._private_key == value)
                return;
            this._private_key = value;
            notify_property("private-key");
            notify_property("is-pair");
            update_details();
        }
    }

    public bool is_pair {
        get { return this._certificate != null && this._private_key != null; }
    }

    private unowned Token? _token;
    public Place? place {
        owned get { return this._token; }
        set { this._token = (Token) value; }
    }
    public Token? token {
        get { return this._token; }
    }

    private GLib.Icon _icon;
    public GLib.Icon? icon {
        get { return this._icon; }
    }

    private string _title = "";
    public string title {
        get { return this._title; }
    }

    private string _subtitle = "";
    public string? subtitle {
        owned get { return this._subtitle; }
    }

    public string description {
        get {
            if (this.is_pair)
                return _("Personal certificate and key");

            if (this._certificate != null) {
                if (Flags.PERSONAL in this.item_flags)
                    return _("Personal certificate");
                return _("Certificate");
            }
            return _("Private key");
        }
    }

    public Usage usage {
        get { return Usage.NONE; }
    }

    public Flags item_flags {
        get {
            // If a matching private key, then this is personal
            if (this._private_key != null)
                return Flags.PERSONAL | Flags.TRUSTED;

            var cert_attributes = this.certificate.attributes;
            ulong category = 0;
            if (cert_attributes != null &&
                cert_attributes.find_ulong(CKA.CERTIFICATE_CATEGORY, out category)) {

                if (category == 2)
                    return Flags.NONE;
                if (category == 1)
                    return Flags.PERSONAL;
            }

            bool is_ca;
            if (this.certificate.get_basic_constraints(out is_ca, null))
                return is_ca ? Flags.NONE : Flags.PERSONAL;

            return Flags.PERSONAL;
        }
    }

    public bool deletable {
        get {
            unowned var token = (Token?) this.token;
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

    public CertKeyPair.for_cert(Token? token, Certificate certificate) {
        Object(place: token, certificate: certificate);
    }

    public CertKeyPair.for_private_key(Token? token, PrivateKey key) {
        Object(place: token, private_key: key);
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

    private void update_details() {
        string? title = null, subtitle = null;

        if (this.private_key != null) {
            var cka_label = this.private_key.get_cka_label();
            title = cka_label ?? _("Unnamed Private Key");
            if (this.certificate != null) {
                var icon = new GLib.ThemedIcon("application-certificate-symbolic");
                var emblem = new GLib.Emblem(new ThemedIcon("key-item-symbolic"));
                this._icon = new GLib.EmblemedIcon(icon, emblem);
            } else {
                this._icon = new ThemedIcon("key-item-symbolic");
            }
        } else if (this.certificate != null) {
            title = this.certificate.issuer_name ?? _("Unnamed Certificate");
            subtitle = _("Issued by: %s").printf(this.certificate.issuer_name);
            this._icon = new GLib.ThemedIcon("application-certificate-symbolic");
        }

        if (title != this._title) {
            this._title = title;
            notify_property("title");
        }
        if (subtitle != this._subtitle) {
            this._subtitle = subtitle;
            notify_property("subtitle");
        }

        notify_property("description");
        notify_property("icon");
    }
}
