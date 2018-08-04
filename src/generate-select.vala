/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

[GtkTemplate (ui = "/org/gnome/Seahorse/seahorse-generate-select.ui")]
public class Seahorse.GenerateSelect : Gtk.Dialog {
    private Gtk.ListStore store;
    [GtkChild]
    private Gtk.TreeView view;
    private List<Gtk.ActionGroup>? action_groups;

    private enum Column {
        ICON,
        TEXT,
        ACTION,
        N_COLUMNS
    }

    public GenerateSelect(Gtk.Window? parent) {
        GLib.Object(
            use_header_bar: 1,
            transient_for: parent,
            modal: true
        );
        this.store = new Gtk.ListStore(Column.N_COLUMNS, typeof(Icon), typeof(string), typeof(Gtk.Action));
        this.store.set_default_sort_func(on_list_sort);
        this.store.set_sort_column_id(Gtk.TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, Gtk.SortType.ASCENDING);

        this.action_groups = (List<Gtk.ActionGroup>) Registry.object_instances("generator");
        foreach (Gtk.ActionGroup action_group in this.action_groups) {
            foreach (weak Gtk.Action action in action_group.list_actions()) {
                string text = "<span size=\"larger\" weight=\"bold\">%s</span>\n%s"
                                  .printf(action.label, action.tooltip);

                Icon? icon = action.gicon;
                if (icon == null) {
                    if (action.icon_name != null)
                        icon = new ThemedIcon(action.icon_name);
                }

                Gtk.TreeIter iter;
                this.store.append(out iter);
                this.store.set(iter, Column.TEXT, text,
                                     Column.ICON, icon,
                                     Column.ACTION, action,
                                     -1);
            }
        }

        // Hook it into the view
        Gtk.CellRendererPixbuf pixcell = new Gtk.CellRendererPixbuf();
        pixcell.stock_size = Gtk.IconSize.DND;
        this.view.insert_column_with_attributes(-1, "", pixcell, "gicon", Column.ICON, null);
        this.view.insert_column_with_attributes(-1, "", new Gtk.CellRendererText(), "markup", Column.TEXT, null);
        this.view.set_model(this.store);

        // Select first item
        Gtk.TreeIter iter;
        this.store.get_iter_first(out iter);
        this.view.get_selection().select_iter(iter);

        this.view.row_activated.connect(on_row_activated);
    }

    private Gtk.Action? get_selected_action() {
        Gtk.TreeIter iter;
        Gtk.TreeModel? model;
        if (!this.view.get_selection().get_selected(out model, out iter))
            return null;

        Gtk.Action? action;
        this.store.get(iter, Column.ACTION, out action, -1);
        assert (action != null);

        return action;
    }

    private void on_row_activated(Gtk.TreeView view, Gtk.TreePath path, Gtk.TreeViewColumn col) {
        Gtk.Action? action = get_selected_action();
        if (action != null) {
            Action.activate_with_window(action, null, this.transient_for);
            destroy();
        }
    }

    public override void response(int response)  {
        Gtk.Action? action = (response == Gtk.ResponseType.OK)? get_selected_action() : null;
        Gtk.Window? parent = (action != null)? this.transient_for : null;

        if (action != null)
            Action.activate_with_window(action, null, parent);
    }

    private int on_list_sort (Gtk.TreeModel? model, Gtk.TreeIter a, Gtk.TreeIter b) {
        string? a_text = null, b_text = null;
        model.get(a, Column.TEXT, out a_text, -1);
        model.get(b, Column.TEXT, out b_text, -1);

        return a_text.collate(b_text);
    }
}
