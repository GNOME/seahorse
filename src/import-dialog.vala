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

public class Seahorse.ImportDialog : Gtk.Dialog {

    private Gcr.ViewerWidget viewer;
    private Gcr.ImportButton import;

    public ImportDialog(Gtk.Window? parent) {
        GLib.Object(
            transient_for: parent,
            title: _("Data to be imported"),
            use_header_bar: 1
        );

        Gtk.Widget button = new Gtk.Button.with_mnemonic(_("_Cancel"));
        button.show();
        add_action_widget(button, Gtk.ResponseType.CANCEL);

        this.import = new Gcr.ImportButton(_("_Import"));
        this.import.halign = Gtk.Align.END;
        this.import.visible = true;
        this.import.get_style_context().add_class("suggested-action");
        this.import.importing.connect(() => this.viewer.clear_error());
        this.import.imported.connect(on_import_button_imported);
        ((Gtk.HeaderBar) get_header_bar()).pack_end(this.import);

        this.viewer = new Gcr.ViewerWidget();
        this.viewer.added.connect((v, r, parsed) => this.import.add_parsed(parsed));
        this.viewer.show();
        ((Gtk.Box) get_content_area()).pack_end(this.viewer);
    }

    public void add_uris(string[] uris) {
        foreach (string uri in uris)
            this.viewer.load_file(File.new_for_uri(uri));
    }

    public void add_text(string? display_name, string text) {
        this.viewer.load_data(display_name, text.data);
    }

    private void on_import_button_imported(GLib.Object importer, Error? error) {
        if (error == null) {
            response(Gtk.ResponseType.OK);

            string uri = ((Gcr.Importer) importer).uri;
            foreach (Backend backend in Backend.get_registered()) {
                Place? place = backend.lookup_place(uri);
                if (place != null)
                    place.load.begin(null);
            }

        } else {
            if (!(error is GLib.IOError.CANCELLED))
                this.viewer.show_error(_("Import failed"), error);
        }
    }
}
