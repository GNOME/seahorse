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

/**
 * The GenerateSelect dialog is the dialog that users get to see when they
 * want to create a new item. It fills up the list with items by looking at
 * the ActionGroups that added themselves to the {@link Seahorse.Registry}
 * as generator.
 */
[GtkTemplate (ui = "/org/gnome/Seahorse/seahorse-generate-select.ui")]
public class Seahorse.GenerateSelect : Gtk.Dialog {

    [GtkChild]
    private Gtk.ListBox generate_list;

    private SimpleActionGroup action_group = new SimpleActionGroup();

    public GenerateSelect(Gtk.Window? parent) {
        GLib.Object(
            use_header_bar: 1,
            transient_for: parent,
            modal: true
        );

        ListStore store = new ListStore(typeof(Action));
        this.generate_list.bind_model(store, on_create_row);

        // Fetch and process the generator actions
        var actions = (List<Action>) Registry.object_instances("generator");
        foreach (var action in actions) {
            this.action_group.add_action(action);
            store.insert_sorted(action, compare_generator_actions);
        }
        insert_action_group("gen", action_group);

        // Select first item (and grab focus, so user can use the keyboard immediately)
        weak Gtk.ListBoxRow? row = this.generate_list.get_row_at_index(0);
        if (row != null) {
            this.generate_list.select_row(row);
            row.grab_focus();
        }
    }

    private int compare_generator_actions(GLib.Object a, GLib.Object b) {
        unowned string? a_label = a.get_data("label");
        unowned string? b_label = b.get_data("label");
        return a_label.collate(b_label);
    }

    private void switch_view(Gtk.Action action) {
        string target = action.action_group.name.split("-", 2)[0];
        ((KeyManager) this.transient_for).set_focused_place(target);
    }

    private Gtk.ListBoxRow on_create_row(GLib.Object item) {
        return new GenerateSelectRow((Action) item);
    }

    public override void response(int response)  {
        if (response != Gtk.ResponseType.OK)
            return;

        GenerateSelectRow? row = this.generate_list.get_selected_row() as GenerateSelectRow;
        if (row == null)
            return;

        row.activate();
    }

    [GtkCallback]
    private void on_row_activated(Gtk.ListBox listbox, Gtk.ListBoxRow row) {
        var generate_row = (GenerateSelectRow) row;
        this.action_group.activate_action(generate_row.action_name, null);
        destroy();
    }
}

private class Seahorse.GenerateSelectRow : Gtk.ListBoxRow {
    private Gtk.Label title;
    private Gtk.Label description;

    // Note that we can't use the actual "action-name" property,
    // or the row-activated signal doesn't get emitted for some reason
    public unowned string? action_name;

    construct {
        var grid = new Gtk.Grid();
        grid.column_spacing = 6;
        grid.margin = 3;
        add(grid);

        this.title = new Gtk.Label(null);
        this.title.halign = Gtk.Align.START;
        grid.attach(this.title, 0, 0);

        this.description = new Gtk.Label(null);
        this.description.get_style_context().add_class("dim-label");
        grid.attach(this.description, 0, 1);
    }

    public GenerateSelectRow(Action action) {
        this.action_name = action.name;

        unowned string? label = action.get_data<string?>("label");

        this.title.set_markup("<b>%s</b>".printf(label));
        this.description.label = action.get_data<string?>("description");

        show_all();
    }
}
