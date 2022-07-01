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

public class Seahorse.Sidebar : Adw.Bin {

    private unowned Gtk.ListBox list_box;

    private Gtk.FlattenListModel places;

    public Gtk.SingleSelection selection { get; private set; }

    construct {
        var listbox = new Gtk.ListBox();
        this.child = listbox;
        this.list_box = listbox;
        listbox.add_css_class("navigation-sidebar");
        listbox.selection_mode = Gtk.SelectionMode.BROWSE;

        // ListModels everywhere \o/
        this.places = new Gtk.FlattenListModel(Backend.get_registered());

        var sorter = new Gtk.CustomSorter(compare_places);
        var places_sorted = new Gtk.SortListModel(this.places, sorter);

        this.selection = new Gtk.SingleSelection(places_sorted);

        // Listbox stuff
        listbox.bind_model(this.selection, place_widget_create_cb);
        listbox.set_header_func(place_header_cb);
        listbox.row_selected.connect(on_row_selected);

        // Right click
        var secondary_click_gesture = new Gtk.GestureClick ();
        secondary_click_gesture.button = Gdk.BUTTON_SECONDARY;
        secondary_click_gesture.pressed.connect(on_right_click);
        listbox.add_controller(secondary_click_gesture);
    }

    private Gtk.Widget place_widget_create_cb(GLib.Object object) {
        return new SidebarItem(object as Seahorse.Place);
    }

    private void place_header_cb(Gtk.ListBoxRow row, Gtk.ListBoxRow? before) {
        unowned var place = ((SidebarItem) row).place;

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
        label.add_css_class("seahorse-sidebar-item-header");
        label.xalign = 0f;
        label.margin_start = 9;
        label.margin_top = 6;
        label.margin_bottom = 3;
        label.show();
        row.set_header(label);
    }

    private int compare_places(GLib.Object? obj_a, GLib.Object? obj_b) {
        unowned var a = (Seahorse.Place) obj_a;
        unowned var b = (Seahorse.Place) obj_b;

        // First of all, order the categories
        if (a.category != b.category)
            return ((int) a.category) - ((int) b.category);

        // In the same category, order alphabetically
        return a.label.casefold().collate(b.label.casefold());
    }

    private void on_row_selected(Gtk.ListBoxRow? row) {
        debug("Updating selected");

        if (row == null) {
            this.selection.unselect_all();
            return;
        }

        this.selection.selected = row.get_index();
    }

    private void on_right_click(Gtk.GestureClick gesture, int n_press, double x, double y) {
        var row = this.list_box.get_row_at_y((int) y) as SidebarItem;
        if (row != null)
            row.show_popup_menu();
    }

    public Place? lookup_place_for_uri(string uri, out uint pos) {
        for (uint i = 0; i < this.selection.get_n_items(); i++) {
            var place = (Place) this.selection.get_item(i);
            if (place.uri == uri) {
                pos = i;
                return place;
            }
        }

        pos = 0;
        return null;
    }

    /**
     * Tries to find a place with the given URI scheme and selects it.
     *
     * @return Whether a matching place was found
     */
    public bool set_focused_place_for_scheme(string uri_scheme) {
        for (uint i = 0; i < this.selection.get_n_items(); i++) {
            var place = (Place) this.selection.get_item(i);
            if (place.uri.has_prefix(uri_scheme)) {
                this.selection.selected = i;
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
        if (this.selection.selected == Gtk.INVALID_LIST_POSITION) {
            // Select the first row
            this.selection.selected = 0;
            return;
        }

        this.selection.selected += positions;
    }
}

internal class Seahorse.SidebarItem : Gtk.ListBoxRow {

    public weak Seahorse.Place place { get; construct set; }

    construct {
        var box = new Gtk.Box(Gtk.Orientation.HORIZONTAL, 6);
        box.add_css_class("seahorse-sidebar-item");
        box.valign = Gtk.Align.CENTER;
        box.margin_top = 3;
        box.margin_bottom = 3;
        set_child(box);

        var label = new Gtk.Label(this.place.label);
        label.ellipsize = Pango.EllipsizeMode.END;
        label.xalign = 0f;
        label.hexpand = true;
        box.append(label);

        unowned var lockable = this.place as Lockable;
        if (lockable != null && (lockable.lockable || lockable.unlockable)) {
            var lock_button = new Gtk.Button();
            lock_button.add_css_class("flat");
            lock_button.halign = Gtk.Align.END;
            update_lock_button(lock_button);
            this.place.notify.connect((obj, pspec) => {
                update_lock_button(lock_button);
            });
            box.append(lock_button);
        }

         var long_press_gesture = new Gtk.GestureLongPress();
         long_press_gesture.pressed.connect(on_long_press);
         add_controller(long_press_gesture);
    }

    private void update_lock_button(Gtk.Button lock_button) {
        unowned var lockable = this.place as Lockable;

        if (lockable.unlockable) {
            lock_button.icon_name = "changes-prevent-symbolic";
            lock_button.tooltip_text = _("Unlock collection %s").printf(place.label);
            lock_button.action_name = "win.unlock-place";
            lock_button.action_target = new Variant.string(place.uri);
        } else if (lockable.lockable) {
            lock_button.icon_name = "changes-allow-symbolic";
            lock_button.tooltip_text = _("Lock collection %s").printf(place.label);
            lock_button.action_name = "win.lock-place";
            lock_button.action_target = new Variant.string(place.uri);
        }
    }

    public SidebarItem(Seahorse.Place place) {
        GLib.Object(place: place);
    }

    private void on_long_press(Gtk.GestureLongPress gesture, double x, double y) {
        show_popup_menu();
    }

    public void show_popup_menu() {
        var model = new GLib.Menu();

        // Start from the menu model provided by the this.place (if any)
        if (this.place.menu_model != null)
            model.append_section(null, this.place.menu_model);

        var uri = place.uri;

        // Lock and unlock items
        if (this.place is Lockable) {
            if (((Lockable) this.place).lockable)
                model.append(_("_Lock"), "win.lock-place('%s')".printf(uri));

            if (((Lockable) this.place).unlockable)
                model.append(_("_Unlock"), "win.unlock-place('%s')".printf(uri));
        }

        // Delete item
        if (this.place is Deletable && ((Deletable) this.place).deletable)
            model.append(_("_Delete"), "win.delete-place('%s')".printf(uri));

        // Properties item
        if (this.place is Viewable)
            model.append(_("_Properties"), "win.view-place('%s')".printf(uri));

        // Don't show a popover if the model is empty
        if (model.get_n_items() == 0)
            return;

        var popover = new Gtk.PopoverMenu.from_model(model);
        popover.set_parent(this);
        if (this.place.actions != null)
            popover.insert_action_group(this.place.action_prefix, this.place.actions);
        popover.popup();
    }
}
