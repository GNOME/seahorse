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

    public GenerateSelect(Gtk.Window? parent) {
        GLib.Object(
            use_header_bar: 1,
            transient_for: parent,
            modal: true
        );

        ListStore store = new ListStore(typeof(Gtk.Action));
        this.generate_list.bind_model(store, on_create_row);

        // Fill up the model
        var action_groups = (List<Gtk.ActionGroup>) Registry.object_instances("generator");
        foreach (var action_group in action_groups) {
            foreach (var action in action_group.list_actions())
                store.append(action);
        }
        store.sort((a, b) => {
            return ((Gtk.Action) a).label.collate(((Gtk.Action) b).label);
        });

        // Select first item
        this.generate_list.select_row(this.generate_list.get_row_at_index(0));
    }

    private Gtk.ListBoxRow on_create_row(GLib.Object item) {
        return new GenerateSelectRow((Gtk.Action) item);
    }

    [GtkCallback]
    private void on_row_activated(Gtk.ListBoxRow row) {
        Gtk.Action action = ((GenerateSelectRow) row).action;
        Action.activate_with_window(action, null, this.transient_for);
        destroy();
    }

    public override void response(int response)  {
        if (response != Gtk.ResponseType.OK)
            return;

        GenerateSelectRow? row = this.generate_list.get_selected_row() as GenerateSelectRow;
        if (row == null)
            return;

        Action.activate_with_window(row.action, null, this.transient_for);
    }
}

private class Seahorse.GenerateSelectRow : Gtk.ListBoxRow {
    private Gtk.Image icon;
    private Gtk.Label title;
    private Gtk.Label description;

    public Gtk.Action action { get; private set; }

    construct {
        var grid = new Gtk.Grid();
        grid.column_spacing = 6;
        grid.margin = 3;
        add(grid);

        this.icon = new Gtk.Image();
        this.icon.icon_size = Gtk.IconSize.DND;
        grid.attach(this.icon, 0, 0, 1, 2);

        this.title = new Gtk.Label(null);
        this.title.halign = Gtk.Align.START;
        grid.attach(this.title, 1, 0);

        this.description = new Gtk.Label(null);
        this.description.get_style_context().add_class("dim-label");
        grid.attach(this.description, 1, 1);
    }

    public GenerateSelectRow(Gtk.Action action) {
        this.action = action;

        this.title.set_markup("<b>%s</b>".printf(action.label));
        this.description.label = action.tooltip;

        if (action.gicon != null)
            this.icon.gicon = action.gicon;
        else if (action.icon_name != null)
            this.icon.icon_name = action.icon_name;

        show_all();
    }
}
