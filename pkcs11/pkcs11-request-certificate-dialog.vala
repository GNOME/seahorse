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
 *
 * Stef Walter <stefw@redhat.com>
 */

[GtkTemplate (ui = "/org/gnome/Seahorse/pkcs11-request-certificate-dialog.ui")]
public class Seahorse.Pkcs11.RequestCertificateDialog : Adw.Dialog {

    public Pkcs11.PrivateKey private_key { construct; get; }

    [GtkChild] private unowned Adw.EntryRow name_row;
    private uint8[] _encoded;

    static construct {
        install_action("create", null, (Gtk.WidgetActionActivateFunc) action_create);
    }

    construct {
        action_set_enabled("create", false);
    }

    public RequestCertificateDialog(Pkcs11.PrivateKey private_key) {
        GLib.Object(private_key: private_key);
    }

    [GtkCallback]
    private void on_name_row_changed(Gtk.Editable editable) {
        action_set_enabled("create", editable.text != "");
    }

    private void action_create(string? action_name, Variant? param) {
        var parent_win = get_root() as Gtk.Window;

        var interaction = new Interaction(parent_win);
        var session = this.private_key.get_session();
        session.set_interaction(interaction);

        var req = Gcr.CertificateRequest.prepare(Gcr.CertificateRequestFormat.CERTIFICATE_REQUEST_PKCS10,
                                                 this.private_key);
        req.set_cn(this.name_row.text);
        req.complete_async.begin(null, (obj, res) => {
            try {
                req.complete_async.end(res);
                save_certificate_request(req);
            } catch (GLib.Error err) {
                Util.show_error(parent_win, _("Couldn’t create certificate request"), err.message);
            }
        });
    }

    private static string BAD_FILENAME_CHARS = "/\\<>|?*";

    private void save_certificate_request(Gcr.CertificateRequest req) {
        var dialog = new Gtk.FileDialog();
        dialog.title = _("Save certificate request");

        // Filter on specific extensions/mime types
        var filters = new GLib.ListStore(typeof(Gtk.FileFilter));

        var der_filter = new Gtk.FileFilter();
        der_filter.name = _("Certificate request");
        der_filter.add_mime_type("application/pkcs10");
        der_filter.add_pattern("*.p10");
        der_filter.add_pattern("*.csr");
        filters.append(der_filter);

        var pem_filter = new Gtk.FileFilter();
        pem_filter.name = _("PEM encoded request");
        pem_filter.add_mime_type("application/pkcs10+pem");
        pem_filter.add_pattern("*.pem");
        filters.append(pem_filter);

        dialog.filters = filters;
        dialog.default_filter = der_filter;

        // Set the initial filename
        var label = this.private_key.get_cka_label() ?? _("Certificate Request");
        var filename = label + ".csr";
        filename = filename.delimit(BAD_FILENAME_CHARS, '_');
        dialog.initial_name = filename;

        dialog.save.begin(get_root() as Gtk.Window, null, (obj, res) => {
            try {
                var file = dialog.save.end(res);
                if (file == null)
                    return;

                bool textual = file.get_path().has_suffix(".pem");
                this._encoded = req.encode(textual);

                file.replace_contents_async.begin(this._encoded, null, false,
                                                  GLib.FileCreateFlags.NONE,
                                                  null, (obj, res) => {
                    try {
                        string new_etag;
                        file.replace_contents_async.end(res, out new_etag);
                    } catch (GLib.Error err) {
                        Util.show_error(parent, _("Couldn’t save certificate request"), err.message);
                    }
                });
            } catch (Error e) {
                Util.show_error(parent, _("Couldn’t save certificate request"), e.message);
            }
        });
    }
}
