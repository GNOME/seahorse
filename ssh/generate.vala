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

public class Seahorse.Ssh.Generate : Gtk.Dialog {
    public const int DEFAULT_DSA_SIZE = 1024;
    public const int DEFAULT_RSA_SIZE = 2048;

    private const Gtk.ActionEntry ACTION_ENTRIES[] = {
        { "ssh-generate-key", Gcr.ICON_KEY_PAIR, N_ ("Secure Shell Key"), "",
          N_("Used to access other computers (eg: via a terminal)"), on_ssh_generate_key }
    };

    private Source source;

    private Gtk.Grid details_grid;
    private KeyLengthChooser key_length_chooser;
    private Gtk.Entry email_entry;
    private Gtk.ComboBoxText algorithm_combo_box;
    private Gtk.Button create_with_setup_button;
    private Gtk.Button create_no_setup_button;

    public Generate(Source src, Gtk.Window parent) {
        GLib.Object(border_width: 5,
                    title: _("New Secure Shell Key"),
                    resizable: false,
                    modal: true,
                    transient_for: parent);
        this.source = src;

        load_ui();

        this.key_length_chooser = new KeyLengthChooser();
        this.details_grid.attach(this.key_length_chooser, 1, 1);

        // on_algo_changed() gets called, bits chooser is setup
        algorithm_combo_box.set_active(0);
    }

    private static void on_ssh_generate_key(Gtk.Action action) {
        Generate generate_dialog = new Generate(Backend.instance.get_dot_ssh(),
                                                Action.get_window(action));
        generate_dialog.show();
    }

    public static void register() {
        Gtk.ActionGroup actions = new Gtk.ActionGroup("ssh-generate");

        actions.set_translation_domain(Config.GETTEXT_PACKAGE);
        actions.add_actions(ACTION_ENTRIES, null);

        // Register this as a generator
        Seahorse.Registry.register_object(actions, "generator");
    }

    // FIXME: normally we would do this using GtkTemplate, but this is quite hard with the current build setup
    private void load_ui() {
        Gtk.Builder builder = new Gtk.Builder();
        try {
            string path = "/org/gnome/Seahorse/seahorse-ssh-generate.ui";
            builder.add_from_resource(path);
        } catch (GLib.Error err) {
            GLib.critical("%s", err.message);
        }
        Gtk.Container content = (Gtk.Container) builder.get_object("ssh-generate");
        ((Gtk.Container)this.get_content_area()).add(content);
        Gtk.Widget actions = (Gtk.Widget) builder.get_object("action_area");
        ((Gtk.Container)this.get_action_area()).add(actions);

        this.details_grid = (Gtk.Grid) builder.get_object("details_grid");
        this.email_entry = (Gtk.Entry) builder.get_object("email-entry");
        this.algorithm_combo_box = (Gtk.ComboBoxText) builder.get_object("algorithm-combo-box");
        this.create_no_setup_button = (Gtk.Button) builder.get_object("create-no-setup");
        this.create_with_setup_button = (Gtk.Button) builder.get_object("create-with-setup");

        // Signals
        this.algorithm_combo_box.changed.connect(on_algo_changed);
        this.create_no_setup_button.clicked.connect((b) => create_key(false));
        this.create_with_setup_button.clicked.connect((b) => create_key(true));
    }

    private void on_algo_changed(Gtk.ComboBox combo) {
        string t = algorithm_combo_box.get_active_text();
        this.key_length_chooser.algorithm = Algorithm.from_string(t);
    }

    private void create_key(bool upload) {
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
        Cancellable cancellable = new Cancellable();
        GenerateOperation op = new GenerateOperation();
        op.generate_async.begin(filename, email, type, bits, cancellable, (obj, res) => {
            try {
                op.generate_async.end(res);

                // The result of the operation is the key we generated
                source.add_key_from_filename.begin(filename, (obj, res) => {
                    try {
                        Key key = source.add_key_from_filename.end(res);

                        if (upload && key != null) {
                            List<Key> keys = new List<Key>();
                            keys.append(key);
                            Upload.prompt(keys, null);
                        }
                    } catch (GLib.Error e) {
                        Seahorse.Util.show_error(null, _("Couldn’t load newly generated Secure Shell key"), e.message);
                    }
                });
            } catch (GLib.Error e) {
                Seahorse.Util.show_error(null, _("Couldn’t generate Secure Shell key"), e.message);
            }
        });
        Seahorse.Progress.show(cancellable, _("Creating Secure Shell Key"), false);

        destroy();
    }

}
