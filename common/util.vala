/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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
		Gtk.Window? window = null;

		if (message == null)
			message = "";

		if (parent != null) {
			if (!(parent is Gtk.Window))
				parent = parent.get_toplevel();
			if (parent is Gtk.Window)
				window = (Gtk.Window)parent;
		}

		var dialog = new Gtk.MessageDialog(window, Gtk.DialogFlags.MODAL,
		                                   Gtk.MessageType.ERROR,
		                                   Gtk.ButtonsType.CLOSE, "");
		if (heading == null)
			dialog.set("text", message);
		else
			dialog.set("text", heading, "secondary-text", message);

		dialog.run();
		dialog.destroy();
	}

	public string get_display_date_string (uint64 time)
	{
		if (time == 0)
			return "";
		var created_date = new DateTime.from_unix_utc((int64) time);
		return created_date.format("%x");
	}

    // TODO: one we rely on GLib >= 2.54, we can use g_list_store_find
    public bool list_store_find(GLib.ListStore store, GLib.Object item, out uint position) {
        return list_store_find_with_equal_func (store, item, GLib.direct_equal, out position);
    }

    // TODO: one we rely on GLib >= 2.54, we can use g_list_store_find_with_equal_func
    public bool list_store_find_with_equal_func(GLib.ListStore store,
                                                GLib.Object item,
                                                GLib.EqualFunc func,
                                                out uint position) {
        for (uint i = 0; i < store.get_n_items(); i++) {
            if (func(store.get_item(i), item)) {
                if (&position != null)
                    position = i;
                return true;
            }
        }

        return false;
    }
}
