/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2017 Niels De Graef
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

namespace Seahorse {

public class KeyserverResults : Seahorse.Catalog {

    static const GtkActionEntry GENERAL_ENTRIES[] = {
        /* TRANSLATORS: The "Remote" menu contains key operations on remote systems. */
        { "remote-menu", null, N_("_Remote") },
        { "app-close", GTK_STOCK_CLOSE, null, "<control>W",
          N_("Close this window"), G_CALLBACK (on_app_close) },
    };

    static const GtkActionEntry SERVER_ENTRIES[] = {
        { "remote-find", GTK_STOCK_FIND, N_("_Find Remote Keys…"), "",
          N_("Search for keys on a key server"), G_CALLBACK (on_remote_find) }
    };

    static const GtkActionEntry IMPORT_ENTRIES[] = {
        { "key-import-keyring", GTK_STOCK_ADD, N_("_Import"), "",
          N_("Import selected keys to local key ring"), G_CALLBACK (on_key_import_keyring) }
    };

    public string search_string { get; set; default = ""; }

    private Seahorse.Predicate pred;
    private Gtk.TreeView view;
    private Gcr.SimpleCollection collection;
    private Gtk.ActionGroup import_actions;
    private Seahorse.KeyManagerStore store;
    private GLib.Settings settings;

    public KeyServerResults() {
        this.settings = new GLib.Settings("org.gnome.seahorse.manager");
        this.collection = new Gcr.SimpleCollection();

        load_ui();
    }

    private void load_ui() {
        string title = (this.search_string == "")? _("Remote Keys") :
                                                   _("Remote Keys Containing “%s”").printf(this.search_string);

        Gtk.Window window = get_window();
        window.set_default_geometry(640, 476);
        window.set_events(GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
        window.title = title;
        window.visible = true;

        window.delete_event.connect(on_delete_event);

        Gtk.ActionGroup actions = new Gtk.ActionGroup("general");
        actions.set_translation_domain(Config.GETTEXT_PACKAGE);
        actions.add_actions(GENERAL_ENTRIES, this);
        include_actions(actions);

        actions = new Gtk.ActionGroup("keyserver");
        actions.set_translation_domain(Config.GETTEXT_PACKAGE);
        actions.add_actions(SERVER_ENTRIES, this);
        include_actions(actions);

        this.import_actions = new Gtk.ActionGroup("import");
        this.import_actions.set_translation_domain(Config.GETTEXT_PACKAGE);
        this.import_actions.add_actions(IMPORT_ENTRIES, this);
        this.import_actions.get_action("key-import-keyring").is_important = true;
        include_actions(this.import_actions);

        /* init key list & selection settings */
        Gtk.Builder builder = get_builder();
        this.view = (Gtk.TreeView) builder.get_object("key_list"));
        Gtk.TreeSelection selection = this.view.get_selection();
        selection.set_mode(GTK_SELECTION_MULTIPLE);
        selection.changed.connect(on_view_selection_changed);
        this.view.row_activated.connect(on_row_activated);
        this.view.button_press_event.connect(on_key_list_button_pressed);
        this.view.popup_menu.connect(on_key_list_popup_menu);
        this.view.realize();

        /* Set focus to the current key list */
        this.view.grab_focus();

        /* To avoid flicker */
        ensure_updated();
        show();

        this.store = new Seahorse.KeyManagerStore(this.collection, this.view, out this.pred, this.settings);
        on_view_selection_changed (selection, self);

        // Include actions from the backend
        include_actions(Pgp.Backend.get().actions);
    }

    /* -----------------------------------------------------------------------------
     * INTERNAL
     */

    /**
    * selection:
    * self: the results object
    *
    * Adds fire_selection_changed as idle function
    *
    **/
    private void on_view_selection_changed (GtkTreeSelection *selection) {
        g_idle_add ((GSourceFunc)fire_selection_changed, self);
    }

    private bool fire_selection_changed() {
        Gtk.TreeSelection selection = this.view.get_selection();
        int rows = selection.count_selected_rows();
        if (this.import_actions)
            this.import_actions.set_sensitive(rows > 0);
        selection_changed();
        return false;
    }

