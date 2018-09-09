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

public class Seahorse.Prefs : Gtk.Dialog {

    private Gtk.Notebook notebook;

    /**
     * Create a new preferences window.
     *
     * @param parent The Gtk.Window to set as the preferences dialog's parent
     */
    public Prefs(Gtk.Window? parent, string? tabid = null) {
        GLib.Object (
            title: _("Preferences"),
            transient_for: parent,
            modal: true
        );

        var builder = new Gtk.Builder();
        try {
            string path = "/org/gnome/Seahorse/seahorse-prefs.ui";
            builder.add_from_resource(path);
        } catch (GLib.Error err) {
            GLib.critical("%s", err.message);
        }
        Gtk.Box content = (Gtk.Box) builder.get_object("prefs");
        get_content_area().border_width = 0;
        get_content_area().add(content);
        this.notebook = (Gtk.Notebook) builder.get_object("notebook");

#if WITH_KEYSERVER
        var keyservers = new Keyservers(this);
        var label = new Gtk.Label("Keyservers");
        add_tab(label, keyservers.keyserver_tab);
#endif

        if (tabid != null) {
            Gtk.Widget? tab = builder.get_object(tabid) as Gtk.Widget;
            if (tab != null)
                select_tab(tab);
        }
    }

    public static bool available() {
#if WITH_KEYSERVER
        return true;
#else
        return false;
#endif
    }

    /**
     * Add a tab to the preferences window
     *
     * @param label Label for the tab to be added
     * @param tab Tab to be added
     */
    public void add_tab(Gtk.Widget? label, Gtk.Widget? tab) {
        label.show();
        this.notebook.prepend_page(tab, label);
    }

    /**
     * Sets the input tab to be the active one
     *
     * @param tab The tab to be set active
     */
    public void select_tab(Gtk.Widget? tab) {
        int pos = this.notebook.page_num(tab);
        if (pos != -1)
            this.notebook.set_current_page(pos);
    }

    /**
     * Removes a tab from the preferences window
     *
     * @tab: The tab to be removed
     */
    public void remove_tab(Gtk.Widget? tab) {
        int pos = this.notebook.page_num(tab);
        if (pos != -1)
            this.notebook.remove_page(pos);
    }

}

#if WITH_KEYSERVER
private class Seahorse.Keyservers {

    private bool updating_model;
    private Prefs prefs_parent;

    public Gtk.Grid keyserver_tab;
    private Gtk.ListBox keyservers_list;
    private GLib.ListStore store;
    private Gtk.Box keyserver_publish;
    private Gtk.Label keyserver_publish_to_label;
    private Gtk.CheckButton auto_retrieve;
    private Gtk.CheckButton auto_sync;

