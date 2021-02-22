/*
 * Seahorse
 *
 * Copyright (C) 2019 Niels De Graef
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

/**
 * A ItemList is a {@GLib.ListModel} which knows how to handle
 * {@link Gcr.Collection}s. These basically provide a compatibility wrapper
 * {@link GLib.ListModel}, lists that propagate changes to their listener (such
 * as a {@link Gtk.ListBox}).
 *
 * This list makes an assumption that each iten is a Seahorse.Object, or provides
 * the necessary properties like "label" and possibly "description".
 */
public class Seahorse.ItemList : GLib.Object, GLib.ListModel {

    /** The basic collection */
    public Gcr.Collection base_collection { get; private set; }
    // XXX put back on private

    /** The filtered and sorted list store */
    private GLib.GenericArray<GLib.Object> items = new GLib.GenericArray<GLib.Object>();

    public enum ShowFilter {
        ANY,
        PERSONAL,
        TRUSTED;

        public unowned string? to_string() {
            switch (this) {
                case ShowFilter.ANY:
                    return "";
                case ShowFilter.PERSONAL:
                    return "personal";
                case ShowFilter.TRUSTED:
                    return "trusted";
                default:
                    assert_not_reached();
            }
        }

        public static ShowFilter from_string(string? str) {
            switch (str) {
                case null:
                case "":
                case "any":
                    return ShowFilter.ANY;
                case "personal":
                    return ShowFilter.PERSONAL;
                case "trusted":
                    return ShowFilter.TRUSTED;
                default:
                    critical ("Got unknown ShowFilter string: %s", str);
                    assert_not_reached();
            }
        }
    }

    public ShowFilter showfilter { get; set; default = ShowFilter.ANY; }

    private string _filter_text = "";
    public string filter_text {
        set {
            if (this._filter_text == value)
                return;
            this._filter_text = value.casefold();
            refilter();
        }
    }

    public ItemList(Gcr.Collection collection) {
        this.base_collection = collection;

        // Make sure our model and the GcrCollection stay in sync
        collection.added.connect(on_collection_item_added);
        collection.removed.connect(on_collection_item_removed);

        // Add the existing elements
        foreach (weak GLib.Object obj in collection.get_objects())
            this.items.add(obj);

        // Sort afterwards
        this.items.sort(compare_items);

        // Notify listeners
        items_changed(0, 0, this.items.length);
    }

    private void on_collection_item_added(GLib.Object object) {
        // First check if the current filter wants this
        if (!item_matches_filters(object))
            return;

        int index = this.items.length;
        for (int i = 0; i < this.items.length; i++) {
            if (compare_items(object, this.items[i]) < 0) {
                index = i;
                break;
            }
        }
        this.items.insert(index, object);
        items_changed(index, 0, 1);
    }

    private void on_collection_item_removed(GLib.Object object) {
        uint index;
        if (this.items.find(object, out index)) {
            this.items.remove_index(index);
            items_changed(index, 1, 0);
        }
    }

    private bool item_matches_filters(GLib.Object object) {
        return matches_showfilter(object)
            && object_contains_filtered_text(object, this._filter_text);
    }

    private bool matches_showfilter(GLib.Object? obj) {
        Flags obj_flags = Flags.NONE;
        obj.get("object-flags", out obj_flags, null);

        switch (this.showfilter) {
            case ShowFilter.PERSONAL:
                return Seahorse.Flags.PERSONAL in obj_flags;
            case ShowFilter.TRUSTED:
                return Seahorse.Flags.TRUSTED in obj_flags;
            case ShowFilter.ANY:
                return true;
        }

        return false;
    }

    // Search through row for text
    private bool object_contains_filtered_text(GLib.Object? object, string? text) {
        // Empty search text results in a match
        if (text == null || text == "")
            return true;

        string? name = null;
        object.get("label", out name, null);
        if (name != null && (text in name.down()))
            return true;

        if (object.get_class().find_property("description") != null) {
            string? description = null;
            object.get("description", out description, null);
            if (description != null && (text in description.down()))
                return true;
        }

        return false;
    }

    private static int compare_items(GLib.Object gobj_a, GLib.Object gobj_b) {
        string? a_label = null, b_label = null;

        gobj_a.get("label", out a_label, null);
        gobj_b.get("label", out b_label, null);

        // Put (null) labels at the bottom
        if (a_label == null || b_label == null)
            return (a_label == null)? 1 : -1;

        return compare_labels(a_label, b_label);
    }

    public GLib.Object? get_item(uint position) {
        return this.items[position];
    }

    public GLib.Type get_item_type() {
        return typeof(GLib.Object);
    }

    public uint get_n_items () {
        return this.items.length;
    }

    /**
     * Updates the collection.
     * Automatically called when you change filter_text to another value
     */
    public void refilter() {
        // First remove all items
        var len = this.items.length;
        this.items.remove_range(0, len);

        // Add only the ones that match the filter
        foreach (weak GLib.Object obj in this.base_collection.get_objects()) {
            if (item_matches_filters(obj))
                this.items.add(obj);
        }

        // Sort afterwards
        this.items.sort(compare_items);

        // Notify listeners
        items_changed(0, len, this.items.length);

        debug("%u/%u elements visible after refilter on '%s'",
              this.items.length, this.base_collection.get_length(),
              this._filter_text);
    }

    // Compares 2 labels in an intuitive way
    // (case-insensitive; with respect to the user's locale)
    private static int compare_labels(string a_label, string b_label) {
        return a_label.casefold().collate(b_label.casefold());
    }
}
