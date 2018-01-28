/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004, 2005, 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2018 Niels De Graef
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

public class Seahorse.KeyManagerStore : Gcr.CollectionModel {

    private const string XDS_FILENAME = "xds.txt";
    private const size_t MAX_XDS_ATOM_VAL_LEN = 4096;
    private static Gdk.Atom XDS_ATOM = Gdk.Atom.intern("XdndDirectSave0", false);
    private static Gdk.Atom TEXT_ATOM = Gdk.Atom.intern("text/plain", false);

    public enum Mode {
        ALL,
        FILTERED
    }

    private enum DragInfo {
        TEXT,
        XDS,
    }

    private const Gtk.TargetEntry[] store_targets = {
        { "text/plain", 0, DragInfo.TEXT },
        { "XdndDirectSave0", 0, DragInfo.XDS }
    };

    private uint filter_stag;

    private string? drag_destination;
    private GLib.Error? drag_error;
    private List<GLib.Object>? drag_objects;

    /**
     * Manager Settings
     */
    public GLib.Settings settings { get; construct set; }

    /**
     * Key store mode controls which keys to display
     */
    public Mode filter_mode {
        get { return this._filter_mode; }
        set {
            if (this._filter_mode != value) {
                this._filter_mode = value;
                refilter_later();
            }
        }
    }
    private Mode _filter_mode;

    /**
     * Key store filter for when in filtered mode
     */
    public string? filter {
        get { return (this.filter_mode == Mode.FILTERED)? this._filter : ""; }
        set {
            // If we're not in filtered mode and there is text OR
            // we're in filtered mode (regardless of text or not)
            // then update the filter
            if ((this._filter_mode != Mode.FILTERED && value != null && value != "")
                || (this._filter_mode == Mode.FILTERED)) {
                this._filter_mode = Mode.FILTERED;

                // We always use lower case text (see filter_callback)
                this._filter = value.down();
                refilter_later ();
            }
        }
    }
    private string? _filter;

    private enum Column {
        ICON,
        MARKUP,
        LABEL,
        DESCRIPTION,
        N_COLS;
    }

    // This is a bit ugly, but we can't use const here due to the typeof-s
    private static Gcr.Column[] _columns = {
        Gcr.Column() { property_name = "icon", property_type = typeof(Icon), column_type = typeof(Icon) },
        Gcr.Column() { property_name = "markup", property_type = typeof(string), column_type = typeof(string) },
        Gcr.Column() { property_name = "label", property_type = typeof(string), column_type = typeof(string), flags = Gcr.ColumnFlags.SORTABLE },
        Gcr.Column() { property_name = "description", property_type = typeof(string), column_type = typeof(string) },
        Gcr.Column()
    };


    construct {
        // FIXME: this is (again) due to broken bindings.
        set("columns", _columns,
            "mode", Gcr.CollectionModelMode.LIST,
            null);
    }

    public KeyManagerStore (Gcr.Collection? collection, Gtk.TreeView? view, Predicate? pred, GLib.Settings settings) {
        Collection filtered = new Collection.for_predicate (collection, pred, null);
        pred.custom = on_filter_visible;
        pred.custom_target = this;

        GLib.Object (
            collection: filtered,
            settings: settings
        );

        // The sorted model is the top level model
        view.model = this;

        // add the icon column
        Gtk.CellRendererPixbuf icon_renderer = new Gtk.CellRendererPixbuf();
        icon_renderer.stock_size = Gtk.IconSize.DND;
        icon_renderer.ypad = 6;
        icon_renderer.yalign = 0.0f;
        Gtk.TreeViewColumn? col = new Gtk.TreeViewColumn.with_attributes ("", icon_renderer, "gicon", Column.ICON, null);
        col.set_resizable(false);
        view.append_column(col);

        // Name column
        col = new Gtk.TreeViewColumn();

        Gtk.CellRendererText text_renderer = new Gtk.CellRendererText();
        text_renderer.ypad = 6;
        text_renderer.yalign = 0.0f;
        text_renderer.ellipsize = Pango.EllipsizeMode.END;
        col.pack_start(text_renderer, true);
        col.set_attributes(text_renderer, "markup", Column.MARKUP, null);

        text_renderer = new Gtk.CellRendererText();
        text_renderer.ypad = 6;
        text_renderer.xpad = 3;
        text_renderer.yalign = 0.0f;
        text_renderer.xalign = 1.0f;
        text_renderer.scale = Pango.Scale.SMALL;
        text_renderer.alignment = Pango.Alignment.RIGHT;
        col.pack_start(text_renderer, false);
        col.set_attributes(text_renderer, "markup", Column.DESCRIPTION, null);
        col.set_resizable(true);
        col.set_expand(true);
        view.append_column(col);
        col.set_sort_column_id(Column.LABEL);

        // Use predicate to figure out which columns to add
        if (collection is Collection)
            pred = ((Collection) collection).predicate;
        else
            pred = null;

        // Also watch for sort-changed on the store
        this.sort_column_changed.connect(on_sort_column_changed);

        // Update sort order in case the sorted column was added
        string? sort_by = settings.get_string("sort-by");
        if (sort_by != null)
            set_sort_to(sort_by);

        view.set_enable_search(false);
        view.set_show_expanders(false);
        view.set_rules_hint(true);
        view.set_headers_visible(false);

        set_sort_column_id (Column.LABEL, Gtk.SortType.ASCENDING);

        // Tree drag
        Egg.TreeMultiDrag.add_drag_support (view);

        view.drag_data_get.connect(drag_data_get);
        view.drag_begin.connect(drag_begin);
        view.drag_end.connect(drag_end);

        Gtk.drag_source_set (view, Gdk.ModifierType.BUTTON1_MASK, store_targets, Gdk.DragAction.COPY);
    }

