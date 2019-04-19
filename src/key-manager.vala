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

[GtkTemplate (ui = "/org/gnome/Seahorse/seahorse-key-manager.ui")]
public class Seahorse.KeyManager : Catalog {

    [GtkChild]
    private Gtk.SearchBar search_bar;
    [GtkChild]
    private Gtk.SearchEntry filter_entry;

    [GtkChild]
    private Gtk.Paned sidebar_panes;
    [GtkChild]
    private Gtk.ScrolledWindow sidebar_area;
    private Sidebar sidebar;
    [GtkChild]
    private Gtk.Stack content_stack;
    [GtkChild]
    private Gtk.TreeView key_list;

    [GtkChild]
    private Gtk.MenuButton new_item_button;
    [GtkChild]
    private Gtk.ToggleButton show_search_button;

    private Gcr.Collection collection;
    private KeyManagerStore store;

    private GLib.Settings settings;

    private enum DndTarget { // Drag 'n Drop target type
        PLAIN,
        URIS
    }

    private const GLib.ActionEntry[] action_entries = {
         { "new-item",           on_new_item                                                     },
         { "show-search",        on_show_search,                                                      },
         { "filter-items",       on_filter_items,              "s",                      "'any'" },
         { "focus-place",        on_focus_place,               "s",           "'secret-service'" },
         { "import-file",        on_import_file                                                  },
         { "combine-keyrings",   on_toggle_action,  null,  "false",  on_combine_keyrings_toggled },
         { "paste",              on_paste,                                                       },
    };

    public KeyManager(Application app) {
        GLib.Object(
            ui_name: "key-manager",
            application: app
        );
        this.settings = new GLib.Settings("org.gnome.seahorse.manager");

        set_events(Gdk.EventMask.POINTER_MOTION_MASK
                   | Gdk.EventMask.POINTER_MOTION_HINT_MASK
                   | Gdk.EventMask.BUTTON_PRESS_MASK
                   | Gdk.EventMask.BUTTON_RELEASE_MASK);

        this.collection = setup_sidebar();

        load_css();

        // Add new key store and associate it
        this.store = new KeyManagerStore(this.collection, this.key_list, this.settings);
        this.store.row_inserted.connect(on_store_row_inserted);
        this.store.row_deleted.connect(on_store_row_deleted);

        init_actions();

        // Notify us when settings change
        this.settings.changed["item-filter"].connect(on_item_filter_changed);
        on_item_filter_changed(this.settings, "item-filter");

        // For the filtering
        on_filter_changed(this.filter_entry);
        this.key_list.start_interactive_search.connect(() => {
            this.filter_entry.grab_focus();
            return false;
        });

        // Set focus to the current key list
        this.key_list.grab_focus();
        selection_changed();

        // Setup drops
        Gtk.drag_dest_set(this, Gtk.DestDefaults.ALL, {}, Gdk.DragAction.COPY);
        Gtk.TargetList targets = new Gtk.TargetList(null);
        targets.add_uri_targets(DndTarget.URIS);
        targets.add_text_targets(DndTarget.PLAIN);
        Gtk.drag_dest_set_target_list(this, targets);

        // In the beginning, nothing's selected, so show empty state
        check_empty_state();
    }

    private void init_actions() {
        add_action_entries (action_entries, this);
    }

    private void load_css() {
        Gtk.CssProvider provider = new Gtk.CssProvider();
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Display.get_default().get_default_screen(),
            provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION);

        provider.parsing_error.connect((section, error) => {
            uint start = section.get_start_line();
            uint end = section.get_end_line();
            if (start == end)
                debug("Error parsing css on line %u: %s", start, error.message);
            else
                debug("Error parsing css on lines %u-%u: %s", start, end, error.message);
        });

