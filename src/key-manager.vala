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
    private Gtk.Container menu_placeholder;
    [GtkChild]
    private Gtk.TreeView key_list;
    [GtkChild]
    private Gtk.Paned sidebar_panes;
    [GtkChild]
    private Gtk.Button import_button;
    [GtkChild]
    private Gtk.Button new_button;
    [GtkChild]
    private Gtk.Button new_item_button;
    [GtkChild]
    private Gtk.SearchEntry filter_entry;
    [GtkChild]
    private Gtk.Container sidebar_area;
    private Sidebar sidebar;

    private Gtk.ActionGroup view_actions;
    private Gtk.RadioAction show_action;

    private Gcr.Collection collection;
    private KeyManagerStore store;

    private GLib.Settings settings;
    private int sidebar_width;
    private uint sidebar_width_sig;

    private enum DndTarget { // Drag 'n Drop target type
        PLAIN,
        URIS
    }

    private const Gtk.ActionEntry[] GENERAL_ACTIONS = {
        // TRANSLATORS: The "Remote" menu contains key operations on remote systems.
        { "remote-menu", null, N_("_Remote") },
        { "new-menu", null, N_("_New") },
        { "app-quit", null, N_("_Quit"), "<control>Q", N_("Close this program") },
        { "file-new", null, N_("_New…"), "<control>N", N_("Create a new key or item") },
        { "file-import", null, N_("_Import…"), "<control>I", N_("Import from a file") },
        { "edit-import-clipboard", null, N_("_Paste"), "<control>V", N_("Import from the clipboard") }
    };

    private const Gtk.ToggleActionEntry[] SIDEBAR_ACTIONS = {
        { "view-sidebar", null, N_("By _Keyring"), null, N_("Show sidebar listing keyrings"), null, false },
    };

    private const Gtk.RadioActionEntry[] VIEW_RADIO_ACTIONS = {
        { "view-personal", null, N_("Show _Personal"), null, N_("Only show personal keys, certificates and passwords"), KeyManagerStore.ShowFilter.PERSONAL },
        { "view-trusted", null, N_("Show _Trusted"), null, N_("Only show trusted keys, certificates and passwords"), KeyManagerStore.ShowFilter.TRUSTED },
        { "view-any", null, N_("Show _Any"), null, N_("Show all keys, certificates and passwords"), KeyManagerStore.ShowFilter.ANY },
    };

    public KeyManager(Application app) {
        GLib.Object(
            ui_name: "key-manager",
            application: app
        );
        this.settings = new GLib.Settings("org.gnome.seahorse.manager");

        set_default_size(640, 476);
        set_events(Gdk.EventMask.POINTER_MOTION_MASK
                   | Gdk.EventMask.POINTER_MOTION_HINT_MASK
                   | Gdk.EventMask.BUTTON_PRESS_MASK
                   | Gdk.EventMask.BUTTON_RELEASE_MASK);

        this.collection = setup_sidebar();

        // Init key list & selection settings
        Gtk.TreeSelection selection = this.key_list.get_selection();
        selection.set_mode(Gtk.SelectionMode.MULTIPLE);
        selection.changed.connect(on_view_selection_changed);
        this.key_list.realize();

        // Add new key store and associate it
        this.store = new KeyManagerStore(this.collection, this.key_list, this.settings);

        init_actions();

        // Notify us when settings change
        this.settings.changed["item-filter"].connect(on_item_filter_changed);
        on_item_filter_changed(this.settings, "item-filter");

        // first time signals
        this.import_button.clicked.connect(on_keymanager_import_button);
        this.new_button.clicked.connect(on_keymanager_new_button);

        // Flush all updates
        ensure_updated();

        // The toolbar
        this.new_item_button.clicked.connect(on_keymanager_new_button);
        on_filter_changed(this.filter_entry);

        // For the filtering
        this.filter_entry.search_changed.connect(on_filter_changed);
        this.key_list.start_interactive_search.connect(() => {
            this.filter_entry.grab_focus();
            return false;
        });

        // Set focus to the current key list
        this.key_list.grab_focus();
        selection_changed();

        // To avoid flicker
        show();

        // Setup drops
        Gtk.drag_dest_set(this, Gtk.DestDefaults.ALL, {}, Gdk.DragAction.COPY);
        Gtk.TargetList targets = new Gtk.TargetList(null);
        targets.add_uri_targets(DndTarget.URIS);
        targets.add_text_targets(DndTarget.PLAIN);
        Gtk.drag_dest_set_target_list(this, targets);

        this.drag_data_received.connect(on_target_drag_data_received);
        this.key_list.button_press_event.connect(on_keymanager_key_list_button_pressed);
        this.key_list.row_activated.connect(on_keymanager_row_activated);
        this.key_list.popup_menu.connect(on_keymanager_key_list_popup_menu);
    }

    ~KeyManager() {
        if (this.sidebar_width_sig != 0) {
            Source.remove(this.sidebar_width_sig);
            this.sidebar_width_sig = 0;
        }
    }

    private void init_actions() {
        // General actions
        Gtk.ActionGroup actions = new Gtk.ActionGroup("general");
        actions.set_translation_domain(Config.GETTEXT_PACKAGE);
        actions.add_actions(GENERAL_ACTIONS, null);
        actions.get_action("app-quit").activate.connect(on_app_quit);
        actions.get_action("file-new").activate.connect(on_file_new);
        actions.get_action("file-import").activate.connect(on_key_import_file);
        actions.get_action("edit-import-clipboard").activate.connect(on_key_import_clipboard);
        include_actions(actions);

        // View actions
        this.view_actions = new Gtk.ActionGroup("view");
        this.view_actions.set_translation_domain(Config.GETTEXT_PACKAGE);
        this.view_actions.add_radio_actions(VIEW_RADIO_ACTIONS, -1, () => {
            this.settings.set_string("item-filter", update_view_filter());
        });
        this.show_action = (Gtk.RadioAction) this.view_actions.get_action("view-personal");
        include_actions(this.view_actions);

        // Make sure import is only available with clipboard content
        Gtk.Clipboard clipboard = Gtk.Clipboard.get(Gdk.SELECTION_PRIMARY);
        clipboard.owner_change.connect((c, e) => update_clipboard_state(c, e, actions));
        update_clipboard_state(clipboard, null, actions);
    }

    protected override void add_menu(Gtk.Widget menu) {
        this.menu_placeholder.add(menu);
        menu.show();
    }

    private void on_view_selection_changed(Gtk.TreeSelection selection) {
        Idle.add(() => {
            // Fire the signal
            selection_changed();
            // Return false so we don't get run again
            return false;
        });
    }

    private void on_keymanager_row_activated(Gtk.TreeView key_list, Gtk.TreePath? path, Gtk.TreeViewColumn column) {
        if (path == null)
            return;

        GLib.Object obj = KeyManagerStore.get_object_from_path(key_list, path);
        if (obj != null)
            show_properties(obj);
    }

    private bool on_keymanager_key_list_button_pressed(Gdk.EventButton event) {
        if (event.button == 3) {
            show_context_menu(Catalog.MENU_OBJECT, event);
            GLib.List<GLib.Object> objects = get_selected_objects();
            if (objects.length() > 1) {
                return true;
            }
        }

        return false;
    }

    private bool on_keymanager_key_list_popup_menu() {
        GLib.List<GLib.Object> objects = get_selected_objects();
        if (objects != null)
            show_context_menu(Catalog.MENU_OBJECT, null);
        return false;
    }

    private void on_file_new(Gtk.Action action) {
        GenerateSelect dialog = new GenerateSelect(this);
        dialog.run();
        dialog.destroy();
    }

    private void on_keymanager_new_button(Gtk.Button button) {
        GenerateSelect dialog = new GenerateSelect(this);
        dialog.run();
        dialog.destroy();
    }

    private void on_filter_changed(Gtk.Editable entry) {
        this.store.filter = this.filter_entry.text;
    }

    private void import_files(string[]? uris) {
        ImportDialog dialog = new ImportDialog(this);
        dialog.add_uris(uris);
        dialog.run();
        dialog.destroy();
    }

    private void import_prompt() {
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

    private void on_key_import_file(Gtk.Action action) {
        import_prompt();
    }

    private void on_keymanager_import_button(Gtk.Button button) {
        import_prompt();
    }

    private void import_text(string? display_name, string? text) {
        ImportDialog dialog = new ImportDialog(this);
        dialog.add_text(display_name, text);
        dialog.run();
        dialog.destroy();
    }

    private void on_target_drag_data_received(Gdk.DragContext context, int x, int y,
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

    private void update_clipboard_state(Gtk.Clipboard clipboard, Gdk.Event? event, Gtk.ActionGroup group) {
        Gtk.Action action = group.get_action("edit-import-clipboard");
        action.set_sensitive(clipboard.wait_is_text_available());
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

    private void on_key_import_clipboard(Gtk.Action action) {
        Gdk.Atom atom = Gdk.Atom.intern("CLIPBOARD", false);
        Gtk.Clipboard clipboard = Gtk.Clipboard.get(atom);
        clipboard.request_text(on_clipboard_received);
    }

    private void on_app_quit(Gtk.Action action) {
        this.application.quit();
    }

    private unowned string update_view_filter() {
        this.store.showfilter = (KeyManagerStore.ShowFilter) this.show_action.current_value;
        this.store.refilter();
        return this.store.showfilter.to_string();
    }

    private void on_item_filter_changed(GLib.Settings settings, string? key) {
        int radio = KeyManagerStore.ShowFilter.from_string(settings.get_string(key));
        this.show_action.set_current_value(radio);
        update_view_filter();
    }

    public override GLib.List<GLib.Object> get_selected_objects() {
        return KeyManagerStore.get_selected_objects(this.key_list);
    }

    public void set_focused_place(string target) {
        string? uri_prefix = null;
        switch(target) {
            case "ssh":
            case "pkcs11":
                uri_prefix = "openssh";
                break;
            case "gpgme":
                uri_prefix = "gnupg";
                break;
            case "gkr":
                uri_prefix = "secret-service";
                break;
        }
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

        this.sidebar_width = this.settings.get_int("sidebar-width");

        this.sidebar_panes.position = this.sidebar_width;
        this.sidebar_panes.realize.connect(() =>   { this.sidebar_panes.position = this.settings.get_int("sidebar-width"); });
        this.sidebar_panes.unrealize.connect(() => { this.settings.set_int("sidebar-width", this.sidebar_panes.position);  });

        this.sidebar_panes.get_child1().set_size_request(50, -1);
        this.sidebar_panes.get_child2().set_size_request(150, -1);

        foreach (weak Backend backend in get_backends()) {
            if (backend.actions != null)
                include_actions(backend.actions);
        }

        this.sidebar_area.add(this.sidebar);
        this.sidebar.show();

        Gtk.ActionGroup actions = new Gtk.ActionGroup("sidebar");
        actions.set_translation_domain(Config.GETTEXT_PACKAGE);
        actions.add_toggle_actions(SIDEBAR_ACTIONS, null);
        Gtk.Action action = actions.get_action("view-sidebar");
        this.settings.bind("sidebar-visible", action, "active", SettingsBindFlags.DEFAULT);
        action.bind_property("active", this.sidebar_area, "visible", BindingFlags.SYNC_CREATE);
        action.bind_property("active", this.sidebar, "combined", BindingFlags.INVERT_BOOLEAN | BindingFlags.SYNC_CREATE);
        include_actions(actions);

        this.settings.bind("keyrings-selected", this.sidebar, "selected-uris", SettingsBindFlags.DEFAULT);

        return this.sidebar.collection;
    }

    public override List<weak Backend> get_backends() {
        return this.sidebar.get_backends();
    }
}