    public Keyservers(Gtk.Window? parent) {
        this.prefs_parent = (Prefs) parent;

        var builder = new Gtk.Builder();
        try {
            string path = "/org/gnome/Seahorse/seahorse-prefs-keyservers.ui";
            builder.add_from_resource(path);
        } catch (GLib.Error err) {
            GLib.critical("%s", err.message);
        }

        this.keyserver_tab = (Gtk.Grid) builder.get_object("keyserver-tab");
        this.keyservers_list = (Gtk.ListBox) builder.get_object("keyservers");
        this.keyserver_publish = (Gtk.Box) builder.get_object("keyserver-publish");
        this.keyserver_publish_to_label = (Gtk.Label) builder.get_object("keyserver-publish-to-label");
        this.auto_retrieve = (Gtk.CheckButton) builder.get_object("auto_retrieve");
        this.auto_sync = (Gtk.CheckButton) builder.get_object("auto_sync");
        this.updating_model = false;

        this.store = new ListStore(typeof(GLib.Object));
        this.keyservers_list.bind_model(store, create_keyserver_row);

        string[] keyservers = Seahorse.Servers.get_uris();
        populate_keyservers(keyservers);
        insert_add_button();

        this.store.items_changed.connect(keyserver_row_changed);

        PgpSettings.instance().changed["keyserver"].connect((settings, key) => {
            populate_keyservers(settings.get_strv(key));
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

    private int listbox_comparator(GLib.Object a, GLib.Object b) {
        string? ks_a = a.get_data("keyserver"),
                ks_b = b.get_data("keyserver");
        if (ks_a == null)
            return 1;
        else if (ks_b == null)
            return -1;
        else
            return ks_a.collate(ks_b);
    }

    private GLib.Object get_keyserver_obj(string uri) {
        GLib.Object obj = new GLib.Object();
        obj.set_data("keyserver", uri);
        return obj;
    }

    private Gtk.ListBoxRow create_keyserver_row(GLib.Object item) {
        var row = new Gtk.ListBoxRow();

        var box = new Gtk.Box(Gtk.Orientation.HORIZONTAL, 2);
        row.add(box);

        string? ks = item.get_data("keyserver");
        if (ks != null) {
            //  widget declaration
            var uri_label = new Gtk.Label(item.get_data("keyserver"));
            var uri_entry = new Gtk.Entry();
            var edit_button = new Gtk.Button.from_icon_name("document-edit-symbolic");
            var remove_button = new Gtk.Button.from_icon_name("list-remove");
            //  callbacks
            edit_button.clicked.connect(() => {
                uri_label.hide();
                uri_entry.set_text(uri_label.get_text());
                uri_entry.show_now();
                uri_entry.grab_focus();
            });
            uri_entry.activate.connect(() => {
                var uri = uri_entry.get_text();
                if (!Servers.is_valid_uri(uri)) {
                    uri_entry.get_style_context().add_class("error");
                    return;
                }
                this.store.remove(row.get_index());
                this.store.insert_sorted(get_keyserver_obj(uri), listbox_comparator);
                uri_entry.hide();
            });
            remove_button.clicked.connect(() => {
                this.store.remove(row.get_index());
            });
            //  styling
            remove_button.get_style_context().add_class("flat");
            edit_button.get_style_context().add_class("flat");
            //  packing
            box.pack_start(uri_entry);
            box.pack_start(uri_label, false);
            box.pack_end(remove_button, false);
            box.pack_end(edit_button, false);

            uri_entry.set_no_show_all(true);
            uri_entry.hide();
        }
        else {
            box.pack_start(item.get_data("add_button"));
        }

        row.show_all();
        return row;
    }

    // Write key server list to settings
    private void save_keyservers () {
        string[] values = {};
        uint i = 0;
        GLib.Object? item = this.store.get_object(i);
        if (item != null) {
            do {
                string keyserver = item.get_data("keyserver");
                values += keyserver;
                item = this.store.get_object(++i);
            } while (item != null);
        }

        PgpSettings.instance().keyservers = values;
    }

    private void keyserver_row_changed (uint position, uint removed, uint added) {
        // If currently updating (see populate_keyservers) ignore
        if (this.updating_model)
            return;
        save_keyservers();
    }

    private void populate_keyservers(string[] keyservers) {
        // Mark this so we can ignore events
        this.updating_model = true;

        // We try and be inteligent about updating so we don't throw
        // away selections and stuff like that
        uint i = 0, j = 0;
        GLib.Object? item = this.store.get_object(j);
        if (item != null) {
            do {
                string? val = item.get_data("keyserver");
                Gtk.Button? add_button = item.get_data("add_button");
                if (keyservers[i] != null
                        && val != null
                        && keyservers[i].collate(val) == 0) {
                    i++;
                    j++;
                }
                else if (add_button == null) {
                    this.store.remove(j);
                }
                item = this.store.get_object(j);
            } while (item != null);
        }

        // Any remaining extra rows
        for ( ; keyservers[i] != null; i++) {
            this.store.insert_sorted(get_keyserver_obj(keyservers[i]), listbox_comparator);
        }

        // Done updating
        this.updating_model = false;
    }

    private void insert_add_button() {
        var item = new GLib.Object();
        var button = new Gtk.Button.from_icon_name("list-add");
        button.clicked.connect(on_prefs_keyserver_add_clicked);
        button.get_style_context().add_class("flat");
        item.set_data("add_button", button);
        this.store.insert_sorted(item, listbox_comparator);
    }

    private void on_prefs_keyserver_add_clicked (Gtk.Button button) {
        AddKeyserverDialog dialog = new AddKeyserverDialog(this.prefs_parent);

        if (dialog.run() == Gtk.ResponseType.OK) {
            string? result = dialog.calculate_keyserver_uri();

            if (result != null) {
                var obj = new GLib.Object();
                obj.set_data("keyserver", result);
                this.store.insert_sorted(get_keyserver_obj(result), listbox_comparator);
            }
        }

        dialog.destroy();
    }
}
#endif
