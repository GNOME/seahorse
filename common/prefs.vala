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

    private bool updating_model;

    private Gtk.Notebook notebook;
    private Gtk.Grid keyserver_tab;
    private Gtk.TreeView keyservers;
    private Gtk.Box keyserver_publish;
    private Gtk.Label keyserver_publish_to_label;
    private Gtk.Button keyserver_remove;
    private Gtk.Button keyserver_add;
    private Gtk.CheckButton auto_retrieve;
    private Gtk.CheckButton auto_sync;

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
        this.updating_model = false;

        Gtk.Builder builder = new Gtk.Builder();
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
        this.keyserver_tab = (Gtk.Grid) builder.get_object("keyserver-tab");
        this.keyservers = (Gtk.TreeView) builder.get_object("keyservers");
        this.keyserver_publish = (Gtk.Box) builder.get_object("keyserver-publish");
        this.keyserver_publish_to_label = (Gtk.Label) builder.get_object("keyserver-publish-to-label");
        this.keyserver_remove = (Gtk.Button) builder.get_object("keyserver_remove");
        this.keyserver_add = (Gtk.Button) builder.get_object("keyserver_add");
        this.auto_retrieve = (Gtk.CheckButton) builder.get_object("auto_retrieve");
        this.auto_sync = (Gtk.CheckButton) builder.get_object("auto_sync");

        this.keyserver_remove.clicked.connect(on_prefs_keyserver_remove_clicked);
        this.keyserver_add.clicked.connect(on_prefs_keyserver_add_clicked);

#if WITH_KEYSERVER
        setup_keyservers();
#else
        remove_tab(this.keyserver_tab);
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

#if WITH_KEYSERVER

    private enum Columns {
        KEYSERVER,
        N_COLUMNS
    }

    // Perform keyserver page initialization
    private void setup_keyservers () {
        string[] keyservers = Seahorse.Servers.get_uris();
        populate_keyservers(keyservers);

        Gtk.TreeModel model = this.keyservers.get_model();
        model.row_changed.connect(keyserver_row_changed);
        model.row_deleted.connect(keyserver_row_deleted);

        Gtk.TreeSelection selection = this.keyservers.get_selection();
        selection.set_mode(Gtk.SelectionMode.SINGLE);
        selection.changed.connect(keyserver_sel_changed);

        Application.pgp_settings().changed["keyserver"].connect((settings, key) => {
            populate_keyservers(settings.get_strv(key));
        });

        KeyserverControl skc = new KeyserverControl("server-publish-to",
                                                    _("None: Don’t publish keys"));
        this.keyserver_publish.add(skc);
        this.keyserver_publish.show_all();

        this.keyserver_publish_to_label.set_mnemonic_widget(skc);

        Application.settings().bind("server-auto-retrieve", this.auto_retrieve, "active",
                                    SettingsBindFlags.DEFAULT);
        Application.settings().bind("server-auto-publish", this.auto_sync, "active",
                                    SettingsBindFlags.DEFAULT);
    }

    // Called when a cell has completed being edited
    private void keyserver_cell_edited (Gtk.CellRendererText cell, string? path, string? text, Gtk.TreeStore store) {
        if (!Servers.is_valid_uri(text)) {
            Util.show_error (null,
                             _("Not a valid Key Server address."),
                             _("For help contact your system administrator or the administrator of the key server." ));
            return;
        }

        Gtk.TreeIter iter;
        warn_if_fail(store.get_iter_from_string(out iter, path));
        store.set(iter, Columns.KEYSERVER, text, -1);
    }

    // The selection changed on the tree
    private void keyserver_sel_changed (Gtk.TreeSelection selection) {
        this.keyserver_remove.sensitive = (selection.count_selected_rows() > 0);
    }

    // User wants to remove selected rows
    private void on_prefs_keyserver_remove_clicked (Gtk.Widget? button) {
        this.keyservers.get_selection().selected_foreach((model, path, iter) => {
            ((Gtk.TreeStore) model).remove(ref iter);
        });
    }

    // Write key server list to settings
    private void save_keyservers (Gtk.TreeModel model) {
        string[] values = {};
        Gtk.TreeIter iter;
        if (model.get_iter_first(out iter)) {
            do {
                string? keyserver = null;
                model.get(iter, Columns.KEYSERVER, out keyserver, -1);
                if (keyserver == null)
                    return;
                values += keyserver;
            } while (model.iter_next(ref iter));
        }

        Application.pgp_settings().set_strv("keyservers", values);
    }

    private void keyserver_row_changed (Gtk.TreeModel model, Gtk.TreePath arg1, Gtk.TreeIter arg2) {
        // If currently updating (see populate_keyservers) ignore
        if (this.updating_model)
            return;

        save_keyservers(model);
    }

    private void keyserver_row_deleted (Gtk.TreeModel model, Gtk.TreePath arg1) {
        // If currently updating (see populate_keyservers) ignore
        if (this.updating_model)
            return;

        save_keyservers(model);
    }

    private void populate_keyservers(string[] keyservers) {
        Gtk.TreeStore store = (Gtk.TreeStore) this.keyservers.get_model();

        // This is our first time so create a store
        if (store == null) {
            store = new Gtk.TreeStore(1, typeof(string));
            this.keyservers.set_model(store);

            // Make the column
            Gtk.CellRendererText renderer = new Gtk.CellRendererText();
            renderer.editable = true;
            renderer.edited.connect((cell, path, text) => {
                keyserver_cell_edited(cell, path, text, store);
            });
            Gtk.TreeViewColumn column = new Gtk.TreeViewColumn.with_attributes(
                                                _("URL"), renderer,
                                                "text", Columns.KEYSERVER,
                                                null);
            this.keyservers.append_column(column);
        }

        // Mark this so we can ignore events
        this.updating_model = true;

        // We try and be inteligent about updating so we don't throw
        // away selections and stuff like that
        uint i = 0;
        Gtk.TreeIter iter;
        if (store.get_iter_first(out iter)) {
            bool cont = false;
            do {
                string? val;
                store.get(iter, Columns.KEYSERVER, out val, -1);
                if (keyservers[i] != null
                        && val != null
                        && keyservers[i].collate(val) == 0) {
                    cont = store.iter_next(ref iter);
                    i++;
                } else {
                    cont = store.remove(ref iter);
                }
            } while (cont);
        }

        // Any remaining extra rows
        for ( ; keyservers[i] != null; i++) {
            store.append(out iter, null);
            store.set(iter, Columns.KEYSERVER, keyservers[i], -1);
        }

        // Done updating
        this.updating_model = false;
    }

    private void on_prefs_keyserver_add_clicked (Gtk.Button button) {
        AddKeyserverDialog dialog = new AddKeyserverDialog(this);

        if (dialog.run() == Gtk.ResponseType.OK) {
            string? result = dialog.calculate_keyserver_uri();

            if (result != null) {
                Gtk.TreeStore store = (Gtk.TreeStore) this.keyservers.get_model();
                Gtk.TreeIter iter;
                store.append(out iter, null);
                store.set(iter, Columns.KEYSERVER, result, -1);
            }
        }

        dialog.destroy();
    }

#else

    private void on_prefs_keyserver_add_clicked (Gtk.Button? button) { }
    void on_prefs_keyserver_remove_clicked (Gtk.Widget? button) { }

#endif

}
