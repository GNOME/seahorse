/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2012 Stefan Walter
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

public class Seahorse.Pkcs11.CertificateDerExportOperation : ExportOperation {

    public Gcr.Certificate certificate { get; construct set; }

    public CertificateDerExportOperation(Gcr.Certificate certificate) {
        GLib.Object(certificate: certificate);
    }

    public override async bool execute(Cancellable? cancellable = null)
                                       throws GLib.Error {
        size_t written;
        yield this.output.write_all_async (this.certificate.get_der_data(),
                                           Priority.DEFAULT,
                                           cancellable,
                                           out written);
        return true;
    }

    public override Gtk.FileDialog create_file_dialog(Gtk.Window? parent) {
        var dialog = new Gtk.FileDialog();
        dialog.accept_label = _("Export SSH key");
        dialog.initial_name = get_suggested_filename();

        var file_filters = new GLib.ListStore(typeof(Gtk.FileFilter));
        var filter = new Gtk.FileFilter();
        filter.name = _("Certificates (DER encoded)");
        filter.add_mime_type ("application/pkix-cert");
        filter.add_mime_type ("application/x-x509-ca-cert");
        filter.add_mime_type ("application/x-x509-user-cert");
        filter.add_pattern ("*.cer");
        filter.add_pattern ("*.crt");
        filter.add_pattern ("*.cert");
        file_filters.append(filter);

        return dialog;
    }

    public const string BAD_FILENAME_CHARS = "/\\<>|:?;";

    private string? get_suggested_filename() {
        string? label = null;
        if (this._certificate != null) {
            label = this._certificate.label;
            if (label == null)
                label = this._certificate.description;
        }
        if (label == null)
            label = _("Certificate");

        string filename = label + ".crt";
        return filename.delimit(BAD_FILENAME_CHARS, '_');
    }
}