    ~KeyManagerStore() {
        SignalHandler.disconnect_by_func((void*) this, (void*) on_sort_column_changed, (void*) this);
    }

    // Search through row for text
    private bool object_contains_filtered_text (GLib.Object? object, string? text) {
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

    // Called to filter each row
    private static bool on_filter_visible (GLib.Object? obj, void* custom_target) {
        KeyManagerStore self = (KeyManagerStore) custom_target;

        // Check the row requested
        switch (self.filter_mode) {
            case Mode.FILTERED:
                return self.object_contains_filtered_text (obj, self.filter);
            case Mode.ALL:
                return true;
            default:
                assert_not_reached();
        };
    }

    public void refilter() {
        ((Collection) get_collection()).refresh();
    }

    // Refilter the tree after a timeout has passed
    private void refilter_later() {
        if (this.filter_stag != 0)
            Source.remove (this.filter_stag);

        this.filter_stag = Timeout.add (200, () => {
            this.filter_stag = 0;
            refilter();
            return false;
        });
    }

    // Update the sort order for a column
    private void set_sort_to(string name) {
        // Prefix with a minus means descending
        Gtk.SortType ord = (name[0] == '-')? Gtk.SortType.DESCENDING : Gtk.SortType.ASCENDING;

        string without_sign = name;
        if (name[0] == '-' || name[0] == '+')
            without_sign = name.substring(1);

        // Find the column sort id
        int id = -1;
        for (int i = Column.N_COLS - 1; i >= 0 ; i--) {
            string? n = (string?) _columns[i].user_data;
            if (n != null && name == n) {
                id = i;
                break;
            }
        }

        if (id != -1)
            set_sort_column_id(id, ord);
    }

    // Called when the column sort is changed
    private void on_sort_column_changed (Gtk.TreeSortable sort) {
        if (this.settings == null)
            return;

        // We have a sort so save it
        int column_id;
        Gtk.SortType ord;
        if (get_sort_column_id (out column_id, out ord)) {
            if (column_id >= 0 && column_id < Column.N_COLS) {
                if (_columns[column_id].user_data != null) {
                    string sign = (ord == Gtk.SortType.DESCENDING)? "-" : "";
                    this.settings.set_string("sort-by", sign + (string) _columns[column_id].user_data);
                }
            }

        // No sort so save blank
        } else if (this.settings != null) {
            this.settings.set_string ("sort-by", "");
        }
    }

    /* The following three functions taken from bugzilla
     * (http://bugzilla.gnome.org/attachment.cgi?id=49362&action=view)
     * Author: Christian Neumair
     * Copyright: 2005 Free Software Foundation, Inc
     * License: GPL */
    private string? xds_get_atom_value (Gdk.DragContext? context) {
        if (context == null)
            return null;

        Gdk.Window source_window = context.get_source_window();

        uint8[] ret;
        int len;
        if (Gdk.property_get (source_window, XDS_ATOM, TEXT_ATOM, 0, MAX_XDS_ATOM_VAL_LEN,
                              0, null, out len, out ret)) {
            ret[len]='\0';
            return (string) ret;
        }

        return null;
    }

    private bool xds_context_offers_target (Gdk.DragContext? context, Gdk.Atom target) {
        return context.list_targets().find(target) != null;
    }

    private bool xds_is_dnd_valid_context (Gdk.DragContext? context) {
        if (context == null)
            return false;

        string? tmp = null;
        if (xds_context_offers_target (context, XDS_ATOM))
            tmp = xds_get_atom_value (context);

        return (tmp != null);
    }

    private void drag_begin (Gtk.Widget widget, Gdk.DragContext context) {
        debug("drag_begin -.");

        this.drag_destination = null;
        this.drag_error = null;
        this.drag_objects = null;

        this.drag_objects = KeyManagerStore.get_selected_objects((Gtk.TreeView) widget);

        if (this.drag_objects != null) {
            Gdk.Window? source_window = context.get_source_window();
            Gdk.property_change (source_window, XDS_ATOM, TEXT_ATOM, 8, Gdk.PropMode.REPLACE,
                                 (uint8[]) XDS_FILENAME, XDS_FILENAME.length);
        }

        debug("drag_begin <--");
    }

    private bool export_to_text(Gtk.SelectionData selection_data) {
        if (this.drag_objects == null)
            return false;
        debug("exporting to text");

        uint8[] output;
        uint count = 0;
        try {
            count = Exportable.export_to_text_wait(this.drag_objects, out output);
        } catch (Error e) {
            this.drag_error = e;
            message ("error occurred on export: %s", this.drag_error.message);
            return false;
        }

        // TODO: Need to print status if only partially exported

        if (count > 0) {
            debug("setting selection text");
            selection_data.set_text ((string) output, output.length);
            return true;
        }

        message ("no objects exported");
        return false;
    }

    private bool export_to_directory (string directory) {
        debug("exporting to %s", directory);
        try {
            Exportable.export_to_directory_wait(this.drag_objects, directory);
        } catch (Error e) {
            this.drag_error = e;
            return false;
        }
        return true;
    }

    private void drag_data_get (Gtk.Widget widget, Gdk.DragContext context, Gtk.SelectionData selection_data,
                   uint info, uint time) {
        if (this.drag_objects == null)
            return;

        debug("drag_data_get %u -.", info);

        // The caller wants plain text
        if (info == DragInfo.TEXT) {
            debug("returning object text");
            export_to_text(selection_data);

        // The caller wants XDS
        } else if (info == DragInfo.XDS) {

            if (xds_is_dnd_valid_context (context)) {
                string? destination = xds_get_atom_value (context);
                if (destination == null)
                    return;
                this.drag_destination = Path.get_dirname (destination);

                selection_data.set(selection_data.get_target(), 8, (uint8[]) "S");
            }

        // Unrecognized format
        } else {
            debug("Unrecognized format: %u", info);
        }

        debug("drag_data_get <--");
    }

    private void drag_end(Gtk.Widget? widget, Gdk.DragContext? context) {
        debug("drag_end -.");

        if (this.drag_destination != null && this.drag_error == null)
            export_to_directory(this.drag_destination);

        if (this.drag_error != null) {
            DBusError.strip_remote_error (this.drag_error);
            Util.show_error (widget, _("Couldn’t export keys"), this.drag_error.message);
        }

        this.drag_error = null;
        this.drag_objects = null;
        this.drag_destination = null;

        debug("drag_end <--");
    }

    public static GLib.Object? get_object_from_path (Gtk.TreeView view, Gtk.TreePath? path) {
        Gtk.TreeIter? iter;
        if (view.model.get_iter (out iter, path))
            return null;
        return ((Gcr.CollectionModel) view.model).object_for_iter(iter);
    }

    public List<GLib.Object> get_all_objects() {
        return get_collection().get_objects();
    }

    public static List<GLib.Object> get_selected_objects (Gtk.TreeView view) {
        List<Gtk.TreePath> paths = view.get_selection().get_selected_rows(null);

        // make object list
        List<GLib.Object> objects = new List<GLib.Object>();
        foreach (Gtk.TreePath path in paths) {
            GLib.Object? obj = KeyManagerStore.get_object_from_path (view, path);
            if (obj != null)
                objects.append(obj);
        }

        // Remove duplicates
        objects.sort((a, b) => {
            if (a == b)
                return 0;
            return ((void*) a > (void*) b)? 1 : -1;
        });

        unowned List<GLib.Object>? l = null;
        for (l = objects; l != null; l = l.next) {
            while (l.next != null && l.data == l.next.data)
                objects.delete_link(l.next);
        }

        return objects;
    }

    public static void set_selected_objects(Gtk.TreeView view, List<GLib.Object> objects) {
        Gtk.TreeSelection? selection = view.get_selection();
        selection.unselect_all();

        bool first = true;
        foreach (GLib.Object obj in objects) {
            Gtk.TreeIter iter = Gtk.TreeIter();
            if (((Gcr.CollectionModel) view.model).iter_for_object (obj, iter)) {
                selection.select_iter(iter);

                // Scroll the first row selected into view
                if (first) {
                    Gtk.TreePath? path = view.model.get_path(iter);
                    view.scroll_to_cell(path, null, false, 0.0f, 0.0f);
                    first = false;
                }
            }
        }
    }

    public GLib.Object? get_selected_object (Gtk.TreeView? view) {
        List<Gtk.TreePath>? paths = view.get_selection().get_selected_rows(null);

        // choose first object
        GLib.Object? obj = null;
        if (paths != null)
            obj = get_object_from_path(view, paths.data);

        return obj;
    }
}
