/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
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

public class Seahorse.Sidebar : Gtk.ListBox {

    private GLib.ListStore store = new GLib.ListStore(typeof(Seahorse.Place));
    private List<Backend> backends = new List<Backend>();

    // A set of chosen uris, used with settings
    private GenericSet<string?> chosen = new GenericSet<string?>(str_hash, str_equal);

    /**
     * Collection of objects sidebar represents
     */
    public Gcr.UnionCollection objects { get; private set; default = new Gcr.UnionCollection(); }

    /**
     * The URIs selected by the user
     */
    public string[] selected_uris {
        owned get { return chosen_uris_to_array(); }
        set { replace_chosen_uris(value); }
    }

    /**
     * Collection shows all objects combined
     */
    public bool combined {
        get { return this._combined; }
        set {
            if (this._combined != value) {
                this._combined = value;
                update_objects();
            }
        }
    }
    private bool _combined;

    /**
     * Emitted when the state of the current collection changed.
     * For example: when going from locked to unlocked and vice versa.
     */
    public signal void current_collection_changed();

    public Sidebar() {
        GLib.Object(selection_mode: Gtk.SelectionMode.BROWSE);

        bind_model(this.store, place_widget_create_cb);
        set_header_func(place_header_cb);
        this.selected_rows_changed.connect(update_objects);

        load_backends();
    }

    ~Sidebar() {
        foreach (Backend backend in this.backends) {
            SignalHandler.disconnect_by_func((void*) backend, (void*) on_place_added, this);
            SignalHandler.disconnect_by_func((void*) backend, (void*) on_place_removed, this);
            SignalHandler.disconnect_by_func((void*) backend, (void*) on_backend_changed, this);

            foreach (weak GLib.Object obj in backend.get_objects())
                on_place_removed (backend, (Place) obj);
        }
    }

    private void load_backends() {
        foreach (Backend backend in Backend.get_registered()) {
            this.backends.append(backend);
            backend.added.connect(on_place_added);
            backend.removed.connect(on_place_removed);
            backend.notify.connect(on_backend_changed);

            foreach (weak GLib.Object obj in backend.get_objects())
                on_place_added(backend, obj);
        }

        this.backends.sort((a, b) => {
            int ordera = order_from_backend((Backend) a);
            int orderb = order_from_backend((Backend) b);
            return ordera - orderb;
        });
    }

    private void on_place_added(Gcr.Collection? places, GLib.Object place) {
        place.notify.connect(() => update_places());
        update_places();
    }

    private void on_place_removed(Gcr.Collection? places, GLib.Object place) {
        SignalHandler.disconnect_by_func((void*) place, (void*) update_places, this);
        update_places();
    }

    private void on_backend_changed(GLib.Object obj, ParamSpec spec) {
        update_places();
    }

    private Gtk.Widget place_widget_create_cb(GLib.Object object) {
        var item = new SidebarItem(object as Seahorse.Place);
        item.place_changed.connect(on_sidebar_item_changed);
        return item;
    }

    private void on_sidebar_item_changed(SidebarItem item) {
        /* current_collection_changed(); */
    }

    private void place_header_cb(Gtk.ListBoxRow row, Gtk.ListBoxRow? before) {
        Seahorse.Place place = ((SidebarItem) row).place;
        string scheme = Uri.parse_scheme(place.uri);

        // We don't need a title iff
        // * there is no previous row
        // * the previous row is from another backend
        if (before != null) {
            Seahorse.Place before_place = ((SidebarItem) before).place;
            if (Uri.parse_scheme(before_place.uri) == scheme)
                return;
        }

        // Find the backend that has the given scheme
        foreach (var b in this.backends) {
            if (place in b) {
                var label = new Gtk.Label(b.label);
                label.tooltip_text = b.description;
                label.get_style_context().add_class("seahorse-sidebar-item-header");
                label.xalign = 0f;
                label.margin_start = 6;
                label.margin_top = 6;
                label.show();
                row.set_header(label);
                return;
            }
        }

        warning("Couldn't find backend for place %s", place.label);
    }

    private int compare_places(GLib.Object obj_a, GLib.Object obj_b) {
        Seahorse.Place a = (Seahorse.Place) obj_a;
        Seahorse.Place b = (Seahorse.Place) obj_b;

        // First of all, order the backends (SSH vs GPG)
        // Since there is no easy way to map a place to its original backend,
        // we can use the URI scheme
        var a_scheme = GLib.Uri.parse_scheme(a.uri);
        var b_scheme = GLib.Uri.parse_scheme(b.uri);
        if (a_scheme != b_scheme)
            return order_from_scheme(b_scheme) - order_from_scheme(a_scheme);

        // In the same backend, order alphabetically
        return a.label.casefold().collate(b.label.casefold());
    }

