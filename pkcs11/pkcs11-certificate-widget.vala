/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2020 Niels De Graef
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

[GtkTemplate (ui = "/org/gnome/Seahorse/pkcs11-certificate-widget.ui")]
public class Seahorse.Pkcs11.CertificateWidget : Gtk.Box {

    public Gcr.Certificate certificate { get; construct set; }

    [GtkChild] private unowned Gtk.Label name_label;
    [GtkChild] private unowned Gtk.Label issuer_short_label;
    [GtkChild] private unowned Gtk.Label expires_short_label;

    struct Rdn {
        unowned string id;
        unowned string display;
    }
    const Rdn[] RDNS = {
        { "cn", N_("Common name") },
        { "ou", N_("Organizational unit") },
        { "o", N_("Organization") },
        { "c", N_("Country") }
    };

    [GtkChild] private unowned Adw.ExpanderRow subject_row;
    [GtkChild] private unowned Adw.ExpanderRow issuer_row;

    [GtkChild] private unowned Adw.ActionRow issued_date_row;
    [GtkChild] private unowned Adw.ActionRow expires_row;

    [GtkChild] private unowned Adw.ActionRow version_row;
    [GtkChild] private unowned Adw.ActionRow serial_nr_row;

    [GtkChild] private unowned Adw.ActionRow sha256_fingerprint_row;
    [GtkChild] private unowned Adw.ActionRow md5_fingerprint_row;

    [GtkChild] private unowned Adw.ActionRow public_key_algo_row;
    [GtkChild] private unowned Adw.ActionRow public_key_size_row;
    [GtkChild] private unowned Adw.ActionRow public_key_algo_params_row;
    [GtkChild] private unowned Gtk.Label public_key_label;

    public signal void notification(Adw.Toast toast);

    static construct {
        install_action("copy-public-key", null, (Gtk.WidgetActionActivateFunc) action_copy_public_key);
    }

    construct {
        fill_in_certificate_details(this.certificate);
    }

    public CertificateWidget(Gcr.Certificate certificate) {
        GLib.Object(certificate: certificate);
    }

