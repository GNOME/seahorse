/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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

/**
 * KeyServerControl: a control which allows you to select from a set
 * of keyservers.
 */
public class Seahorse.KeyserverDropdown : Adw.Bin {

    private Gtk.StringList base_model;
    private Gtk.SortListModel sorted_model;

    private unowned Gtk.DropDown dropdown;

    construct {
        // The base model corresponds to the entries in the "keyserver" setting
        // along with an empty string for "None"
        this.base_model = new Gtk.StringList(null);
        this.base_model.splice(0, 0, PgpSettings.instance().get_uris());
        this.base_model.append("");

        // Next, we define a SortListModel on top for each searching in the UI
        var expression = new Gtk.PropertyExpression(typeof(Gtk.StringObject),
                                                    null,
                                                    "string");
        var sorter = new Gtk.StringSorter(expression);
        this.sorted_model = new Gtk.SortListModel(this.base_model, (owned) sorter);

        // Now, on top of that, we map the empty string to something reasonable
        var map_model = new Gtk.MapListModel(sorted_model, map_model_cb);

        // Create the dropdown
        var drop_down = new Gtk.DropDown(map_model, expression);
        drop_down.notify["selected"].connect(on_dropdown_selected_changed);
        this.dropdown = drop_down;
        this.child = drop_down;

        // Now make sure we listen to changes
        PgpSettings.instance().changed["keyserver"].connect(on_keyservers_changed);
        AppSettings.instance().changed["server-publish-to"].connect(on_server_publish_to_changed);
    }

    private GLib.Object map_model_cb (owned GLib.Object item) {
        unowned var str_object = (Gtk.StringObject) item;
        if (str_object.string != "")
            return (owned) item;

        return new Gtk.StringObject(_("None: Don’t publish keys"));
    }

    private void on_dropdown_selected_changed(GLib.Object object, ParamSpec pspec) {
        var selected_item = this.sorted_model.get_item(this.dropdown.selected);
        var selected_str = ((Gtk.StringObject) selected_item).string;
        AppSettings.instance().server_publish_to = selected_str ?? "";
    }

    private void on_keyservers_changed (GLib.Settings settings,
                                        string key) {
        // Save the selected option
        var selected_item = this.sorted_model.get_item(this.dropdown.selected);
        var selected_str = ((Gtk.StringObject) selected_item).string;

        // Remove all except the "None" option and immedaitely add the new ones
        this.base_model.splice(0,
                               base_model.get_n_items() - 1,
                               PgpSettings.instance().get_uris());

        // Restore the selected option from before if possible
        for (uint i = 0; i < this.sorted_model.get_n_items(); i++) {
            var item = (Gtk.StringObject) this.sorted_model.get_item (i);
            if (item.string == selected_str) {
                this.dropdown.selected = i;
                break;
            }
        }
    }

    private void on_server_publish_to_changed (GLib.Settings settings,
                                               string key) {
        var server_publish_to = ((AppSettings) settings).server_publish_to;

        for (uint i = 0; i < this.sorted_model.get_n_items(); i++) {
            var item = (Gtk.StringObject) this.sorted_model.get_item (i);
            if (item.string == server_publish_to) {
                this.dropdown.selected = i;
                break;
            }
        }
    }
}
