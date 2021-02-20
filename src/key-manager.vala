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
    private unowned Hdy.Leaflet header;
    [GtkChild]
    private unowned Gtk.HeaderBar left_header;
    [GtkChild]
    private unowned Gtk.HeaderBar right_header;
    [GtkChild]
    private unowned Gtk.Revealer back_revealer;

    [GtkChild]
    private unowned Gtk.SearchBar search_bar;
    [GtkChild]
    private unowned Gtk.SearchEntry filter_entry;

    [GtkChild]
    private unowned Hdy.Leaflet content_box;
    [GtkChild]
    private unowned Gtk.ScrolledWindow sidebar_area;
    private Sidebar sidebar;
    [GtkChild]
    private unowned Gtk.Stack content_stack;
    [GtkChild]
    private unowned Gtk.ListBox item_listbox;

    [GtkChild]
    private unowned Gtk.MenuButton new_item_button;
    [GtkChild]
    private unowned Gtk.ToggleButton show_search_button;

    private Seahorse.ItemList item_list;
    private Gcr.Collection collection;

    private GLib.Settings settings;

    private enum DndTarget { // Drag 'n Drop target type
        PLAIN,
        URIS
    }

    private const GLib.ActionEntry[] action_entries = {
         { "new-item",         on_new_item            },
         { "show-search",      on_show_search,        },
         { "filter-items",     on_filter_items,       "s", "'any'" },
         { "focus-place",      on_focus_place,        "s", "'secret-service'" },
         { "import-file",      on_import_file          },
         { "paste",            on_paste,               },
    };

    public KeyManager(Application app) {
        GLib.Object(
            ui_name: "key-manager",
            application: app
        );
        this.settings = new GLib.Settings("org.gnome.seahorse.manager");

        this.collection = setup_sidebar();

        load_css();

        // Add new item list and bind our listbox to it
        this.item_list = new Seahorse.ItemList(this.collection);
        this.item_list.items_changed.connect((idx, removed, added) => check_empty_state());
        this.item_listbox.bind_model(this.item_list, (obj) => { return new KeyManagerItemRow(obj); });
        this.item_listbox.row_activated.connect(on_item_listbox_row_activated);
        this.item_listbox.selected_rows_changed.connect(on_item_listbox_selected_rows_changed);
        this.item_listbox.popup_menu.connect(on_item_listbox_popup_menu);
        this.item_listbox.button_press_event.connect(on_item_listbox_button_press_event);

        init_actions();

        // Notify us when settings change
        this.settings.changed["item-filter"].connect(on_item_filter_changed);
        on_item_filter_changed(this.settings, "item-filter");

        // For the filtering
        on_filter_changed(this.filter_entry);

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

        // Makes sure it's visible when people use nightlies
        if (Config.PROFILE == "development")
            get_style_context().add_class("devel");
    }

    private void on_item_listbox_row_activated(Gtk.ListBox item_listbox, Gtk.ListBoxRow row) {
        unowned GLib.Object obj = ((KeyManagerItemRow) row).object;
        assert(obj != null);
        show_properties(obj);
    }

    private void on_item_listbox_selected_rows_changed(Gtk.ListBox item_listbox) {
        selection_changed();
    }

    private bool on_item_listbox_button_press_event (Gdk.EventButton event) {
        // Check for right click
        if ((event.type == Gdk.EventType.BUTTON_PRESS) && (event.button == 3)) {
            // Make sure that the right-clicked row is also selected
            var row = this.item_listbox.get_row_at_y((int) event.y);
            if (row != null) {
                this.item_listbox.unselect_all();
                this.item_listbox.select_row(row);
            }

            // Show context menu (unless no row was right clicked or nothing was selected)
            var objects = get_selected_item_listbox_items();
            debug("We have %u selected objects", objects.length());
            if (objects != null)
                show_context_menu(null);
            return true;
        }

        return false;
    }

    private bool on_item_listbox_popup_menu(Gtk.Widget? listview) {
        var objects = get_selected_item_listbox_items();
        if (objects != null)
            show_context_menu(null);
        return false;
    }

    private GLib.List<weak GLib.Object> get_selected_item_listbox_items() {
        var rows = this.item_listbox.get_selected_rows();
        var objects = new GLib.List<weak GLib.Object>();

        foreach (var row in rows)
            objects.prepend(((KeyManagerItemRow) row).object);

        return objects;
    }

    public override void selection_changed() {
        base.selection_changed();

        var objects = get_selected_objects();
        foreach (weak Backend backend in get_backends())
            backend.actions.set_actions_for_selected_objects(objects);
    }

    private void check_empty_state() {
        bool empty = (this.item_list.get_n_items() == 0);
        debug("Checking empty state: %s", empty.to_string());

        this.show_search_button.sensitive = !empty;
        if (!empty) {
            this.content_stack.visible_child_name = "item_listbox_page";
            return;
        }

        // We have an empty page, that might still have 2 reasons:
        // - we really have no items in our collections
        // - we're dealing with a locked keyring
        Place? place = this.sidebar.get_focused_place();
        if (place != null && place is Lockable && ((Lockable) place).unlockable) {
            this.content_stack.visible_child_name = "locked_keyring_page";
            return;
        }
        this.content_stack.visible_child_name = "empty_state_page";
    }

    private void on_new_item(SimpleAction action, GLib.Variant? param) {
        this.new_item_button.activate();
    }

    [GtkCallback]
    private void on_filter_changed(Gtk.Editable entry) {
        this.item_list.filter_text = this.filter_entry.text;
    }

    [GtkCallback]
    private void on_back_clicked() {
        show_sidebar_pane();
    }

    private void show_sidebar_pane() {
        this.content_box.visible_child_name = "sidebar-pane";
        update_header();
    }

    private void show_item_list_pane() {
        this.content_box.visible_child_name = "item-list-pane";
        update_header();
    }

    [GtkCallback]
    private void on_fold () {
        update_header();
    }

    private void update_header() {
        bool folded = this.content_box.folded;

        this.left_header.show_close_button =
            !folded || header.visible_child == left_header;

        this.right_header.show_close_button =
            !folded || header.visible_child == right_header;

        this.back_revealer.reveal_child = this.back_revealer.visible =
            folded && header.visible_child == right_header;
    }

    public void import_files(string[]? uris) {
        ImportDialog dialog = new ImportDialog(this);
        dialog.add_uris(uris);
        dialog.run();
        dialog.destroy();
    }

    private void on_import_file(SimpleAction action, GLib.Variant? parameter) {
        Gtk.FileChooserNative dialog =
            new Gtk.FileChooserNative(_("Import Key"), this,
                                      Gtk.FileChooserAction.OPEN,
                                      _("_Open"), _("_Cancel"));

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

        // Update the item list
        this.item_list.showfilter = ItemList.ShowFilter.from_string(filter_str);
        this.item_list.refilter();
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
        var objects = new GLib.List<GLib.Object>();

        foreach (var row in this.item_listbox.get_selected_rows()) {
            objects.append(((KeyManagerItemRow) row).object);
        }
        return objects;
    }

    private void on_focus_place(SimpleAction action, Variant? param) {
        string? uri_prefix = param.get_string();
        if (uri_prefix != null) {
            this.sidebar.set_focused_place_for_scheme(uri_prefix);
        }
    }

    private Gcr.Collection setup_sidebar() {
        this.sidebar = new Sidebar();

        restore_keyring_selection();

        // Make sure we update the empty state on any change
        this.sidebar.selected_rows_changed.connect(on_sidebar_selected_rows_changed);
        this.sidebar.current_collection_changed.connect((sidebar) => { check_empty_state (); });

        foreach (weak Backend backend in get_backends()) {
            ActionGroup actions = backend.actions;
            actions.catalog = this;
            insert_action_group(actions.prefix, actions);
        }

        this.sidebar_area.add(this.sidebar);
        this.sidebar.show();

        return this.sidebar.objects;
    }

    // On setup, select the LRU place from the user's last session (if any).
    private void restore_keyring_selection() {
        // Make sure we already get the setting, or it might accidentally be
        // changed meanwhile. If none is set (yet), then don't even bother
        var last_keyring = this.settings.get_strv("keyrings-selected");
        if (last_keyring == null || last_keyring.length == 0)
            return;

        // Since some backends take time to initialize, wait a bit beforehand.
        Timeout.add(150, () => {
            unowned string uri = last_keyring[0];

            debug("Selecting last used place %s", uri);
            if (this.sidebar.set_focused_place_for_uri(uri))
                return GLib.Source.REMOVE;

            // If that didn't work, try do a attempt by matching the scheme.
            // FIXME should do this poperly (not always getting proper URIs atm)
            var uri_scheme = uri.split(":")[0];
            if (this.sidebar.set_focused_place_for_scheme(uri_scheme))
                return GLib.Source.REMOVE;

            // Still nothing. Can happen on first run -> just select first entry
            this.sidebar.select_row(this.sidebar.get_row_at_index(0));
            return GLib.Source.REMOVE;
        });
    }

    private void on_sidebar_selected_rows_changed(Gtk.ListBox sidebar) {
        check_empty_state();

        show_item_list_pane();

        Place? place = this.sidebar.get_focused_place();
        return_if_fail (place != null);

        this.right_header.title = place.label;
        this.settings.set_strv("keyrings-selected", { place.uri });
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
        Lockable? place = this.sidebar.get_focused_place() as Lockable;
        return_if_fail(place != null && place.unlockable);

        unlock_button.sensitive = false;
        place.unlock.begin(null, null, (obj, res) => {
            try {
                unlock_button.sensitive = true;
                place.unlock.end(res);
                // Explicitly trigger an update of the main view
                check_empty_state();
            } catch (GLib.Error e) {
                unlock_button.sensitive = true;
                Util.show_error(this, _("Couldn’t unlock keyring"), e.message);
            }
        });
    }

    [GtkCallback]
    private bool on_key_pressed(Gtk.Widget widget, Gdk.EventKey event) {
        bool folded = this.content_box.folded;

        switch (event.keyval) {
          // <Alt>Down and <Alt>Up for easy sidebar navigation
          case Gdk.Key.Down:
              if (Gdk.ModifierType.MOD1_MASK in event.state && !folded) {
                  this.sidebar.select_next_place();
                  return true;
              }
              break;
          case Gdk.Key.Up:
              if (Gdk.ModifierType.MOD1_MASK in event.state && !folded) {
                  this.sidebar.select_previous_place();
                  return true;
              }
              break;

          // <Alt>Left to go back to sidebar in folded mode
          case Gdk.Key.Left:
              if (Gdk.ModifierType.MOD1_MASK in event.state && folded) {
                  show_sidebar_pane();
                  return true;
              }
              break;

          // <Alt>Right to open currently selected element in folded mode
          case Gdk.Key.Right:
              if (Gdk.ModifierType.MOD1_MASK in event.state && folded) {
                  show_item_list_pane();
                  return true;
              }
              break;
          default:
              break;
        }

        // By default: propagate to the search bar, for search-as-you-type
        return this.search_bar.handle_event(event);
    }
}