    private void fill_in_certificate_details(Gcr.Certificate cert) {
        // Summary
        this.name_label.label = cert.get_subject_name()?? _("Nameless certificate");
        this.issuer_short_label.label = _("Issued by: %s").printf(cert.get_issuer_name() ?? _("Unknown"));
        var expiry_date = cert.get_expiry_date();
        if (expiry_date != null)
            this.expires_short_label.label = _("Expires at %s").printf(expiry_date.format("%F"));
        else
            this.expires_short_label.label = _("Never expires");

        // Subject name
        this.subject_row.title = cert.get_subject_part("cn");
        foreach (unowned var rdn in RDNS)
            fill_in_rdn(this.subject_row, rdn, cert.get_subject_part(rdn.id));

        // Issuer name
        this.issuer_row.title = cert.get_issuer_part("cn");
        foreach (unowned var rdn in RDNS)
            fill_in_rdn(this.issuer_row, rdn, cert.get_issuer_part(rdn.id));

        // Validity
        if (expiry_date != null)
            set_row_value(this.expires_row, expiry_date.format("%F"));
        else
            set_row_value(this.expires_row, _("No expiry date"));
        if (cert.get_issued_date() != null)
            set_row_value(this.issued_date_row, cert.get_issued_date().format("%F"));
        else
            set_row_value(this.issued_date_row, _("No issued date"));

        set_row_value(this.version_row, "%lu".printf(cert.get_version()));
        set_row_value(this.serial_nr_row, cert.get_serial_number_hex());

        // fingerprints
        unowned var der_data = cert.get_der_data();
        set_row_value(this.sha256_fingerprint_row,
                      Checksum.compute_for_data(ChecksumType.SHA256, der_data));
        set_row_value(this.md5_fingerprint_row,
                      Checksum.compute_for_data(ChecksumType.MD5, der_data));

        unowned var public_key_info = cert.get_public_key_info();
        set_row_value(this.public_key_algo_row, public_key_info.get_algorithm_description());
        set_row_value(this.public_key_size_row, "%u bits".printf(public_key_info.get_key_size()));
        set_row_value(this.public_key_algo_params_row, bytes_to_hex(public_key_info.get_algorithm_parameters_raw()));
        set_label_value(this.public_key_label, bytes_to_hex(public_key_info.get_key()));

        // Extensions
        var extension_list = cert.list_extensions();
        for (uint i = 0; i < extension_list.n_items; i++) {
            unowned var extension = extension_list.get_extension(i);

            var prefs_group = new Adw.PreferencesGroup();
            prefs_group.title = extension.get_description();
            if (extension.is_critical())
                prefs_group.description = _("Critical Extension");
            else
                prefs_group.description = _("Extension");
            append(prefs_group);

            if (extension is Gcr.CertificateExtensionBasicConstraints) {
                add_basic_constraints(prefs_group,
                                      (Gcr.CertificateExtensionBasicConstraints) extension);
            } else if (extension is Gcr.CertificateExtensionKeyUsage) {
                add_key_usage(prefs_group,
                              (Gcr.CertificateExtensionKeyUsage) extension);
            } else if (extension is Gcr.CertificateExtensionExtendedKeyUsage) {
                add_ext_key_usage(prefs_group,
                                  (Gcr.CertificateExtensionExtendedKeyUsage) extension);
            } else if (extension is Gcr.CertificateExtensionCertificatePolicies) {
                add_cert_policies(prefs_group, (Gcr.CertificateExtensionCertificatePolicies) extension);
            } else if (extension is Gcr.CertificateExtensionSubjectKeyIdentifier) {
                add_ski(prefs_group, (Gcr.CertificateExtensionSubjectKeyIdentifier) extension);
            } else if (extension is Gcr.CertificateExtensionAuthorityKeyIdentifier) {
                add_aki(prefs_group, (Gcr.CertificateExtensionAuthorityKeyIdentifier) extension);
            } else if (extension is Gcr.CertificateExtensionAuthorityInfoAccess) {
                add_aia(prefs_group, (Gcr.CertificateExtensionAuthorityInfoAccess) extension);
            } else if (extension is Gcr.CertificateExtensionSubjectAltName) {
                add_san(prefs_group, (Gcr.CertificateExtensionSubjectAltName) extension);
            } else if (extension is Gcr.CertificateExtensionCrlDistributionPoints) {
                add_cdp(prefs_group, (Gcr.CertificateExtensionCrlDistributionPoints) extension);
            } else {
                add_generic_extension(prefs_group, extension);
            }
        }
    }

    private void add_basic_constraints(Adw.PreferencesGroup group,
                                       Gcr.CertificateExtensionBasicConstraints ext) {
        Adw.ActionRow row;

        row = create_property_row(_("Certificate Authority"),
                                  ext.is_ca()? _("Yes") : _("No"));
        group.add(row);

        var path_len = ext.get_path_len_constraint();
        row = create_property_row(_("Max Path Length"),
                                  (path_len != -1)? path_len.to_string() : null,
                                  _("Unset"));
        group.add(row);
    }

    private void add_key_usage(Adw.PreferencesGroup group,
                               Gcr.CertificateExtensionKeyUsage ext) {
        var descriptions = string.joinv(", ", ext.get_descriptions());
        var row = create_property_row(_("Purposes"), descriptions);
        group.add(row);
    }

    private void add_ext_key_usage(Adw.PreferencesGroup group,
                                   Gcr.CertificateExtensionExtendedKeyUsage ext) {
        var descriptions = string.joinv(", ", ext.get_descriptions());
        var row = create_property_row(_("Purposes"), descriptions);
        group.add(row);
    }

    private void add_cert_policies(Adw.PreferencesGroup group,
                                   Gcr.CertificateExtensionCertificatePolicies ext) {
        for (uint i = 0; i < ext.get_n_items(); i++) {
            unowned var policy = ext.get_policy(i);
            var row = create_property_row(_("Policy"), policy.get_name());
            group.add(row);

            for (uint j = 0; j < policy.get_n_items(); j++) {
                var qualifier = (Gcr.CertificatePolicyQualifier) policy.get_item(j);
                row = create_property_row(_("Policy Qualifier"), qualifier.get_name());
                group.add(row);
            }
        }
    }

