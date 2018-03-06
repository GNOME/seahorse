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
        GLib.Object(transient_for: parent);

        Gtk.Widget button = new Gtk.Button.from_stock(Gtk.Stock.CANCEL);
        button.show();
        add_action_widget(button, Gtk.ResponseType.CANCEL);

        this.import = new Gcr.ImportButton(_("Import"));
        this.import.importing.connect(() => this.viewer.clear_error());
        this.import.imported.connect(on_import_button_imported);
        this.import.show();
        ((Gtk.Box) get_action_area()).pack_start(this.import, false, true, 0);

        Gtk.Frame frame = new Gtk.Frame(_("<b>Data to be imported:</b>"));
        ((Gtk.Label) frame.label_widget).use_markup = true;
        ((Gtk.Box) get_content_area()).pack_start(frame, true, true, 0);
        frame.set_border_width(6);
        frame.show();

        this.viewer = new Gcr.ViewerWidget();
        this.viewer.added.connect((v, r, parsed) => this.import.add_parsed(parsed));
        this.viewer.show();

        frame.add(this.viewer);
    }

    public void add_uris(string[] uris) {
        foreach (string uri in uris)
            this.viewer.load_file(File.new_for_uri(uri));
    }

    public void add_text(string? display_name, string text) {
        this.viewer.load_data(display_name, (uint8[]) text);
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
