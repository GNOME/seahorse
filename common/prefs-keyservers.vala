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
public class Seahorse.PrefsKeyservers : Hdy.PreferencesPage {

    [GtkChild]
    public unowned Gtk.Grid keyserver_tab;
    [GtkChild]
    private unowned Gtk.ListBox keyservers_list;
    [GtkChild]
    private unowned Gtk.Box keyserver_publish;
    [GtkChild]
    private unowned Gtk.Label keyserver_publish_to_label;
    [GtkChild]
    private unowned Gtk.CheckButton auto_retrieve;
    [GtkChild]
    private unowned Gtk.CheckButton auto_sync;

    public PrefsKeyservers() {
        var model = Pgp.Backend.get().get_remotes();
        this.keyservers_list.bind_model(model, (item) => {
            return new KeyServerRow(item as ServerSource);
        });

        KeyserverControl skc = new KeyserverControl("server-publish-to",
                                                    _("None: Don’t publish keys"));
        this.keyserver_publish.add(skc);
        this.keyserver_publish.show_all();

        this.keyserver_publish_to_label.set_mnemonic_widget(skc);

        AppSettings.instance().bind("server-auto-retrieve", this.auto_retrieve, "active",
                                    SettingsBindFlags.DEFAULT);
        AppSettings.instance().bind("server-auto-publish", this.auto_sync, "active",
                                    SettingsBindFlags.DEFAULT);
    }

    private class KeyServerRow : Gtk.ListBoxRow {

        private unowned ServerSource server;
        private Gtk.Label label;
        private Gtk.Entry entry;

        public KeyServerRow(ServerSource server) {
            this.server = server;

            var box = new Gtk.Box(Gtk.Orientation.HORIZONTAL, 2);
            box.margin = 6;
            add(box);

            this.label = new Gtk.Label(server.uri);
            box.pack_start(this.label, false);

            this.entry = new Gtk.Entry();
            this.entry.set_no_show_all(true);
            this.entry.hide();
            this.entry.activate.connect(on_entry_activated);
            box.pack_start(this.entry);

            var remove_button = new Gtk.Button.from_icon_name("list-remove-symbolic");
            remove_button.get_style_context().add_class("flat");
            remove_button.clicked.connect(on_remove_button_clicked);
            box.pack_end(remove_button, false);

            var edit_button = new Gtk.Button.from_icon_name("document-edit-symbolic");
            edit_button.get_style_context().add_class("flat");
            edit_button.clicked.connect(on_edit_button_clicked);
            box.pack_end(edit_button, false);

            show_all();
        }

        private void on_edit_button_clicked(Gtk.Button edit_button) {
            this.label.hide();
            this.entry.set_text(this.label.get_text());
            this.entry.show_now();
            this.entry.grab_focus();
        }

        private void on_remove_button_clicked(Gtk.Button remove_button) {
            Pgp.Backend.get().remove_remote(this.server.uri);
        }

        private void on_entry_activated(Gtk.Entry entry) {
            var uri = this.entry.get_text();

            // If nothing changed, just go away
            if (uri == server.uri) {
                this.entry.hide();
                return;
            }

            // Before doing anything, make sure we have a valid thing going on
            if (!ServerCategory.is_valid_uri(uri)) {
                this.entry.get_style_context().add_class("error");
                return;
            }

            // We don't really have a way of ediing a remote, so simulate an edit
            // by adding and removing one
            Pgp.Backend.get().remove_remote(this.server.uri);
            Pgp.Backend.get().add_remote(uri, true);
            this.entry.hide();
        }
    }

    [GtkCallback]
    private void on_add_button_clicked(Gtk.Button button) {
        AddKeyserverDialog dialog = new AddKeyserverDialog(get_toplevel() as Gtk.Window);

        if (dialog.run() == Gtk.ResponseType.OK) {
            string? result = dialog.calculate_keyserver_uri();

            if (result != null)
                Pgp.Backend.get().add_remote(result, true);
        }

        dialog.destroy();
    }
}