        provider.load_from_resource("/org/gnome/Seahorse/seahorse.css");
    }

    [GtkCallback]
    private void on_view_selection_changed(Gtk.TreeSelection selection) {
        Idle.add(() => {
            // Fire the signal
            selection_changed();
            // Return false so we don't get run again
            return false;
        });
    }

    public override void selection_changed() {
        base.selection_changed();

        var objects = get_selected_objects();
        foreach (weak Backend backend in get_backends())
            backend.actions.set_actions_for_selected_objects(objects);
    }

    [GtkCallback]
    private void on_key_list_row_activated(Gtk.TreeView key_list, Gtk.TreePath? path, Gtk.TreeViewColumn column) {
        if (path == null)
            return;

        GLib.Object obj = KeyManagerStore.get_object_from_path(key_list, path);
        if (obj != null)
            show_properties(obj);
    }

    private void on_store_row_inserted(Gtk.TreeModel store, Gtk.TreePath path, Gtk.TreeIter iter) {
        check_empty_state();
    }

    private void on_store_row_deleted(Gtk.TreeModel store, Gtk.TreePath path) {
        check_empty_state();
    }

    private void check_empty_state() {
        bool empty = (store.iter_n_children(null) == 0);

        this.show_search_button.sensitive = !empty;
        if (!empty) {
            this.content_stack.visible_child_name = "key_list_page";
            return;
        }

        // We have an empty page, that might still have 2 reasons:
        // - we really have no items in our collections
        // - we're dealing with a locked keyring
        Place? place = get_focused_place();
        if (place != null && place is Lockable && ((Lockable) place).unlockable) {
            this.content_stack.visible_child_name = "locked_keyring_page";
            return;
        }
        this.content_stack.visible_child_name = "empty_state_page";
    }

    [GtkCallback]
    private bool on_key_list_button_pressed(Gdk.EventButton event) {
        if (event.button == 3) {
            show_context_menu(event);
            GLib.List<GLib.Object> objects = get_selected_objects();
            if (objects.length() > 1) {
                return true;
            }
        }

        return false;
    }

    [GtkCallback]
    private bool on_key_list_popup_menu() {
        GLib.List<GLib.Object> objects = get_selected_objects();
        if (objects != null)
            show_context_menu(null);
        return false;
    }

    private void on_new_item(SimpleAction action, GLib.Variant? param) {
        this.new_item_button.activate();
    }

    private void on_toggle_action(SimpleAction action, GLib.Variant? param) {
        action.change_state(!action.state.get_boolean());
    }

    private void on_combine_keyrings_toggled(SimpleAction action, GLib.Variant? new_state) {
        bool combined = new_state.get_boolean();
        action.set_state(combined);

        this.sidebar.combined = combined;

        /* Don't show the sidebar if everyhing is combined */
        this.sidebar_area.visible = !combined;
        this.settings.set_boolean("sidebar-visible", !combined);
    }

    [GtkCallback]
    private void on_filter_changed(Gtk.Editable entry) {
        this.store.filter = this.filter_entry.text;
    }

    private void import_files(string[]? uris) {
        ImportDialog dialog = new ImportDialog(this);
        dialog.add_uris(uris);
        dialog.run();
        dialog.destroy();
    }

    private void on_import_file(SimpleAction action, GLib.Variant? parameter) {
        Gtk.FileChooserDialog dialog =
            new Gtk.FileChooserDialog(_("Import Key"), this,
                                      Gtk.FileChooserAction.OPEN,
                                      _("_Cancel"), Gtk.ResponseType.CANCEL,
                                      _("_Open"), Gtk.ResponseType.ACCEPT,
                                      null);

        dialog.set_default_response(Gtk.ResponseType.ACCEPT);
        dialog.set_local_only(false);

        // TODO: This should come from libgcr somehow
        Gtk.FileFilter filter = new Gtk.FileFilter();
        filter.set_name(_("All key files"));
        filter.add_mime_type("application/pgp-keys");
        filter.add_mime_type("application/x-ssh-key");
        filter.add_mime_type("application/pkcs12");
        filter.add_mime_type("application/pkcs12+pem");
        filter.add_mime_type("application/pkcs7-mime");
        filter.add_mime_type("application/pkcs7-mime+pem");
        filter.add_mime_type("application/pkcs8");
        filter.add_mime_type("application/pkcs8+pem");
        filter.add_mime_type("application/pkix-cert");
        filter.add_mime_type("application/pkix-cert+pem");
        filter.add_mime_type("application/pkix-crl");
        filter.add_mime_type("application/pkix-crl+pem");
        filter.add_mime_type("application/x-pem-file");
        filter.add_mime_type("application/x-pem-key");
        filter.add_mime_type("application/x-pkcs12");
        filter.add_mime_type("application/x-pkcs7-certificates");
        filter.add_mime_type("application/x-x509-ca-cert");
        filter.add_mime_type("application/x-x509-user-cert");
        filter.add_mime_type("application/pkcs10");
        filter.add_mime_type("application/pkcs10+pem");
        filter.add_mime_type("application/x-spkac");
        filter.add_mime_type("application/x-spkac+base64");
        filter.add_pattern("*.asc");
        filter.add_pattern("*.gpg");
        filter.add_pattern("*.pub");
        filter.add_pattern("*.pfx");
        filter.add_pattern("*.cer");
        filter.add_pattern("*.crt");
        filter.add_pattern("*.pem");
        dialog.add_filter(filter);
        dialog.set_filter(filter);

        filter = new Gtk.FileFilter();
        filter.set_name(_("All files"));
        filter.add_pattern("*");
        dialog.add_filter(filter);

        string? uri = null;
        if (dialog.run() == Gtk.ResponseType.ACCEPT) {
            uri = dialog.get_uri();
        }

        dialog.destroy();

        if (uri != null) {
            import_files({ uri });
        }
    }

    private void import_text(string? display_name, string? text) {
        ImportDialog dialog = new ImportDialog(this);
        dialog.add_text(display_name, text);
        dialog.run();
        dialog.destroy();
    }

    [GtkCallback]
    private void on_drag_data_received(Gdk.DragContext context, int x, int y,
                                       Gtk.SelectionData? selection_data, uint info, uint time) {
        if (selection_data == null)
            return;

        if (info == DndTarget.PLAIN) {
            string? text = selection_data.get_text();
            import_text(_("Dropped text"), text);
        } else if (info == DndTarget.URIS) {
            string[]? uris = selection_data.get_uris();
            foreach (string uri in uris)
                uri._strip();
            import_files(uris);
        }
    }

    private void on_paste(SimpleAction action, Variant? param) {
        Gdk.Atom atom = Gdk.Atom.intern("CLIPBOARD", false);
        Gtk.Clipboard clipboard = Gtk.Clipboard.get(atom);

        if (clipboard.wait_is_text_available())
            return;

        clipboard.request_text(on_clipboard_received);
    }

    private void on_clipboard_received(Gtk.Clipboard board, string? text) {
        if (text == null)
            return;

        assert(this.filter_entry != null);
        if (this.filter_entry.is_focus)
            this.filter_entry.paste_clipboard();
        else
            if (text != null && text.char_count() > 0)
                import_text(_("Clipboard text"), text);
    }

    private void update_view_filter(string filter_str, bool update_settings = true) {
        // Update the setting
        if (update_settings)
            this.settings.set_string("item-filter", filter_str);

        // Update the action
        SimpleAction action = lookup_action("filter-items") as SimpleAction;
        action.set_state(filter_str);

        // Update the store
        this.store.showfilter = KeyManagerStore.ShowFilter.from_string(filter_str);
        this.store.refilter();
    }

    private void on_show_search(SimpleAction action, Variant? param) {
        this.show_search_button.active = true;
    }

    private void on_filter_items(SimpleAction action, Variant? param) {
        update_view_filter (param.get_string());
    }

    private void on_item_filter_changed(GLib.Settings settings, string? key) {
        update_view_filter(settings.get_string("item-filter"), false);
    }

    public override GLib.List<GLib.Object> get_selected_objects() {
        return KeyManagerStore.get_selected_objects(this.key_list);
    }

    private void on_focus_place(SimpleAction action, Variant? param) {
        string? uri_prefix = param.get_string();
        if (uri_prefix != null) {
            this.sidebar.set_focused_place(uri_prefix);
        }
    }

    public override Place? get_focused_place() {
        return this.sidebar.get_focused_place();
    }

    private Gcr.Collection setup_sidebar() {
        this.sidebar = new Sidebar();
        sidebar.hexpand = true;

        /* Make sure we update the empty state on any change */
        this.sidebar.get_selection().changed.connect((sel) => check_empty_state());
        this.sidebar.current_collection_changed.connect (() => check_empty_state ());

        this.sidebar_panes.position = this.settings.get_int("sidebar-width");
        this.sidebar_panes.realize.connect(() =>   { this.sidebar_panes.position = this.settings.get_int("sidebar-width"); });
        this.sidebar_panes.unrealize.connect(() => { this.settings.set_int("sidebar-width", this.sidebar_panes.position);  });

        foreach (weak Backend backend in get_backends()) {
            ActionGroup actions = backend.actions;
            actions.catalog = this;
            insert_action_group(actions.prefix, actions);
        }

        this.sidebar_area.add(this.sidebar);
        this.sidebar.show();

        this.settings.bind("keyrings-selected", this.sidebar, "selected-uris", SettingsBindFlags.DEFAULT);

        return this.sidebar.collection;
    }

    public override List<weak Backend> get_backends() {
        return this.sidebar.get_backends();
    }

    [GtkCallback]
    private void on_popover_grab_notify(Gtk.Widget widget, bool was_grabbed) {
        if (!was_grabbed)
            widget.hide();
    }

    [GtkCallback]
    private void on_locked_keyring_unlock_button_clicked(Gtk.Button unlock_button) {
        Lockable? place = get_focused_place() as Lockable;
        return_if_fail(place != null && place.unlockable);

        unlock_button.sensitive = false;
        place.unlock.begin(null, null, (obj, res) => {
            try {
                unlock_button.sensitive = true;
                place.unlock.end(res);
            } catch (GLib.Error e) {
                unlock_button.sensitive = true;
                Util.show_error(this, _("Couldn’t unlock keyring"), e.message);
            }
        });
    }

    [GtkCallback]
    private bool on_key_pressed(Gtk.Widget widget, Gdk.EventKey event) {
        return this.search_bar.handle_event(event);
    }
}