    // Keep in sync with order_from_scheme()
    private static int order_from_backend (Backend backend) {
        switch (backend.name) {
            case "gkr":
                return 0;
            case "pgp":
                return 1;
            case "pkcs11":
                return 2;
            case "ssh":
                return 3;
            default:
                return 10;
        }
    }

    // Keep in sync with order_from_backend()
    private static int order_from_scheme (string scheme) {
        switch (scheme) {
            case "secret-service":
                return 0;
            case "gnupg":
                return 1;
            case "pkcs11":
                return 2;
            case "openssh":
                return 3;
            default:
                return 10;
        }
    }

    private void update_objects() {
        debug("Updating objects (combined: %s)", this.combined.to_string());

        // Combined overrides and shows all objects
        if (this.combined) {
            foreach (Backend backend in this.backends) {
                foreach (var obj in backend.get_objects()) {
                    var place = (Place) obj;
                    if (!this.objects.have(place))
                        this.objects.add(place);
                }
            }
            return;
        }

        // Only selected ones should be in this.objects
        var selected = get_focused_place();
        if (selected == null)
            return;

        foreach (var place in this.objects.elements()) {
            if (selected != place)
                this.objects.remove(place);
        }
        if (!this.objects.have(selected))
            this.objects.add(selected);
    }

    private void update_places() {
        debug("Updating sidebar places");

        foreach (Backend backend in this.backends)
            update_backend(backend);

        this.store.sort(compare_places);

        // Update selection
        /* update_objects(); */
    }

    private void update_backend(Backend? backend) {
        if (backend.get_objects() == null) // Ignore categories that have nothing
            return;

        foreach (weak GLib.Object obj in backend.get_objects()) {
            unowned Place? place = obj as Place;
            if (place == null)
                continue;

            bool already_in = false;
            for (int i = 0; i < this.store.get_n_items(); i++) {
                if (this.store.get_object(i) == place) {
                    already_in = true;
                    break;
                }
            }

            if (!already_in)
                this.store.append(place);
        }
    }

    public override bool popup_menu() {
        if (base.popup_menu())
            return true;

        var row = get_selected_row() as SidebarItem;
        if (row == null)
            return false;

        row.show_popup_menu();
        return true;
    }

    public override bool button_press_event(Gdk.EventButton event) {
        if (base.button_press_event(event))
            return true;

        if (event.button != 3 || event.type != Gdk.EventType.BUTTON_PRESS)
            return false;

        var row = get_row_at_y((int) event.y) as SidebarItem;
        if (row != null)
            row.show_popup_menu();

        return true;
    }

    public string[] chosen_uris_to_array() {
        string[] results = {};
        foreach (string? uri in this.chosen)
            results += uri;

        results += null;

        return results;
    }

    public void replace_chosen_uris(string[] uris) {
        // For quick lookups
        GenericSet<string?> chosen = new GenericSet<string?>(str_hash, str_equal);
        foreach (string uri in uris)
            chosen.add(uri);

        /* update_objects(); */
        this.chosen = chosen;
    }

    public List<weak Gcr.Collection>? get_selected_places() {
        List<weak Gcr.Collection>? places = null;

        foreach (var row in get_selected_rows()) {
            var item = row as SidebarItem;
            if (item != null)
                places.append(item.place);
        }

        return places;
    }

    public Place? get_focused_place() {
        var row = get_selected_row() as SidebarItem;
        debug("Getting focused place: %p", row);
        return (row != null)? row.place : null;
    }

    public void set_focused_place(string uri_prefix) {
        foreach (Backend backend in this.backends) {
            foreach (weak GLib.Object obj in backend.get_objects()) {
                Place place = obj as Place;
                if (place == null)
                    continue;
                else if (place.uri.has_prefix(uri_prefix)) {
                    var chosen = new GenericSet<string?>(str_hash, str_equal);
                    chosen.add(place.uri);
                    //XXX
                    /* update_objects(); */
                    return;
                }
            }
        }
    }

    public List<weak Backend>? get_backends() {
        return this.backends.copy();
    }
}

internal class Seahorse.SidebarItem : Gtk.ListBoxRow {

    private Gtk.Button? lock_button = null;

    public weak Seahorse.Place place { get; construct set; }

    public signal void place_changed();

