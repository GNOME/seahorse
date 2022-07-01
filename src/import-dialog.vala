/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

[GtkTemplate (ui = "/org/gnome/Seahorse/import-dialog.ui")]
public class Seahorse.ImportDialog : Adw.Window {

    [GtkChild] private unowned Gtk.Box container;

    private Gcr.Parser parser;
    private GenericArray<Gcr.Parsed> parsed_items = new GenericArray<Gcr.Parsed>();

    public InputStream input { get; construct set; }

    static construct {
        install_action("import-all", null, (Gtk.WidgetActionActivateFunc) action_import_all);
    }

    construct {
        action_set_enabled("import-all", false);

        // Create the parser and listen to its signals
        this.parser = new Gcr.Parser();
        this.parser.parsed.connect(on_parser_parsed);
        this.parser.authenticate.connect(on_parser_authenticate);

        parse_input.begin();
    }

    public ImportDialog(InputStream input,
                        string? title,
                        Gtk.Window? parent) {
        GLib.Object(
            input: input,
            title: title ?? _("Import Data"),
            transient_for: parent
        );
    }

    private async void parse_input() {
        var cancellable = new Cancellable();

        try {
            debug("Parsing input");
            yield this.parser.parse_stream_async(this.input, cancellable);
            debug("Successfully parsed input");
        } catch (GLib.Error err) {
            warning("Error parsing input: %s", err.message);
            // XXX show some kind of error status page
            show_error("Failed to read input: %s".printf(err.message));
        }
    }

    private void on_parser_parsed(Gcr.Parser parser) {
        var parsed = parser.get_parsed();
        var attributes = parsed.get_attributes();

        switch (parsed.get_format()) {
            case Gcr.DataFormat.DER_CERTIFICATE_X509:
                // Certificate
                debug("Parser found x.509 DER certificate");
                var certificate = new Gcr.SimpleCertificate(parsed.get_data());
                this.container.append(new Pkcs11.CertificateWidget(certificate));
                //XXX
                break;
            case Gcr.DataFormat.OPENPGP_PACKET:
                debug("Parser found OpenPGP packet");
                var key = Pgp.Backend.get().create_key_for_parsed(parsed);
                this.container.append(((Viewable) key).create_panel());
                break;
            case Gcr.DataFormat.OPENSSH_PUBLIC:
                debug("Parser found Public SSH Key");
                var stream = new MemoryInputStream.from_data(parsed.get_data());
                Ssh.Key.parse.begin(stream, null, (obj, res) => {
                    try {
                        var parse_res = Ssh.Key.parse.end(res);
                        // so much eek XXX
                        var key = new Ssh.Key(null, parse_res.public_keys[0]);
                        this.container.append(key.create_panel());
                    } catch (GLib.Error err) {
                        warning("Couldn't parse SSH key data: %s", err.message);
                        show_error("Couldn't parse SSH key: %s".printf(err.message));
                    }
                });
                break;
            case Gcr.DataFormat.DER_PRIVATE_KEY:
            case Gcr.DataFormat.DER_PRIVATE_KEY_RSA:
            case Gcr.DataFormat.DER_PRIVATE_KEY_DSA:
            case Gcr.DataFormat.DER_PRIVATE_KEY_EC:
                debug("Parser found private key");
                var key = new Pkcs11.PrivateKey.from_attributes(attributes);
                var pair = new Pkcs11.CertKeyPair.for_private_key(null, key);
                this.container.append(pair.create_panel());
                break;
            default:
                warning("unsupported format %u", parsed.get_format());
                show_error("Unsupported format %u".printf(parsed.get_format()));
                break;
        }

        // Add to the list of parsed items
        this.parsed_items.add(parsed);

        // Check if we have any importers
        var importers = Gcr.Importer.create_for_parsed(parsed);
        action_set_enabled("import-all", !importers.is_empty());
    }

    private bool on_parser_authenticate(Gcr.Parser parser, int count) {
        debug("Need authentication for parser");

        var label = new Gtk.Label(_("A password is needed to read the input file"));
        this.container.append(label);
        var entry = new Gtk.PasswordEntry();
        this.container.append(entry);
        var button = new Gtk.Button.with_label(_("Enter password"));
        this.container.append(button);

        button.clicked.connect((btn) => {
            var pw = entry.text;

            this.container.remove(label);
            this.container.remove(entry);
            this.container.remove(button);

            debug("Adding password to parser");
            parser.add_password(pw);
        });

        return true; // handled here
    }

    private void action_import_all(string action_name, Variant? param) {
        foreach (unowned var parsed in this.parsed_items) {
            var importers = Gcr.Importer.create_for_parsed(parsed);
            if (importers.length() == 0) {
                warning("No importers available. How was the import action called?");
                return;
            }

            var importer = importers.data;
            importer.queue_for_parsed(parsed);
            importer.import_async.begin(null, (obj, res) => {
                try {
                    importer.import_async.end(res);
                    debug("Successfully imported item");
                    close();
                } catch (Error err) {
                    warning("Import failed: %s", err.message);
                    show_error(_("Import failed: %s").printf(err.message));
                }
            });
        }
    }

    private void show_error(string message) {
        var label = new Gtk.Label(message);
        label.wrap = true;
        label.add_css_class("error");
        this.container.prepend(label);
    }
}
