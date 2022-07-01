/*
 * Seahorse
 *
 * Copyright (C) 2004-2005 Stefan Walter
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

[GtkTemplate (ui = "/org/gnome/Seahorse/seahorse-prefs-keyservers.ui")]
public class Seahorse.PrefsKeyservers : Adw.PreferencesPage {

    [GtkChild] private unowned Adw.PreferencesGroup keyservers_group;
    private GenericArray<unowned KeyServerRow> ks_rows =
        new GenericArray<unowned KeyServerRow>();

    [GtkChild] private unowned Gtk.Box keyserver_publish;
    [GtkChild] private unowned Gtk.Label keyserver_publish_to_label;
    [GtkChild] private unowned Gtk.CheckButton auto_retrieve;
    [GtkChild] private unowned Gtk.CheckButton auto_sync;

    construct {
        var model = Pgp.Backend.get().get_remotes();
        on_remotes_changed(model, 0, 0, model.get_n_items());
        model.items_changed.connect(on_remotes_changed);

        var server_dropdown = new KeyserverDropdown();
        this.keyserver_publish.append(server_dropdown);

        this.keyserver_publish_to_label.set_mnemonic_widget(server_dropdown);

        AppSettings.instance().bind("server-auto-retrieve", this.auto_retrieve, "active",
                                    SettingsBindFlags.DEFAULT);
        AppSettings.instance().bind("server-auto-publish", this.auto_sync, "active",
                                    SettingsBindFlags.DEFAULT);
    }

    private class KeyServerRow : Adw.EntryRow {

        private unowned ServerSource server;

        public KeyServerRow(ServerSource server) {
            this.server = server;
            this.title = _("Server URL");
            this.text = server.uri;

            this.apply.connect(on_apply);

            var remove_button = new Gtk.Button.from_icon_name("list-remove-symbolic");
            remove_button.add_css_class("flat");
            remove_button.clicked.connect(on_remove_button_clicked);
            add_suffix(remove_button);
        }

        private void on_remove_button_clicked(Gtk.Button remove_button) {
            Pgp.Backend.get().remove_remote(this.server.uri);
        }

        private void on_apply() {
            var uri = this.text;

            // If nothing changed, just go away
            if (uri == server.uri)
                return;

            // Before doing anything, make sure we have a valid thing going on
            if (!ServerCategory.is_valid_uri(uri)) {
                add_css_class("error");
                return;
            }

            // We don't really have a way of ediing a remote, so simulate an edit
            // by adding and removing one
            Pgp.Backend.get().remove_remote(this.server.uri);
            Pgp.Backend.get().add_remote(uri, true);
            hide();
        }
    }

    private void on_remotes_changed(ListModel model,
                                    uint position, uint removed, uint added) {
        for (uint i = position; i < position + removed; i++) {
            unowned var row = this.ks_rows[i];
            this.keyservers_group.remove(row);
            this.ks_rows.remove_index(i);
        }

        for (uint i = position; i < position + added; i++) {
            var row = new KeyServerRow(model.get_item(i) as ServerSource);
            this.ks_rows.insert((int) i, row);
            this.keyservers_group.add(row);
        }
    }

    [GtkCallback]
    private void on_add_button_clicked(Gtk.Button button) {
        var dialog = new AddKeyserverDialog(get_root() as Gtk.Window);
        dialog.present();
    }
}