    construct {
      var grid = new Gtk.Grid();
      grid.get_style_context().add_class("seahorse-sidebar-item");
      grid.valign = Gtk.Align.CENTER;
      grid.row_spacing = 6;
      grid.column_spacing = 6;
      add(grid);

      var icon = new Gtk.Image.from_gicon(place.icon, Gtk.IconSize.BUTTON);
      grid.attach(icon, 0, 0);

      var label = new Gtk.Label(place.label);
      label.hexpand = true;
      label.ellipsize = Pango.EllipsizeMode.END;
      label.xalign = 0f;
      grid.attach(label, 1, 0);

      var lockable = place as Lockable;
      if (lockable != null && (lockable.lockable || lockable.unlockable)) {
          this.lock_button = new Gtk.Button.from_icon_name(get_lock_icon_name(lockable),
                                                          Gtk.IconSize.BUTTON);
          this.lock_button.get_style_context().add_class("flat");
          this.lock_button.clicked.connect((b) => {
               if (lockable.unlockable)
                   place_unlock(lockable, (Gtk.Window) get_toplevel());
               else if (lockable.lockable)
                   place_lock(lockable, (Gtk.Window) get_toplevel());

               update_lock_icon(lockable);
          });
          grid.attach(this.lock_button, 2, 0);
      }

      show_all();
    }

    private static unowned string? get_lock_icon_name(Lockable lockable) {
          if (lockable.unlockable)
              return "changes-prevent-symbolic";

          if (lockable.lockable)
              return "changes-allow-symbolic";

          return null;
    }

    private void update_lock_icon(Lockable lockable) {
        ((Gtk.Image) this.lock_button.get_image()).icon_name = get_lock_icon_name(lockable);
    }

    public SidebarItem(Seahorse.Place place) {
        GLib.Object(place: place);
    }

    public void show_popup_menu() {
        // Start from the menu model provided by the this.place (if any)
        var menu = (this.place.menu_model != null)? new Gtk.Menu.from_model(this.place.menu_model)
                                                  : new Gtk.Menu();

        // Make sure the actions from the collection
        if (this.place.actions != null)
            menu.insert_action_group(this.place.action_prefix, this.place.actions);

        // Lock and unlock items
        if (this.place is Lockable) {
            Gtk.MenuItem item = new Gtk.MenuItem.with_mnemonic(_("_Lock"));
            item.activate.connect(() => on_place_lock(item, (Lockable) this.place));
            this.place.bind_property("lockable", item, "visible", BindingFlags.SYNC_CREATE);
            menu.append(item);

            item = new Gtk.MenuItem.with_mnemonic(_("_Unlock"));
            item.activate.connect(() => on_place_unlock(item, (Lockable) this.place));
            this.place.bind_property("unlockable", item, "visible", BindingFlags.SYNC_CREATE);
            menu.append(item);
        }

        // Delete item
        if (this.place is Deletable) {
            Gtk.MenuItem item = new Gtk.MenuItem.with_mnemonic(_("_Delete"));
            item.activate.connect(() => on_place_delete(item, (Deletable) this.place));
            this.place.bind_property("deletable", item, "sensitive", BindingFlags.SYNC_CREATE);
            menu.append(item);
            item.show();
        }

        // Properties item
        if (this.place is Viewable) {
            Gtk.MenuItem item = new Gtk.MenuItem.with_mnemonic(_("_Properties"));
            item.activate.connect(() => Viewable.view(this.place, (Gtk.Window) item.get_toplevel()));
            menu.append(item);
            item.show();
        }

        bool visible = false;
        menu.foreach((widget) => {
            visible |= widget.visible;
        });

        if (visible) {
            menu.popup_at_pointer();
            menu.attach_to_widget(this, null);
            menu.show();
        } else {
            menu.destroy();
        }
    }

    private void place_lock(Lockable lockable, Gtk.Window? window) {
        Cancellable cancellable = new Cancellable();
        TlsInteraction interaction = new Interaction(window);

        lockable.lock.begin(interaction, cancellable, (obj, res) => {
            try {
                lockable.lock.end(res);
                update_lock_icon (lockable);
                place_changed();
            } catch (Error e) {
                Util.show_error(window, _("Couldn’t lock"), e.message);
            }
        });
    }

    private void on_place_lock(Gtk.Widget widget, Lockable lockable) {
        place_lock(lockable, (Gtk.Window) widget.get_toplevel());
    }

    private void place_unlock(Lockable lockable, Gtk.Window? window) {
        Cancellable cancellable = new Cancellable();
        TlsInteraction interaction = new Interaction(window);

        lockable.unlock.begin(interaction, cancellable, (obj, res) => {
            try {
                lockable.unlock.end(res);
                update_lock_icon (lockable);
                place_changed();
            } catch (Error e) {
                Util.show_error(window, _("Couldn’t unlock"), e.message);
            }
        });
    }

    private void on_place_unlock(Gtk.MenuItem widget, Lockable lockable) {
        place_unlock(lockable, (Gtk.Window) widget.get_toplevel());
    }

    private void on_place_delete(Gtk.MenuItem item, Deletable deletable) {
        Deleter deleter = deletable.create_deleter();
        if (deleter.prompt((Gtk.Window) item.get_toplevel())) {
            deleter.delete.begin(null, (obj, res) => {
                try {
                    deleter.delete.end(res);
                } catch (Error e) {
                    Util.show_error(parent, _("Couldn’t delete"), e.message);
                }
            });
        }
    }
}
