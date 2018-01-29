/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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
 * KeyServerControl: a control which allows you to select from a set
 * of keyservers.
 *
 * - Also displays shares for keys found via DNS-SD over the network.
 *
 * Properties:
 *   settings-key: (gchar*) The GSettings key to retrieve and set keyservers.
 *   none-option: (gchar*) Text to display for 'no key server'
 */
public class Seahorse.KeyserverControl : Gtk.ComboBox {

    /**
     * Settings key to read/write selection
     */
    public string settings_key { get; construct set; }

    /**
     * Puts in an option for 'no key server'
     */
    public string none_option { get; construct set; }

    private bool updating;

    private enum Column {
        TEXT,
        INFO
    }

    private enum Option {
        NONE,
        SEPARATOR,
        KEYSERVER
    }

    public KeyserverControl(string? settings_key, string? none_option) {
        GLib.Object(
            settings_key: settings_key,
            none_option: none_option
        );
        this.updating = false;

        Gtk.CellRenderer renderer = new Gtk.CellRendererText();
        pack_start(renderer, true);
        set_attributes(renderer, "text", Column.TEXT, null);
        set_row_separator_func(separator_func);

        populate_combo(true);
        this.changed.connect(on_keyserver_changed);
        PgpSettings.instance().changed["keyserver"].connect(() => populate_combo(false));
        if (this.settings_key != null)
            AppSettings.instance().changed[this.settings_key].connect(() => populate_combo(true));
    }

    public string? selected() {
        Gtk.TreeIter iter;
        if (!get_active_iter(out iter))
            return null;

        int info;
        string? server;
        this.model.get(iter, Column.TEXT, out server,
                             Column.INFO, out info,
                             -1);

        if (info == Option.KEYSERVER)
            return server;

        return null;
    }

    private void on_keyserver_changed(Gtk.ComboBox widget) {
        // If currently updating (see populate_combo), ignore
        if (this.updating)
            return;

        if (this.settings_key != null)
            AppSettings.instance().set_string(this.settings_key, selected() ?? "");
    }

    private int compare_func(Gtk.TreeModel model, Gtk.TreeIter a, Gtk.TreeIter b) {
        string desc_a, desc_b;
        int info_a, info_b;
        model.get(a, Column.TEXT, out desc_a, Column.INFO, out info_a, -1);
        model.get(b, Column.TEXT, out desc_b, Column.INFO, out info_b, -1);

        if (info_a != info_b)
            return info_a - info_b;

        if (info_a == Option.KEYSERVER)
            return desc_a.collate(desc_b);

        return 0;
    }

    private bool separator_func(Gtk.TreeModel model, Gtk.TreeIter iter) {
        int info;
        model.get(iter, Column.INFO, out info, -1);

        return info == Option.SEPARATOR;
    }

    private void populate_combo(bool with_key) {
        Gtk.TreeIter? iter, none_iter = null, chosen_iter = null;

        // Get the appropriate selection
        string? chosen = null;
        int chosen_info = Option.KEYSERVER;
        if (with_key && this.settings_key != null) {
            chosen = AppSettings.instance().get_string(this.settings_key);
        } else {
            if (get_active_iter(out iter)) {
                this.model.get(iter, Column.TEXT, out chosen,
                                     Column.INFO, out chosen_info,
                                     -1);
            }
        }

        // Mark this so we can ignore events
        this.updating = true;

        // Remove old model, and create new one
        this.model = null;

        Gtk.ListStore store = new Gtk.ListStore(2, typeof(string), typeof(int));

        // The all key servers option
        if (this.none_option != null) {
            none_iter = append_entry(store, this.none_option, Option.NONE);
            // And add a separator row
            iter = append_entry(store, null, Option.SEPARATOR);
        }

        bool chosen_iter_set = false;
        foreach (weak string keyserver in Servers.get_uris()) {
            assert (keyserver != null);
            iter = append_entry(store, keyserver, Option.KEYSERVER);
            if (chosen != null && chosen == keyserver) {
                chosen_iter = iter;
                chosen_iter_set = true;
            }
        }

        // Turn on sorting after populating the store, since that's faster
        store.set_sort_func(Column.TEXT, compare_func);
        store.set_sort_column_id(Column.TEXT, Gtk.SortType.ASCENDING);

        this.model = store;

        // Re-set the selected value, if it's still present in the model
        if (chosen_iter_set)
            set_active_iter(chosen_iter);
        else if (this.none_option != null)
            set_active_iter(none_iter);

        // Done updating
        this.updating = false;
    }

    private Gtk.TreeIter append_entry(Gtk.ListStore store, string? keyserver, Option option) {
        Gtk.TreeIter iter;
        store.append(out iter);
        store.set(iter, Column.TEXT, keyserver, Column.INFO, option);
        return iter;
    }
}
