/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2020 Niels De Graef
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

namespace Seahorse.Util {

    public void show_error (Gtk.Widget? parent,
                            string? heading,
                            string? message) {

        if (message == null)
            message = "";

        Gtk.Window? window = null;
        if (parent != null && !(parent is Gtk.Window)) {
            window = parent.get_root() as Gtk.Window;
        }

        var dialog = new Adw.AlertDialog(heading, message);
        dialog.add_response("close", _("_Close"));
        dialog.default_response = "close";
        dialog.present(parent);
    }

    public void toggle_action (GLib.SimpleAction action,
                               GLib.Variant?     variant,
                               void*             user_data) {
        var old_state = action.get_state();
        var new_state = new GLib.Variant.boolean(!old_state.get_boolean());
        action.change_state(new_state);
    }
}
