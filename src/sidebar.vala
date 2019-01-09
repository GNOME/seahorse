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

public class Seahorse.Sidebar : Gtk.TreeView {

    private const int ACTION_BUTTON_XPAD = 6;

    private Gtk.ListStore store = new Gtk.ListStore.newv(Column.types());
    private List<Backend> backends = new List<Backend>();
    private Gcr.UnionCollection objects = new Gcr.UnionCollection();

    // The selection
    private HashTable<Gcr.Collection, Gcr.Collection> selection
         = new HashTable<Gcr.Collection, Gcr.Collection>(direct_hash, direct_equal);
    private bool updating;

    // A set of chosen uris, used with settings
    private GenericSet<string?> chosen = new GenericSet<string?>(str_hash, str_equal);

    // Action icons
    private Gdk.Pixbuf? pixbuf_lock;
    private Gdk.Pixbuf? pixbuf_unlock;
    private Gdk.Pixbuf? pixbuf_lock_l;
    private Gdk.Pixbuf? pixbuf_unlock_l;
    private Gtk.TreePath? action_highlight_path;
    private Gtk.CellRendererPixbuf action_cell_renderer;
    private int action_button_size;
    private Gtk.AccelGroup accel_group = new Gtk.AccelGroup();

    private uint update_places_sig;