    private void add_ski(Adw.PreferencesGroup group,
                         Gcr.CertificateExtensionSubjectKeyIdentifier ext) {
        var keyid = bytes_to_hex(ext.get_key_id(), ":");
        var row = create_property_row(_("Key Identifier"), keyid);
        row.add_css_class("monospace");
        group.add(row);
    }

    private void add_aki(Adw.PreferencesGroup group,
                         Gcr.CertificateExtensionAuthorityKeyIdentifier ext) {
        var keyid = bytes_to_hex(ext.get_key_id(), ":");
        var row = create_property_row(_("Key Identifier"), keyid);
        row.add_css_class("monospace");
        group.add(row);
    }

    private void add_aia(Adw.PreferencesGroup group,
                         Gcr.CertificateExtensionAuthorityInfoAccess ext) {
        for (uint i = 0; i < ext.get_n_items(); i++) {
            unowned var descr = ext.get_description(i);
            var method = _("Method: %s").printf(descr.get_method_name());
            unowned var location = descr.get_location().get_value();
            var row = create_property_row(method, location);
            group.add(row);
        }
    }

    private void add_san(Adw.PreferencesGroup group,
                         Gcr.CertificateExtensionSubjectAltName ext) {
        for (uint i = 0; i < ext.get_n_items(); i++) {
            unowned var name = ext.get_name(i);

            var row = create_property_row(name.get_description(), name.get_value());
            group.add(row);
        }
    }

    private void add_cdp(Adw.PreferencesGroup group,
                         Gcr.CertificateExtensionCrlDistributionPoints ext) {
        for (uint i = 0; i < ext.get_n_items(); i++) {
            unowned var endpoint = ext.get_distribution_point(i);

            unowned var full_name = endpoint.get_full_name();
            if (full_name != null) {
                string[] name_val = {};
                for (uint j = 0; j < full_name.get_n_items(); j++)
                    name_val += full_name.get_name(j).get_value();
                var row = create_property_row(_("Distribution Point"), string.joinv(";", name_val));
                group.add(row);
            }
        }
    }

    private void add_generic_extension(Adw.PreferencesGroup group,
                                       Gcr.CertificateExtension ext) {
        var value_str = bytes_to_hex(ext.value);
        var row = create_property_row(_("Raw value"), value_str);
        row.add_css_class("monospace");
        group.add(row);
    }

    private void fill_in_rdn(Adw.ExpanderRow expander_row, Rdn rdn, string? val) {
        if (val == null || val == "")
            return;

        var row = new Adw.ActionRow();
        row.add_css_class("property");
        row.title = gettext(rdn.display);
        row.subtitle = val;
        row.subtitle_selectable = true;
        expander_row.add_row(row);
    }

    private string bytes_to_hex(Bytes bytes, string? sep2char = null) {
        var result = new StringBuilder();
        unowned uint8[] data = bytes.get_data();
        for (uint i = 0; i < data.length; i++) {
            if (sep2char != null && i > 0)
                result.append(sep2char);
            result.append_printf("%02x", data[i]);
        }
        return result.free_and_steal();
    }

    private void set_label_value(Gtk.Label label,
                                 string? value,
                                 string fallback = _("Unknown")) {
        if (value != null && value != "") {
            label.label = value;
            label.remove_css_class("dim-label");
            label.selectable = true;
        } else {
            label.label = fallback;
            label.add_css_class("dim-label");
            label.selectable = false;
        }
    }

    private Adw.ActionRow create_property_row(string title,
                                              string? value,
                                              string fallback = _("Unknown")) {
        var row = new Adw.ActionRow();
        row.add_css_class("property");
        row.title = title;
        set_row_value(row, value, fallback);
        return row;
    }

    private void set_row_value(Adw.ActionRow row,
                               string? value,
                               string fallback = _("Unknown")) {
        if (value != null && value != "") {
            row.subtitle = value;
            row.remove_css_class("dim-label");
            row.subtitle_selectable = true;
        } else {
            row.subtitle = fallback;
            row.add_css_class("dim-label");
            row.subtitle_selectable = false;
        }
    }

    private void action_copy_public_key(string action_name, Variant? param) {
        unowned var spki = this.certificate.get_public_key_info();
        get_clipboard().set_text(bytes_to_hex(spki.get_key()));
        notification(new Adw.Toast(_("Copied public key to clipboard")));
    }
}