    private void on_row_activated (Gtk.TreeView view, GtkTreePath *path, GtkTreeViewColumn *column) {
        g_return_if_fail (path != null);
        g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (column));

        GLib.Object obj = seahorse_key_manager_store_get_object_from_path (view, path);
        if (obj != null)
            show_properties(obj);
    }

    private bool on_key_list_button_pressed (Gtk.TreeView view, GdkEventButton* event) {
        if (event->button == 3)
            show_context_menu(SEAHORSE_CATALOG_MENU_OBJECT, event->button, event->time);
        return false;
    }

    private bool on_key_list_popup_menu (Gtk.TreeView view) {
        List<?> objects = get_selected_objects();
        if (objects != null)
            show_context_menu(SEAHORSE_CATALOG_MENU_OBJECT, 0, gtk_get_current_event_time ());
        return true;
    }

    /**
    * action: the closing action or null
    * self: The SeahorseKeyServerResults widget to destroy
    *
    * destroys the widget
    *
    **/
    private void on_app_close (Gtk.Action action) {
        g_return_if_fail (action == null || GTK_IS_ACTION (action));
        destroy();
    }

    private void on_remote_find (Gtk.Action action) {
        g_return_if_fail (GTK_IS_ACTION (action));
        seahorse_keyserver_search_show (get_window());
    }

    private void on_import_complete(GObject *source, GAsyncResult *result) {
        GError *error = null;

        if (!seahorse_pgp_backend_transfer_finish (SEAHORSE_PGP_BACKEND (source),
                                                   result, &error))
            seahorse_util_handle_error (&error, seahorse_catalog_get_window (self),
                                        _("Couldn’t import keys"));
    }

    static List<?> objects_prune_non_exportable (List<?> objects) {
        List<?> exportable = null;
        List<?> l;

        for (l = objects; l; l = g_list_next (l)) {
            if (seahorse_object_get_flags (l->data) & SEAHORSE_FLAG_EXPORTABLE)
                exportable = g_list_append (exportable, l->data);
        }

        return exportable;
    }

    private void on_key_import_keyring (Gtk.Action action) {
        List<?> objects = get_selected_objects();
        objects = objects_prune_non_exportable (objects);

        /* No objects, nothing to do */
        if (objects == null)
            return;

        Cancellable cancellable = new Cancellable();
        Pgp.Backend backend = Pgp.Backend.get();
        GpgME.Keyring keyring = backend.get_default_keyring();
        backend.transfer_async(objects, keyring, cancellable, on_import_complete, g_object_ref (self));
        Seahorse.Progress.show(cancellable, _ ("Importing keys from key servers"), true);
    }

    /**
    * widget: sending widget
    * event: ignored
    * self: The SeahorseKeyserverResults widget to destroy
    *
    * When this window closes we quit seahorse
    *
    * Returns true on success
    **/
    static bool on_delete_event (GtkWidget* widget, GdkEvent* event, SeahorseKeyserverResults* self) {
        g_return_val_if_fail (GTK_IS_WIDGET (widget), false);
        on_app_close (null, self);
        return true;
    }

    static List<?> get_selected_objects() {
        return seahorse_key_manager_store_get_selected_objects (this.view);
    }

    static Seahorse.Place get_focused_place() {
        return Pgp.Backend.get().get_default_keyring();
    }

    /**
     * Creates a search results window and adds the operation to it's progress status.
     *
     * @param search_text The text to search for
     * @param parent A GTK window as parent (or null)
     */
    public void show(string search_text, GtkWindow *parent) {
        g_return_if_fail (search_text != null);

        self = g_object_new (SEAHORSE_TYPE_KEYSERVER_RESULTS,
                             "ui-name", "keyserver-results",
                             "search", search_text,
                             "transient-for", parent,
                             null);

        Cancellable cancellable = new Cancellable();

        Pgp.Backend.get().search_remote_async.begin(null, search_text, this.collection, cancellable, (obj, res) => {
            try {
                Backend.get().search_remote_finish(res);
            } catch (GLib.Error e) {
                g_dbus_error_strip_remote_error (e);
                seahorse_util_show_error (this, _("The search for keys failed."), e);
            }
        });

        Seahorse.Progress.attach(cancellable, get_builder());
    }
}

}
