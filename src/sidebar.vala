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

    /**
     * Collection of objects sidebar represents
     */
    public Gcr.UnionCollection objects { get; private set; default = new Gcr.UnionCollection(); }

    /**
     * Emitted when the state of the current collection changed.
     * For example: when going from locked to unlocked and vice versa.
     */
    public signal void current_collection_changed();

    construct {
        this.selection_mode = Gtk.SelectionMode.BROWSE;

        bind_model(this.store, place_widget_create_cb);
        set_header_func(place_header_cb);
        this.row_selected.connect(on_row_selected);

        load_backends();
    }

    ~Sidebar() {
        foreach (Backend backend in this.backends)
            foreach (weak GLib.Object obj in backend.get_objects())
                on_place_removed (backend, (Place) obj);
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
    }

    private void on_place_added(Gcr.Collection? backend, GLib.Object place_obj) {
        var place = place_obj as Place;
        return_if_fail (place != null);

        debug("New place '%s' added", place.label);

        if (place.get_length() > 0 || place.show_if_empty)
            this.store.insert_sorted(place, compare_places);
        place.notify.connect(on_place_changed);
    }

    private void on_place_changed(GLib.Object obj, ParamSpec pspec) {
        update_places();
    }

    private void on_place_removed(Gcr.Collection? backend, GLib.Object place_obj) {
        var place = place_obj as Place;
        return_if_fail (place != null);

        debug("Place '%s' removed", place.label);
        for (uint i = 0; i < this.store.get_n_items(); i++) {
            if (this.store.get_item(i) == place) {
                this.store.remove(i);
                break;
            }
        }
    }

    private void on_backend_changed(GLib.Object obj, ParamSpec spec) {
        Backend backend = (Backend) obj;
        debug("Backend '%s' changed", backend.label);
        update_places();
    }

    private Gtk.Widget place_widget_create_cb(GLib.Object object) {
        var item = new SidebarItem(object as Seahorse.Place);
        item.place_changed.connect(on_sidebar_item_changed);
        return item;
    }

    private void on_sidebar_item_changed(SidebarItem item) {
        select_row(item);
        current_collection_changed();
    }

    private void place_header_cb(Gtk.ListBoxRow row, Gtk.ListBoxRow? before) {
        Seahorse.Place place = ((SidebarItem) row).place;

        // We need a title iff
        // the previous row exists, and it's part of the same category
        if (before != null) {
            Seahorse.Place before_place = ((SidebarItem) before).place;
            if (place.category == before_place.category) {
                row.set_header(null);
                return;
            }
        }

        var label = new Gtk.Label(place.category.to_string());
        label.get_style_context().add_class("seahorse-sidebar-item-header");
        label.xalign = 0f;
        label.margin_start = 9;
        label.margin_top = 6;
        label.margin_bottom = 3;
        label.show();
        row.set_header(label);
    }

    private int compare_places(GLib.Object obj_a, GLib.Object obj_b) {
        Seahorse.Place a = (Seahorse.Place) obj_a;
        Seahorse.Place b = (Seahorse.Place) obj_b;

        // First of all, order the categories
        if (a.category != b.category)
            return ((int) a.category) - ((int) b.category);

        // In the same category, order alphabetically
        return a.label.casefold().collate(b.label.casefold());
    }

    private void on_row_selected(Gtk.ListBoxRow? row) {
        debug("Updating objects");

        // First clear the list
        foreach (var place in this.objects.elements())
            this.objects.remove(place);

        // Only selected ones should be in this.objects
        var selected = row as SidebarItem;
        if (selected == null)
            return;

        foreach (var place in this.objects.elements()) {
            if (selected.place != place)
                this.objects.remove(place);
        }
        if (!this.objects.have(selected.place))
            this.objects.add(selected.place);
    }

    private void update_places() {
        // Save current selection
        var old_selected = get_selected_row() as SidebarItem;
        Place? place = null;
        if (old_selected != null)
            place = old_selected.place;

        foreach (Backend backend in this.backends)
            update_backend(backend);

        // Needed when you have multiple keyrings (this can lead
        // to multiple 'Password' titles).
        invalidate_headers();
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

            if (!already_in && (place.get_length() > 0 || place.show_if_empty))
                this.store.insert_sorted(place, compare_places);
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
        return (row != null)? row.place : null;
    }

    /**
     * Tries to find a place with the given URI and selects it.
     *
     * @return Whether a matching place was found
     */
    public bool set_focused_place_for_uri(string uri) {
        // First, try to see if we have an exact match
        foreach (var row in get_children()) {
            var item = (SidebarItem) row;
            if (item.place.uri == uri) {
                select_row(item);
                return true;
            }
        }

        return false;
    }

    /**
     * Tries to find a place with the given URI scheme and selects it.
     *
     * @return Whether a matching place was found
     */
    public bool set_focused_place_for_scheme(string uri_scheme) {
        foreach (var row in get_children()) {
            var item = (SidebarItem) row;
            if (item.place.uri.has_prefix(uri_scheme)) {
                select_row(item);
                return true;
            }
        }
        return false;
    }

    public void select_next_place() {
      select_relative(1);
    }

    public void select_previous_place() {
      select_relative(-1);
    }

    // Selects the item that is n positions lower/above the current selection
    private void select_relative(int positions) {
        var selected = get_selected_row();
        if (selected == null) {
            // Select the first row
            select_row(get_row_at_index(0));
            return;
        }

        var next = get_row_at_index(selected.get_index() + positions);
        // If we're at the top/bottom of the list, don't do anything
        if (next != null)
            select_row(next);
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
      var box = new Gtk.Box(Gtk.Orientation.HORIZONTAL, 6);
      box.get_style_context().add_class("seahorse-sidebar-item");
      box.valign = Gtk.Align.CENTER;
      box.margin_top = 3;
      box.margin_bottom = 3;
      add(box);

      var icon = new Gtk.Image.from_gicon(place.icon, Gtk.IconSize.BUTTON);
      box.pack_start(icon, false, false);

      var label = new Gtk.Label(place.label);
      label.ellipsize = Pango.EllipsizeMode.END;
      label.xalign = 0f;
      box.pack_start(label);

      var lockable = place as Lockable;
      if (lockable != null && (lockable.lockable || lockable.unlockable)) {
          this.lock_button = new Gtk.Button.from_icon_name(get_lock_icon_name(lockable),
                                                          Gtk.IconSize.BUTTON);
          this.lock_button.get_style_context().add_class("flat");
          this.lock_button.halign = Gtk.Align.END;
          this.lock_button.clicked.connect((b) => {
              if (lockable.unlockable)
                  place_unlock(lockable, (Gtk.Window) get_toplevel());
              else if (lockable.lockable)
                  place_lock(lockable, (Gtk.Window) get_toplevel());

              update_lock_icon(lockable);
          });
          box.pack_end(this.lock_button, false, false);
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
                update_lock_icon(lockable);
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
                update_lock_icon(lockable);
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