    /**
     * Collection of objects sidebar represents
     */
    public Gcr.Collection collection {
        get { return this.objects; }
    }

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
                update_objects_in_collection(false);
            }
        }
    }
    private bool _combined;

    private enum RowType {
        BACKEND,
        PLACE,
    }

    private enum Column {
        ROW_TYPE,
        ICON,
        LABEL,
        TOOLTIP,
        CATEGORY,
        COLLECTION,
        URI,
        N_COLUMNS;

        public static Type[] types() {
            return {
                typeof(uint),
                typeof(Icon),
                typeof(string),
                typeof(string),
                typeof(string),
                typeof(Gcr.Collection),
                typeof(string)
            };
        }
    }

    public Sidebar() {
        /* get_style_context().set_junction_sides(Gtk.JunctionSides.RIGHT | Gtk.JunctionSides.LEFT); */

        // tree view
        Gtk.TreeViewColumn col = new Gtk.TreeViewColumn();

        // initial padding
        Gtk.CellRenderer cell = new Gtk.CellRendererText();
        col.pack_start(cell, false);
        cell.xpad = 6;

        // headings
        Gtk.CellRendererText headings_cell = new Gtk.CellRendererText();
        col.pack_start(headings_cell, false);
        col.set_attributes(headings_cell, "text", Column.LABEL, null);
        headings_cell.weight = Pango.Weight.BOLD;
        headings_cell.weight_set = true;
        headings_cell.ypad = 6;
        headings_cell.xpad = 0;
        col.set_cell_data_func(headings_cell, on_cell_renderer_heading_visible);

        // icon padding
        cell = new Gtk.CellRendererText();
        col.pack_start(cell, false);
        col.set_cell_data_func(cell, on_padding_cell_renderer);

        // icon renderer
        cell = new Gtk.CellRendererPixbuf();
        col.pack_start(cell, false);
        col.set_attributes(cell, "gicon", Column.ICON, null);
        col.set_cell_data_func(cell, on_cell_renderer_heading_not_visible);

        // normal text renderer
        Gtk.CellRendererText text_cell = new Gtk.CellRendererText();
        col.pack_start(text_cell, true);
        text_cell.editable = false;
        col.set_attributes(text_cell, "text", Column.LABEL, null);
        col.set_cell_data_func(text_cell, on_cell_renderer_heading_not_visible);
        text_cell.ellipsize = Pango.EllipsizeMode.END;
        text_cell.ellipsize_set = true;

        // lock/unlock icon renderer
        this.action_cell_renderer = new Gtk.CellRendererPixbuf();
        this.action_cell_renderer.mode = Gtk.CellRendererMode.ACTIVATABLE;
        this.action_cell_renderer.stock_size = Gtk.IconSize.MENU;
        this.action_cell_renderer.xpad = ACTION_BUTTON_XPAD;
        this.action_cell_renderer.xalign = 1.0f;
        col.pack_start(this.action_cell_renderer, false);
        col.set_cell_data_func(this.action_cell_renderer, on_cell_renderer_action_icon);
        col.set_max_width(24);
        append_column(col);

        set_headers_visible(false);
        set_tooltip_column(Column.TOOLTIP);
        set_search_column(Column.LABEL);
        set_model(this.store);
        this.popup_menu.connect(on_popup_menu);
        this.button_press_event.connect(on_button_press_event);
        this.motion_notify_event.connect(on_motion_notify_event);
        this.button_release_event.connect(on_button_release_event);

        Gtk.TreeSelection selection = get_selection();
        selection.set_mode(Gtk.SelectionMode.MULTIPLE);
        selection.set_select_function(on_tree_selection_validate);
        selection.changed.connect(() => update_objects_for_selection(selection));

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

        invalidate_sidebar_pixbufs();

        if (this.update_places_sig != 0)
            Source.remove(this.update_places_sig);
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

    private void on_place_added(Gcr.Collection? places, GLib.Object obj) {
        ((Place) obj).notify.connect(() => update_places_later());
        update_places_later();
    }

    private void on_place_removed(Gcr.Collection? places, GLib.Object obj) {
        SignalHandler.disconnect_by_func((void*) obj, (void*) update_places_later, this);
        update_places_later();
    }

    private void on_backend_changed(GLib.Object obj, ParamSpec spec) {
        update_places_later();
    }

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

    private void ensure_sidebar_pixbufs() {
        if (this.pixbuf_lock != null && this.pixbuf_lock_l != null
            && this.pixbuf_unlock_l != null && this.pixbuf_unlock != null)
            return;

        Gtk.IconTheme icon_theme = Gtk.IconTheme.get_default();
        Gtk.StyleContext style = get_style_context();

        int height;
        if (!Gtk.icon_size_lookup(Gtk.IconSize.MENU, out this.action_button_size, out height))
            this.action_button_size = 16;

        // Lock icon
        Icon icon = new ThemedIcon.with_default_fallbacks("changes-prevent-symbolic");
        Gtk.IconInfo? icon_info = icon_theme.lookup_by_gicon(icon, this.action_button_size, Gtk.IconLookupFlags.FORCE_SYMBOLIC);
        if (icon_info == null)
            return;
        try {
            if (this.pixbuf_lock == null)
                this.pixbuf_lock = icon_info.load_symbolic_for_context(style, null);
            if (this.pixbuf_lock_l == null)
                this.pixbuf_lock_l = create_spotlight_pixbuf(this.pixbuf_lock);
        } catch (Error e) {
            debug("Error while looking up lock icon: %s", e.message);
        }

        // Unlock icon
        icon = new ThemedIcon.with_default_fallbacks("changes-allow-symbolic");
        icon_info = icon_theme.lookup_by_gicon(icon, this.action_button_size, Gtk.IconLookupFlags.FORCE_SYMBOLIC);
        if (icon_info == null)
            return;
        try {
            if (this.pixbuf_unlock == null)
                this.pixbuf_unlock = icon_info.load_symbolic_for_context(style, null);
            if (this.pixbuf_unlock_l == null)
                this.pixbuf_unlock_l = create_spotlight_pixbuf(this.pixbuf_unlock);
        } catch (Error e) {
            debug("Error while looking up unlock icon: %s", e.message);
        }
    }

    private Gdk.Pixbuf? create_spotlight_pixbuf (Gdk.Pixbuf? src) {
        Gdk.Pixbuf? dest = new Gdk.Pixbuf(src.colorspace, src.has_alpha, src.bits_per_sample,
                                          src.width, src.height);

        bool has_alpha = src.has_alpha;
        int width = src.width;
        int height = src.height;
        int dst_row_stride = dest.rowstride;
        int src_row_stride = src.rowstride;
        uint8* target_pixels = dest.pixels;
        uint8* original_pixels = src.pixels;

        for (int i = 0; i < height; i++) {
            uint8* pixdest = target_pixels + i * dst_row_stride;
            uint8* pixsrc = original_pixels + i * src_row_stride;
            for (int j = 0; j < width; j++) {
                *pixdest++ = lighten_component (*pixsrc++);
                *pixdest++ = lighten_component (*pixsrc++);
                *pixdest++ = lighten_component (*pixsrc++);
                if (has_alpha) {
                    *pixdest++ = *pixsrc++;
                }
            }
        }
        return dest;
    }

    private uint8 lighten_component(uint8 cur_value) {
        int new_value = cur_value + 24 + (cur_value >> 3);
        return (new_value > 255)? (uint8)255 : (uint8)new_value;
    }

    private void invalidate_sidebar_pixbufs() {
        this.pixbuf_lock = null;
        this.pixbuf_unlock = null;
        this.pixbuf_lock_l = null;
        this.pixbuf_unlock_l = null;
    }

    private void next_or_append_row(Gtk.ListStore? store, ref Gtk.TreeIter iter, string? category,
                                    Gcr.Collection? collection) {
        // We try to keep the same row in order to preserve checked state
        // and selections. So if the next row matches the values we want to
        // set on it, then just keep that row.
        //
        // This is complicated by the fact that the first row being inserted
        // doesn't have a valid iter, and we don't have a standard way to
        // detect that an iter isn't valid.

        // A marker that tells us the iter is not yet valid
        if (iter.stamp == int.from_pointer(&iter) && iter.user_data3 == (&iter) &&
            iter.user_data2 == (&iter) && iter.user_data == (&iter)) {
            if (!store.get_iter_first(out iter))
                store.append(out iter);
            return;
        }

        if (!store.iter_next(ref iter)) {
            store.append(out iter);
            return;
        }

        for (;;) {
            string? row_category;
            Gcr.Collection? row_collection;
            store.get(iter, Column.CATEGORY, out row_category,
                            Column.COLLECTION, out row_collection);

            if (row_category == category && row_collection == collection)
                return;

            if (!store.remove(ref iter)) {
                store.append(out iter);
                return;
            }
        }
    }

    private void update_objects_in_collection(bool update_chosen) {
        if (this.updating) // Updating collection is blocked
            return;

        bool changed = false;
        foreach (Backend backend in this.backends) {
            foreach (weak GLib.Object obj in backend.get_objects()) {
                Place place = (Place) obj;
                bool include = this.selection.lookup(place) != null;

                if (update_chosen) {
                    string? uri = place.uri;
                    bool have = (uri in this.chosen);
                    if (include && !have) {
                        this.chosen.add(uri);
                        changed = true;
                    } else if (!include && have) {
                        this.chosen.remove(uri);
                        changed = true;
                    }
                }

                // Combined overrides and shows all objects
                if (this.combined)
                    include = true;

                bool have = this.objects.have(place);
                if (include && !have)
                    this.objects.add(place);
                else if (!include && have)
                    this.objects.remove(place);
            }
        }
    }

    private void update_objects_for_selection(Gtk.TreeSelection selection) {
        if (this.updating)
            return;

        HashTable<Gcr.Collection, Gcr.Collection> selected = new HashTable<Gcr.Collection, Gcr.Collection>(direct_hash, direct_equal);
        selection.selected_foreach((model, path, iter) => {
            Gcr.Collection? collection = null;
            model.get(iter, Column.COLLECTION, out collection, -1);
            if (collection != null)
                selected.insert(collection, collection);
        });

        this.selection = selected;

        if (!this.combined)
            update_objects_in_collection(true);
    }

    private void update_objects_for_chosen(GenericSet<string?> chosen) {
        this.updating = true;

        Gtk.TreeSelection selection = get_selection();

        // Update the display
        Gtk.TreeIter iter;
        if (this.store.get_iter_first(out iter)) {
            do {
                Gcr.Collection? collection = null;
                string? uri = null;
                this.store.get(iter, Column.COLLECTION, out collection,
                                     Column.URI, out uri, -1);

                if (collection != null && uri != null) {
                    if (uri in chosen)
                        selection.select_iter(iter);
                    else
                        selection.unselect_iter(iter);
                }
            } while (this.store.iter_next(ref iter));
        }

        this.updating = false;
        update_objects_for_selection(selection);
    }

    private void update_places() {
        Gtk.TreeIter iter = Gtk.TreeIter();
        iter.stamp = int.from_pointer(&iter); // A marker that tells us the iter is not yet valid
        iter.user_data3 = iter.user_data2 = iter.user_data = &iter;

        foreach (Backend backend in this.backends)
            update_backend(backend, ref iter);

        // Update selection
        update_objects_for_chosen(this.chosen);

        if (this.combined)
            update_objects_in_collection(false);
    }

    private void update_backend(Backend? backend, ref Gtk.TreeIter iter) {
        if (backend.get_objects() == null) // Ignore categories that have nothing
            return;

        next_or_append_row(this.store, ref iter, backend.name, backend);
        this.store.set(iter, Column.ROW_TYPE, RowType.BACKEND,
                             Column.CATEGORY, backend.name,
                             Column.LABEL, backend.label,
                             Column.TOOLTIP, backend.description,
                             Column.COLLECTION, backend);

        foreach (weak GLib.Object obj in backend.get_objects()) {
            Place place = obj as Place;
            if (place == null)
                continue;

            next_or_append_row(this.store, ref iter, backend.name, place);
            this.store.set(iter, Column.ROW_TYPE, RowType.PLACE,
                                 Column.CATEGORY, backend.name,
                                 Column.LABEL, place.label,
                                 Column.TOOLTIP, place.description,
                                 Column.ICON, place.icon,
                                 Column.COLLECTION, place,
                                 Column.URI, place.uri);
        }
    }

    private void update_places_later() {
        if (this.update_places_sig == 0) {
            this.update_places_sig = Idle.add(() => {
                this.update_places_sig = 0;
                update_places();
                return false; // don't call again
            });
        }
    }

    private Lockable? lookup_lockable_for_iter(Gtk.TreeModel? model, Gtk.TreeIter? iter) {
        Gcr.Collection? collection = null;
        model.get(iter, Column.COLLECTION, out collection, -1);

        return collection as Lockable;
    }

    private void on_cell_renderer_action_icon(Gtk.CellLayout layout, Gtk.CellRenderer? cell,
                                              Gtk.TreeModel? model, Gtk.TreeIter? iter) {
        bool can_lock = false;
        bool can_unlock = false;

        Lockable? lockable = lookup_lockable_for_iter(model, iter);
        if (lockable != null) {
            can_lock = lockable.lockable;
            can_unlock = lockable.unlockable;
        }

        if (can_lock || can_unlock) {
            ensure_sidebar_pixbufs();

            bool highlight = false;
            if (this.action_highlight_path != null) {
                Gtk.TreePath? path = model.get_path(iter);
                highlight = path.compare(this.action_highlight_path) == 0;
            }

            Gdk.Pixbuf? pixbuf;
            if (can_lock)
                pixbuf = highlight ? this.pixbuf_unlock : this.pixbuf_unlock_l;
            else
                pixbuf = highlight ? this.pixbuf_lock : this.pixbuf_lock_l;

            this.action_cell_renderer.visible = true;
            this.action_cell_renderer.pixbuf = pixbuf;
        } else {
            this.action_cell_renderer.visible = false;
            this.action_cell_renderer.pixbuf = null;
        }
    }

    private void on_cell_renderer_heading_visible(Gtk.CellLayout layout, Gtk.CellRenderer? cell,
                                                  Gtk.TreeModel? model, Gtk.TreeIter? iter) {
        RowType type;
        model.get(iter, Column.ROW_TYPE, out type, -1);
        cell.visible = (type == RowType.BACKEND);
    }

    private void on_padding_cell_renderer(Gtk.CellLayout layout, Gtk.CellRenderer? cell,
                                          Gtk.TreeModel? model, Gtk.TreeIter? iter) {
        RowType type;
        model.get(iter, Column.ROW_TYPE, out type, -1);

        if (type == RowType.BACKEND) {
            cell.visible = false;
            cell.xpad = 0;
            cell.ypad = 0;
        } else {
            cell.visible = true;
            cell.xpad = 3;
            cell.ypad = 3;
        }
    }

    private void on_cell_renderer_heading_not_visible(Gtk.CellLayout layout, Gtk.CellRenderer? cell,
                                                      Gtk.TreeModel? model, Gtk.TreeIter? iter) {
        RowType type;
        model.get(iter, Column.ROW_TYPE, out type, -1);
        cell.visible = (type != RowType.BACKEND);
    }

    private bool on_tree_selection_validate(Gtk.TreeSelection selection, Gtk.TreeModel? model,
                                            Gtk.TreePath? path, bool path_currently_selected) {
        Gtk.TreeIter iter;
        model.get_iter(out iter, path);

        RowType row_type;
        model.get(iter, Column.ROW_TYPE, out row_type, -1);
        if (row_type == RowType.BACKEND)
            return false;

        return true;
    }

    private void place_lock(Lockable lockable, Gtk.Window? window) {
        Cancellable cancellable = new Cancellable();
        TlsInteraction interaction = new Interaction(window);

        lockable.lock.begin(interaction, cancellable, (obj, res) => {
            try {
                lockable.lock.end(res);
            } catch (Error e) {
                Util.show_error(window, _("Couldn’t lock"), e.message);
            }
        });
    }

    private void on_place_lock(Gtk.MenuItem item, Lockable lockable) {
        place_lock(lockable, (Gtk.Window) item.get_toplevel());
    }

    private void place_unlock(Lockable lockable, Gtk.Window? window) {
        Cancellable cancellable = new Cancellable();
        TlsInteraction interaction = new Interaction(window);

        lockable.unlock.begin(interaction, cancellable, (obj, res) => {
            try {
                lockable.unlock.end(res);
            } catch (Error e) {
                Util.show_error(window, _("Couldn’t unlock"), e.message);
            }
        });
    }

    private void on_place_unlock(Gtk.MenuItem item, Lockable lockable) {
        place_unlock(lockable, (Gtk.Window) item.get_toplevel());
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

    private void popup_menu_for_place(Place place) {
        Gtk.Menu menu = new Gtk.Menu();

        // First add all the actions from the collection
        Gtk.ActionGroup actions = place.actions;
        if (actions != null) {
            foreach (weak Gtk.Action action in actions.list_actions()) {
                action.set_accel_group(this.accel_group);
                menu.append((Gtk.MenuItem) action.create_menu_item());
            }
        }

        // Lock and unlock items
        if (place is Lockable) {
            Gtk.MenuItem item = new Gtk.MenuItem.with_mnemonic(_("_Lock"));
            item.activate.connect(() => on_place_lock(item, (Lockable) place));
            place.bind_property("lockable", item, "visible", BindingFlags.SYNC_CREATE);
            menu.append(item);

            item = new Gtk.MenuItem.with_mnemonic(_("_Unlock"));
            item.activate.connect(() => on_place_unlock(item, (Lockable) place));
            place.bind_property("unlockable", item, "visible", BindingFlags.SYNC_CREATE);
            menu.append(item);
        }

        // Delete item
        if (place is Deletable) {
            Gtk.MenuItem item = new Gtk.MenuItem.with_mnemonic(_("_Delete"));
            item.activate.connect(() => on_place_delete(item, (Deletable) place));
            place.bind_property("deletable", item, "sensitive", BindingFlags.SYNC_CREATE);
            menu.append(item);
            item.show();
        }

        // Properties item
        if (place is Viewable) {
            Gtk.MenuItem item = new Gtk.MenuItem.with_mnemonic(_("_Properties"));
            item.activate.connect(() => Viewable.view(place, (Gtk.Window) item.get_toplevel()));
            menu.append(item);
            item.show();
        }

        bool visible = false;
        menu.foreach((widget) => {
            if (widget.visible)
                visible = true;
        });

        if (visible) {
            menu.popup_at_pointer();
            menu.attach_to_widget(this, null);
            menu.show();
        } else {
            menu.destroy();
        }
    }

    private bool on_popup_menu(Gtk.Widget? widget) {
        Gtk.TreePath? path;
        get_cursor(out path, null);
        if (path == null)
            return false;

        Gtk.TreeIter iter;
        if (!this.store.get_iter(out iter, path))
            return false;

        Gcr.Collection? collection;
        this.store.get(iter, Column.COLLECTION, out collection, -1);

        if (collection is Place) {
            popup_menu_for_place((Place) collection);
            return true;
        }

        return false;
    }

    private void update_action_buttons_take_path(Gtk.TreePath? path) {
        if (path == this.action_highlight_path)
            return;

        if (path != null && this.action_highlight_path != null &&
            this.action_highlight_path.compare(path) == 0) {
            return;
        }

        Gtk.TreePath? old_path = this.action_highlight_path;
        this.action_highlight_path = path;

        Gtk.TreeIter? iter = null;
        if (this.action_highlight_path != null
              && this.store.get_iter(out iter, this.action_highlight_path))
            this.store.row_changed(this.action_highlight_path, iter);

        if (old_path != null && this.store.get_iter(out iter, old_path))
            this.store.row_changed(old_path, iter);
    }

    private bool over_action_button(int x, int y, out Gtk.TreePath? path) {

        path = null;
        Gtk.TreeViewColumn column;
        if (get_path_at_pos(x, y, out path, out column, null, null)) {
            Gtk.TreeIter iter;
            this.store.get_iter(out iter, path);

            int hseparator;
            style_get("horizontal-separator", out hseparator, null);

            // Reload cell attributes for this particular row
            column.cell_set_cell_data(this.store, iter, false, false);
            int width, x_offset;
            column.cell_get_position(this.action_cell_renderer, out x_offset, out width);

            // This is kinda weird, but we have to do it to workaround gtk+ expanding
            // the eject cell renderer (even thought we told it not to) and we then
            // had to set it right-aligned
            x_offset += width - hseparator - ACTION_BUTTON_XPAD - this.action_button_size;

            if (x - x_offset >= 0 && x - x_offset <= this.action_button_size)
                return true;
        }

        if (path != null)
            path = null;

        return false;
    }

    private bool on_motion_notify_event(Gtk.Widget? widget, Gdk.EventMotion event) {
        Gtk.TreePath? path = null;
        if (over_action_button((int) event.x, (int) event.y, out path)) {
            update_action_buttons_take_path(path);
            return true;
        }

        update_action_buttons_take_path(null);
        return false;
    }

    private bool on_button_press_event (Gtk.Widget? widget, Gdk.EventButton event) {
        if (event.button != 3 || event.type != Gdk.EventType.BUTTON_PRESS)
            return false;

        Gtk.TreePath? path;
        if (!get_path_at_pos((int) event.x, (int) event.y, out path, null, null, null))
            return false;

        set_cursor(path, null, false);
        Gtk.TreeIter iter;
        if (!this.store.get_iter(out iter, path))
            return false;

        Gcr.Collection? collection;
        this.store.get(iter, Column.COLLECTION, out collection, -1);

        if (collection is Place)
            popup_menu_for_place((Place) collection);

        return true;
    }

    private bool on_button_release_event (Gtk.Widget? widget, Gdk.EventButton event) {
        if (event.type != Gdk.EventType.BUTTON_RELEASE)
            return true;

        Gtk.TreePath? path;
        if (!over_action_button((int) event.x, (int) event.y, out path))
            return false;

        Gtk.TreeIter iter;
        if (!this.store.get_iter(out iter, path))
            return false;

        Gtk.Window? window = (Gtk.Window) widget.get_toplevel();

        Lockable? lockable = lookup_lockable_for_iter(this.store, iter);
        if (lockable != null) {
            if (lockable.lockable)
                place_lock(lockable, window);
            else if (lockable.unlockable)
                place_unlock(lockable, window);
        }

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

        update_objects_for_chosen(chosen);
        this.chosen = chosen;
    }

    public List<weak Gcr.Collection>? get_selected_places() {
        List<weak Gcr.Collection> places = this.objects.elements();

        Gtk.TreePath? path = null;
        get_cursor(out path, null);
        if (path != null) {

            Gtk.TreeIter iter;
            if (!this.store.get_iter(out iter, path))
                return null;

            Gcr.Collection? collection;
            RowType row_type;
            this.store.get(iter, Column.ROW_TYPE, out row_type,
                                 Column.COLLECTION, out collection, -1);

            if (collection != null) {
                if (row_type == RowType.PLACE) {
                    places.remove(collection);
                    places.prepend(collection);
                }
            }
        }

        return places;
    }

    public Place? get_focused_place() {
        Gtk.TreeIter iter;

        Gtk.TreePath? path = null;
        get_cursor(out path, null);
        if (path != null) {
            if (!this.store.get_iter(out iter, path))
                return null;

            Gcr.Collection? collection;
            RowType row_type;
            this.store.get(iter, Column.ROW_TYPE, out row_type,
                                 Column.COLLECTION, out collection, -1);

            if (row_type == RowType.PLACE)
                return (Place) collection;
        }

        return null;
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
                    this.update_objects_for_chosen(chosen);
                    return;
                }
            }
        }
    }

    public List<weak Backend>? get_backends() {
        Gtk.TreeIter iter;

        List<weak Backend> backends = this.backends.copy();
        backends.reverse();

        Gtk.TreePath? path = null;
        get_cursor(out path, null);
        if (path != null) {
            if (!this.store.get_iter(out iter, path))
                return null;

            Gcr.Collection? collection;
            RowType row_type;
            this.store.get(iter, Column.ROW_TYPE, out row_type,
                                 Column.COLLECTION, out collection, -1);

            if (collection != null) {
                if (row_type == RowType.BACKEND) {
                    backends.remove((Backend) collection);
                    backends.prepend((Backend) collection);
                }
            }
        }

        return backends;
    }
}
