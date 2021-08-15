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
}
