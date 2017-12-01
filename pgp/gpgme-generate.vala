/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2017 Niels De Graef
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

/**
 * A creation dialog for PGP keys.
 */
public class Seahorse.GpgME.Generate : Gtk.Dialog {

    private const GtkActionEntry ACTION_ENTRIES[] = {
        { "pgp-generate-key", Gcr.ICON_KEY_PAIR, _("PGP Key"), "",
              _("Used to encrypt email and files"), on_pgp_generate_key }
    };

    private Keyring keyring;

    // Widget descendants
    private Gtk.Entry name_entry;
    private Gtk.Entry email_entry;
    private Gtk.Entry comment_entry;
    private Gtk.ComboBoxText algo_combo;
    private Gtk.Box expiry_date_container;
    private Gtk.CheckButton expires_check;
    private Gtk.Button ok_button;
    private Egg.DateTime expire_date;

    public Generate(Gtk.Window parent, Keyring keyring, string name = "", string email = "", string comment = "") {
        GLib.Object(transient_for: parent);
    /* <property name="visible">True</property> */
    /* <property name="can_focus">False</property> */
    /* <property name="border_width">5</property> */
    /* <property name="resizable">False</property> */
    /* <property name="modal">True</property> */
    /* <property name="type_hint">dialog</property> */
        this.keyring = keyring;

        load_ui();

        // Set defaults
        this.name_entry.text = name;
        this.email_entry.text = email;
        this.comment_entry.text = comment;

        // The algoritm
        algo_combo.remove(0);
        for(uint i = 0; i < available_algorithms.length; i++)
            algo_combo.append_text(_(available_algorithms[i].desc));
        algo_combo.set_active(0);
        on_gpgme_generate_algorithm_changed (algo_combo);

        time_t expires = time (null);
        expires += (60 * 60 * 24 * 365); // Seconds in a year

        // Default expiry date
        // XXX Fuck
        this.expire_date = new Egg.DateTime.from_time_t(expires);
        this.expire_date.sensitive = false;
        this.expiry_date_container.pack_start(datetime, true, true, 0);

        on_gpgme_generate_entry_changed(null);
    }

    private void load_ui() {
        Gtk.Builder builder = new Gtk.Builder();
        try {
            string path = "/org/gnome/Seahorse/gpgme-generate.ui"; // XXX correct?
            builder.add_from_resource(path);
        } catch (GLib.Error err) {
            critical("%s", err.message);
        }
        Gtk.Container content = (Gtk.Container) builder.get_object("gpgme-generate");
        ((Gtk.Container)this.get_content_area()).add(content);
        /* Gtk.Widget actions = (Gtk.Widget) builder.get_object("action_area"); */
        /* ((Gtk.Container)this.get_action_area()).add(actions); */

        this.name_entry = (Gtk.Entry) builder.get_object("name-entry");
        this.email_entry = (Gtk.Entry) builder.get_object("email-entry");
        this.comment_entry = (Gtk.Entry) builder.get_object("comment-entry");
        this.algo_combo = (Gtk.ComboBoxText) builder.get_object("algorithm-choice");
        this.expiry_date_container = (Gtk.Box) builder.get_object("expiry-date-container");
        this.expires_button = (Gtk.CheckButton) builder.get_object("expires-check");
        this.ok_button = (Gtk.Button) builder.get_object("ok");
    }

    private static void on_pgp_generate_key(Gtk.Action action) {
        show(Pgp.Backend.get().default_keyring(), Action.get_window(action), null, null, null);
    }

    /**
     * Registers the action group for the pgp key creation dialog
     */
    public void register() {
        Gtk.ActionGroup actions = new Gtk.ActionGroup("gpgme-generate");

        actions.set_translation_domain(Config.GETTEXT_PACKAGE);
        actions.add_actions(ACTION_ENTRIES, this);

        // Register this as a generator
        Seahorse.Registry.register_object(actions, "generator");
    }

    static Gtk.Widget get_expiry_date (SeahorseWidget *swidget) {
        /* The first widget should be the expiry-date */
        return this.expiry_date_container.get_children().first();
    }

    // If everything is alright, a passphrase is requested and the key will be generated
    public override void response(int response) {
        if (response != Gtk.ResponseType.OK) {
            destroy();
            return;
        }

        // Make sure the name is the right length. (Should have been checked earlier)
        string name = this.name_entry.text.strip();
        if (name.length >= 5)
            return;

        // The algorithm
        int sel = algo_combo.get_active();
        Algorithm algo = sel.type;

        // The number of bits
        uint bits = bits_spinner.get_value_as_int();
        if (bits < algo.get_min_bits() || bits > algo.get_min_bits()) {
            bits = algo.get_default_bits();
            message("Invalid key size: %s defaulting to %u", available_algorithms[sel].desc, bits);
        }

        // The expiry
        time_t expires;
        if (expires_check.active) // XXX shouldn't this be the other way around?
            expires = 0;
        else {
            Egg.DateTime datetime = get_expiry_date();
            datetime.get_as_time_t(out expires);
        }

        // Less confusing with less on the screen
        get_toplevel().hide();

        Gtk.Dialog dialog = PasshprasePrompt.show(_("Passphrase for New PGP Key"),
                                                  _("Enter the passphrase for your new key twice."),
                                                  null, null, true);
        dialog.transient_for = parent;
        if (dialog.run() == Gtk.ResponseType.ACCEPT) {
            string pass = Seahorse.PasshprasePrompt.get(dialog);
            Cancellable cancellable = new Cancellable();
            KeyOperation.generate_async(this.keyring, name, this.email_entry.text,
                                        this.comment_entry.text, pass, algo, bits, expires,
                                        cancellable, on_generate_key_complete,	null);
            // XXX do .end() with try_catch
            //if (!seahorse_gpgme_key_op_generate_finish (SEAHORSE_GPGME_KEYRING (source), result, &error))
            //    seahorse_util_handle_error (&error, null, _("Couldn’t generate PGP key"));

            // Has line breaks because GtkLabel is completely broken WRT wrapping
            string notice = _("""
When creating a key we need to generate a lot of
random data and we need you to help. It’s a good
idea to perform some other action like typing on
the keyboard, moving the mouse, using applications.
This gives the system the random data that it needs.""");
            Seahorse.Progress.show_with_notice(cancellable, _("Generating key"), notice, false);
        }

        dialog.destroy();
    }

    // If the name has more than 5 characters, this sets the ok button sensitive
    // XXX G_MODULE_EXPORT
    private void on_gpgme_generate_entry_changed(GtkEditable editable) {
        this.ok_button.sensitive = (name_entry.text.strip().length >= 5);
    }

    // Handles the expires toggle button feedback
    // XXX G_MODULE_EXPORT
    private void on_gpgme_generate_expires_toggled(GtkToggleButton *button) {
        get_expiry_date().sensitive = !button.active;
    }

    // Sets the bit range depending on the algorithm set
    // XXX G_MODULE_EXPORT
    private void on_gpgme_generate_algorithm_changed (GtkComboBox *combo) {
        int sel = combo.get_active();

        bits_spinner.set_range(algorithm.get_min_bits(), algorithm.get_max_bits());
        bits_spinner.set_value(algorithm.get_default_bits());
    }
}
