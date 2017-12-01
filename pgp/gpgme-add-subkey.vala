/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

public class Seahorse.GpgME.AddSubkey : Gtk.Dialog {

    private Key key;

    private Gtk.SpinButton length_spinner;
    private Gtk.ComboBox type_combo;
    private Gtk.CheckButton never_expires;
    private Egg.DateTime expires_datetime;

    private const string LENGTH = "length";

    private enum Column {
      NAME,
      TYPE,
      N_COLUMNS;
    }

    public AddSubkey(Key key, Gtk.Window? parent) {
        this.key = key;

        this.title = _("Add subkey to %s").printf(pkey.label);

        Gtk.ListStore model = new Gtk.ListStore(Column.N_COLUMNS, typeof(string), typeof(int));

        this.type_combo.set_model(model);
        this.type_combo.clear();

        Gtk.CellRendererText renderer = new Gtk.CellRendererText();

        this.type_combo.pack_start(renderer, true);
        this.type_combo.add_attribute(renderer, "text", Column.NAME);

        Gtk.TreeIter iter;
        model.append(out iter);
        model.set(iter, Column.NAME, _("DSA (sign only)"),
                        Column.TYPE, 0, -1);

        this.type_combo.set_active_iter(&iter);

        model.append(out iter);
        model.set(iter, Column.NAME, _("ElGamal (encrypt only)"),
                        Column.TYPE, 1, -1);

        model.append(out iter);
        model.set(iter, Column.NAME, _("RSA (sign only)"),
                        Column.TYPE, 2, -1);

        model.append(out iter);
        model.set(iter, Column.NAME, _("RSA (encrypt only)"),
                        Column.TYPE, 3, -1);

        load_ui();
    }

    private void load_ui() {
        Gtk.Builder builder = new Gtk.Builder();
        try {
            string path = "/org/gnome/Seahorse/gpgme-add-subkey.ui";
            builder.add_from_resource(path);
        } catch (GLib.Error err) {
            GLib.critical("%s", err.message);
        }
        Gtk.Container content = (Gtk.Container) builder.get_object("add-subkey");
        ((Gtk.Container)this.get_content_area()).add(content);

        this.length_spinner = (Gtk.SpinButton) builder.get_object("length_spinner");
        this.type_combo = (Gtk.ComboBox) builder.get_object("type_combo");
        this.never_expires = (Gtk.CheckButton) builder.get_object("never_expires");
        this.ok_button = (Gtk.Button) builder.get_object("ok_button");

        // Expiration date chooser
        Gtk.Alignment? datetime_placeholder = (Gtk.Alignment) builder.get_object("datetime_placeholder");
        this.expires_datetime = new Egg.DateTime();
        this.expires_datetime.show();
        this.expires_datetime = false;
        datetime_placeholder.add(this.expires_datetime);

        // Signals
        this.type_combo.changed.connect(on_type_combo_changed);
        this.never_expires.toggled.connect(on_never_expires_toggled);
        this.ok_button.clicked.connect(on_ok_button_clicked);
    }

    private void on_type_combo_changed(Gtk.ComboBox combo) {
        Gtk.TreeIter iter;
        combo.get_active_iter(out iter);
        int type;
        combo.get_model().get(iter, Column.TYPE, out type, -1);

        this.length_spinner.set_range(type.get_min_bits(), type.get_max_bits());
        this.length_spinner.set_value(type.get_default_bits());
    }

    private void on_never_expires_toggled(Gtk.ToggleButton toggle_button) {
        this.expires_datetime.set_sensitive(!toggle_button.active);
    }

    private void on_ok_clicked (Gtk.Button button) {
        Gtk.TreeIter iter;
        this.type_combo.get_active_iter(out iter);
        int type;
        this.type_combo.model.get(&iter, Column.TYPE, out type, -1);

        uint length = this.length_spinner.get_value_as_int();

        time_t expires = 0;
        if (!this.never_expires.active)
            this.expires_datetime.get_as_time_t(out expires);

        KeyEncType real_type;
        switch (type) {
            case 0:
                real_type = DSA;
                break;
            case 1:
                real_type = ELGAMAL;
                break;
            case 2:
                real_type = RSA_SIGN;
                break;
            default:
                real_type = RSA_ENCRYPT;
                break;
        }

        this.sensitive = false;
        GPG.Error err = KeyOperation.add_subkey(this.key, real_type, length, expires);
        this.sensitive = true;

        if (!err.is_success() || err.is_cancelled())
            Util.show_error(null, _("Couldn’t add subkey"), err.strerror());

        destroy();
    }
}
