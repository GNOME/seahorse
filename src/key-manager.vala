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

    [GtkChild] private unowned Adw.HeaderBar right_header;
    [GtkChild] private unowned Gtk.SearchEntry filter_entry;

    [GtkChild] private unowned Adw.NavigationSplitView content_box;
    [GtkChild] private unowned Gtk.ScrolledWindow sidebar_area;
    private Sidebar sidebar;

    [GtkChild] private unowned Gtk.Stack content_stack;
    [GtkChild] private unowned Gtk.ScrolledWindow item_listbox_page;
    [GtkChild] private unowned Adw.StatusPage empty_state_page;
    [GtkChild] private unowned Adw.StatusPage locked_keyring_page;

    [GtkChild] private unowned Gtk.ListBox item_listbox;

    [GtkChild] private unowned Gtk.MenuButton new_item_button;
    [GtkChild] private unowned Gtk.Popover new_item_menu_popover;
    [GtkChild] private unowned Gtk.ToggleButton show_search_button;

    [GtkChild] private unowned Gtk.DropTarget drop_target;

    private KeyManagerFilter item_filter = new KeyManagerFilter();
    private Gtk.FilterListModel item_list;

    private GLib.Settings settings;

    private const GLib.ActionEntry[] action_entries = {
         { "new-item", on_new_item },
         { "show-search", on_show_search },
         { "filter-items", on_filter_items, "s", "'any'" },
         { "focus-place", on_focus_place, "s", "'secret-service'" },
         { "import-file", on_import_file },
         { "paste", on_paste },

         { "lock-place", on_lock_place, "s" },
         { "unlock-place", on_unlock_place, "s" },
         { "delete-place", on_delete_place, "s" },
         { "view-place", on_view_place, "s" },
    };

    public KeyManager(Application app) {
        GLib.Object(
            ui_name: "key-manager",
            application: app
        );
        this.settings = new GLib.Settings("org.gnome.seahorse.manager");

        setup_sidebar();

        this.item_list = new Gtk.FilterListModel(null, this.item_filter);
        this.sidebar.selection.bind_property("selected-item",
                                             this.item_list, "model",
                                             GLib.BindingFlags.SYNC_CREATE);

        // Makes sure it's visible when people use nightlies
        if (Config.PROFILE == "development")
            add_css_class("devel");

        // Add new item list and bind our listbox to it
        this.item_list.items_changed.connect((idx, removed, added) => check_empty_state());
        this.item_listbox.bind_model(this.item_list, (obj) => {
            unowned var item = (Item) obj;
            return new KeyManagerItemRow(item);
        });

        init_actions();

        // Notify us when settings change
        this.settings.changed["item-filter"].connect(on_item_filter_changed);
        on_item_filter_changed(this.settings, "item-filter");

        // For the filtering
        on_filter_changed(this.filter_entry);

        // DnD
        this.drop_target.drop.connect(on_drop);

        // In the beginning, nothing's selected, so show empty state
        check_empty_state();
    }

    private void init_actions() {
        add_action_entries (action_entries, this);
    }

    [GtkCallback]
    private void on_item_listbox_row_activated(Gtk.ListBox item_listbox, Gtk.ListBoxRow row) {
        unowned var item = ((KeyManagerItemRow) row).item;
        item.view(this);
    }

    [GtkCallback]
    private void on_item_listbox_selected_rows_changed(Gtk.ListBox item_listbox) {
        selection_changed();
    }

    [GtkCallback]
    private void on_new_item_menu_item_clicked(Gtk.Button button) {
        // We have to do this manually, or the popover doesn't recognize that
        // the AdwDialog should be given focus instead.
        this.new_item_menu_popover.popdown();
    }

    [GtkCallback]
    private void on_item_listbox_long_pressed(Gtk.GestureLongPress gesture,
                                              double x, double y) {
        show_item_context_menu(x, y);
    }
    [GtkCallback]
    private void on_item_listbox_right_click(Gtk.GestureClick gesture,
                                             int n_press,
                                             double x,
                                             double y) {
        show_item_context_menu(x, y);
    }

    private void show_item_context_menu(double x, double y) {
        // Make sure that the right-clicked row is also selected
        var row = this.item_listbox.get_row_at_y((int) y);
        if (row != null) {
            this.item_listbox.unselect_all();
            this.item_listbox.select_row(row);
        }

        // Show context menu (unless no row was right clicked or nothing was selected)
        var objects = get_selected_items();
        debug("We have %u selected objects", objects.length());
        if (objects != null)
            show_context_menu((int) x, (int) y);
    }

    public void show_context_menu(int x, int y) {
        var menu = new Gtk.PopoverMenu.from_model(this.context_menu);
        menu.insert_action_group("win", this);
        var backends = Backend.get_registered();
        for (uint i = 0; i < backends.get_n_items(); i++) {
            var backend = (Backend) backends.get_item(i);
            menu.insert_action_group(backend.actions.prefix, backend.actions);
        }

        menu.set_parent(this.item_listbox);
        if (x != -1 && y != -1)
            menu.set_pointing_to({ x, y, 1, 1 });
        menu.popup();
    }

    public override void selection_changed() {
        base.selection_changed();

        var items = get_selected_items();
        var backends = Backend.get_registered();
        for (uint i = 0; i < backends.get_n_items(); i++) {
            var backend = (Backend) backends.get_item(i);
            backend.actions.set_actions_for_selected_objects(items);
        }
    }

    private void check_empty_state() {
        bool empty = (this.item_list.get_n_items() == 0);
        debug("Checking empty state: %s", empty.to_string());

        if (!empty) {
            this.content_stack.visible_child = this.item_listbox_page;
            return;
        }

        // We have an empty page, that might still have 2 reasons:
        // - we really have no items in our collections
        // - we're dealing with a locked keyring
        var place = (Place) this.sidebar.selection.selected_item;
        if (place != null && (place is Lockable) && ((Lockable) place).unlockable) {
            this.content_stack.visible_child = this.locked_keyring_page;
            return;
        }
        this.content_stack.visible_child = this.empty_state_page;
    }

    private void on_new_item(SimpleAction action, GLib.Variant? param) {
        this.new_item_button.activate();
    }

    [GtkCallback]
    private void on_filter_changed(Gtk.Editable entry) {
        this.item_filter.filter_text = this.filter_entry.text;
    }

    public async void import_file(File file) {
        try {
            var file_stream = yield file.read_async();
            var dialog = new ImportDialog(file_stream, file.get_basename(), this);
            dialog.present();
        } catch (GLib.Error err) {
            Util.show_error(this, _("Couldn't open file"), err.message);
        }
    }

    private void on_import_file(SimpleAction action, GLib.Variant? parameter) {
        var dialog = new Gtk.FileDialog();
        dialog.title = _("Import Key");

        var filters = new GLib.ListStore(typeof(Gtk.FileFilter));

        var filter = new Gtk.FileFilter();
        filter.name = _("All key files");
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
        filters.append(filter);

        var all_filter = new Gtk.FileFilter();
        all_filter.name = _("All files");
        all_filter.add_pattern("*");
        filters.append(all_filter);

        dialog.filters = filters;
        dialog.default_filter = filter;

        dialog.open.begin(this, null, (obj, res) => {
            try {
                var file = dialog.open.end (res);
                if (file != null) {
                    import_file.begin(file);
                }
            } catch (Error e) {
                warning("Problem importing key: %s", e.message);
            }
        });
    }

    private void import_text(string? display_name, string? text) {
        if (text == null) {
            return; // FIXME should we show something here?
        }
        var data_stream = new MemoryInputStream.from_data(text.data);
        var dialog = new ImportDialog(data_stream, display_name, this);
        dialog.present();
    }

    private bool on_drop(Gtk.DropTarget drop_target,
                         GLib.Value? value,
                         double x,
                         double y) {
        if (value.holds(typeof(string))) {
            import_text(_("Dropped text"), value.get_string());
            return true;
        }
        if (value.holds(typeof(GLib.Uri))) {
            var uri = (GLib.Uri) value.get_boxed();
            import_file.begin(File.new_for_uri(uri.to_string()));
            return true;
        }

        return false;
    }

    private void on_paste(SimpleAction action, Variant? param) {
        var clipboard = get_clipboard();
        clipboard.read_text_async.begin(null, (obj, res) => {
            try {
                var text = clipboard.read_text_async.end(res);

                if (this.filter_entry.is_focus())
                    this.filter_entry.text = text;
                else
                    if (text != null && text.char_count() > 0)
                        import_text(_("Clipboard text"), text);
            } catch (GLib.Error err) {
                warning("Couldn't read clipboard: %s", err.message);
            }
        });
    }

    private void update_view_filter(string filter_str, bool update_settings = true) {
        // Update the setting
        if (update_settings)
            this.settings.set_string("item-filter", filter_str);

        // Update the action
        SimpleAction action = lookup_action("filter-items") as SimpleAction;
        action.set_state(filter_str);

        // Update the item list
        this.item_filter.show_filter = KeyManagerFilter.ShowFilter.from_string(filter_str);
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

    public override GLib.List<Seahorse.Item> get_selected_items() {
        var items = new GLib.List<Seahorse.Item>();

        foreach (var row in this.item_listbox.get_selected_rows()) {
            items.append(((KeyManagerItemRow) row).item);
        }
        return items;
    }

    private void on_focus_place(SimpleAction action, Variant? param) {
        string? uri_prefix = param.get_string();
        if (uri_prefix != null) {
            this.sidebar.set_focused_place_for_scheme(uri_prefix);
        }
    }

    private void on_lock_place(SimpleAction action, Variant? param) {
        string? uri = param.get_string();
        return_if_fail(uri != null);

        uint pos;
        var place = this.sidebar.lookup_place_for_uri(uri, out pos) as Lockable;
        return_if_fail(place != null && place.lockable);

        place.lock.begin(null, null, (obj, res) => {
            try {
                place.lock.end(res);
                check_empty_state();
            } catch (Error e) {
                Util.show_error(this, _("Couldn’t lock"), e.message);
            }
        });
    }

    private void on_unlock_place(SimpleAction action, Variant? param) {
        string? uri = param.get_string();
        return_if_fail(uri != null);

        uint pos;
        var place = this.sidebar.lookup_place_for_uri(uri, out pos) as Lockable;
        return_if_fail(place != null && place.unlockable);

        var interaction = new Interaction(this);
        place.unlock.begin(interaction, null, (obj, res) => {
            try {
                place.unlock.end(res);
                check_empty_state();
            } catch (Error e) {
                Util.show_error(this, _("Couldn’t unlock"), e.message);
            }
        });
    }

    private void on_delete_place(SimpleAction action, Variant? param) {
        string? uri = param.get_string();
        return_if_fail(uri != null);

        uint pos;
        var place = this.sidebar.lookup_place_for_uri(uri, out pos) as Deletable;
        return_if_fail(place != null && place.deletable);

        var delete_op = place.create_delete_operation();
        delete_op.execute_interactively.begin(this, null, (obj, res) => {
            try {
                delete_op.execute_interactively.end(res);
            } catch (GLib.IOError.CANCELLED e) {
                debug("Deletion of place '%s' cancelled by user", uri);
            } catch (GLib.Error e) {
                var label = ((Place) place).label;
                Util.show_error(this,
                                _("Couldn’t delete '%s'").printf(label),
                                e.message);
            }
        });
    }

    private void on_view_place(SimpleAction action, Variant? param) {
        string? uri = param.get_string();
        return_if_fail(uri != null);

        uint pos;
        var place = this.sidebar.lookup_place_for_uri(uri, out pos) as Viewable;
        return_if_fail(place != null);
        place.view(this);
    }

    private void setup_sidebar() {
        this.sidebar = new Sidebar();

        restore_keyring_selection();

        // Make sure we update the empty state on any change
        this.sidebar.selection.selection_changed.connect(on_sidebar_selection_changed);

        var backends = Backend.get_registered();
        for (uint i = 0; i < backends.get_n_items(); i++) {
            var backend = (Backend) backends.get_item(i);
            ActionGroup actions = backend.actions;
            actions.catalog = this;
            insert_action_group(actions.prefix, actions);
        }

        this.sidebar_area.set_child(this.sidebar);
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
            uint pos;
            var place = this.sidebar.lookup_place_for_uri(uri, out pos);
            if (place != null) {
                this.sidebar.selection.selected = pos;
                return GLib.Source.REMOVE;
            }

            // If that didn't work, try do a attempt by matching the scheme.
            // FIXME should do this poperly (not always getting proper URIs atm)
            var uri_scheme = uri.split(":")[0];
            if (this.sidebar.set_focused_place_for_scheme(uri_scheme))
                return GLib.Source.REMOVE;

            // Still nothing. Can happen on first run -> just select first entry
            this.sidebar.selection.selected = 0;
            return GLib.Source.REMOVE;
        });
    }

    private void on_sidebar_selection_changed(Gtk.SelectionModel selection,
                                              uint position,
                                              uint n_items) {
        check_empty_state();

        this.content_box.show_content = true;

        var place = (Place) this.sidebar.selection.selected_item;
        return_if_fail (place != null);

        this.right_header.title_widget = new Adw.WindowTitle(place.label, "");
        this.settings.set_strv("keyrings-selected", { place.uri });
    }

    [GtkCallback]
    private void on_locked_keyring_unlock_button_clicked(Gtk.Button unlock_button) {
        unowned var place = this.sidebar.selection.selected_item as Lockable;
        return_if_fail(place != null && place.unlockable);

        activate_action("unlock-place", new Variant.string(((Place) place).uri));
    }

    [GtkCallback]
    private bool on_key_pressed(Gtk.EventControllerKey controller,
                                uint keyval,
                                uint keycode,
                                Gdk.ModifierType state) {
        bool folded = this.content_box.collapsed;

        switch (keyval) {
          // <Alt>Down and <Alt>Up for easy sidebar navigation
          case Gdk.Key.Down:
              if (Gdk.ModifierType.ALT_MASK in state && !folded) {
                  this.sidebar.select_next_place();
                  return true;
              }
              break;
          case Gdk.Key.Up:
              if (Gdk.ModifierType.ALT_MASK in state && !folded) {
                  this.sidebar.select_previous_place();
                  return true;
              }
              break;

          // <Alt>Left to go back to sidebar in folded mode
          case Gdk.Key.Left:
              if (Gdk.ModifierType.ALT_MASK in state && folded) {
                  this.content_box.show_content = false;
                  return true;
              }
              break;

          // <Alt>Right to open currently selected element in folded mode
          case Gdk.Key.Right:
              if (Gdk.ModifierType.ALT_MASK in state && folded) {
                  this.content_box.show_content = true;
                  return true;
              }
              break;
          default:
              break;
        }

        return false;
    }
}
