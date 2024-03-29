/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) Niels De Graef
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
 */

[GtkTemplate (ui = "/org/gnome/Seahorse/seahorse-ssh-generate.ui")]
public class Seahorse.Ssh.Generate : Gtk.Dialog {
    public const int DEFAULT_DSA_SIZE = 1024;
    public const int DEFAULT_RSA_SIZE = 2048;

    public Ssh.Source source { get; construct set; }

    [GtkChild]
    private unowned Gtk.Grid details_grid;
    private KeyLengthChooser key_length_chooser;
    [GtkChild]
    private unowned Gtk.Entry email_entry;
    [GtkChild]
    private unowned Gtk.ComboBoxText algorithm_combo_box;

    public Generate(Source src, Gtk.Window parent) {
        GLib.Object (
            source: src,
            use_header_bar: 1,
            transient_for: parent
        );

        this.source = src;

        this.key_length_chooser = new KeyLengthChooser();
        this.key_length_chooser.halign = Gtk.Align.START;
        this.details_grid.attach(this.key_length_chooser, 1, 3);

        // on_algo_changed() gets called, bits chooser is setup
        algorithm_combo_box.set_active(0);
    }

    [GtkCallback]
    private void on_algo_changed(Gtk.ComboBox combo) {
        string t = algorithm_combo_box.get_active_text();
        this.key_length_chooser.algorithm = Algorithm.from_string(t);
    }

    /**
     * Generate a key from the fields that were filled in.
     *
     * Note: make sure you don't destroy the window before this async method
     * has finished
     */
    public async void generate_key() {
        // The email address
        string email = this.email_entry.text;

        // The algorithm
        string t = this.algorithm_combo_box.get_active_text();
        Algorithm type = Algorithm.from_string(t);
        assert(type != Algorithm.UNKNOWN);

        uint bits = this.key_length_chooser.get_length();

        // The filename
        string filename = this.source.new_filename_for_algorithm(type);

        // We start creation
        try {
            debug("Generating %s key '%s' (file '%s')", t, email, filename);
            GenerateOperation op = new GenerateOperation();
            Cancellable cancellable = new Cancellable();
            Seahorse.Progress.show(cancellable, _("Creating Secure Shell Key"), false);
            yield op.generate_async(filename, email, type, bits, cancellable);

            // We generated a key, but we still need to import it
            try {
                debug("Importing generated key (file '%s')", filename);
                Key key = yield source.add_key_from_filename(filename);
            } catch (GLib.Error e) {
                Seahorse.Util.show_error(null, _("Couldn’t load newly generated Secure Shell key"), e.message);
            }
        } catch (GLib.Error e) {
            Seahorse.Util.show_error(null, _("Couldn’t generate Secure Shell key"), e.message);
        }
    }
}
