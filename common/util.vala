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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

namespace Seahorse {

namespace Util {

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

	public string get_display_date_string (long time)
	{
		if (time == 0)
			return "";
		var created_date = GLib.Date();
		created_date.set_time_t (time);
		var buffer = new char[128];
		created_date.strftime(buffer, _("%Y-%m-%d"));
		return (string)buffer;
	}

	public Gtk.Builder load_built_contents(Gtk.Container? frame,
	                                       string name) {
		var builder = new Gtk.Builder();
		string path = GLib.Path.build_filename(Config.UIDIR, "seahorse-%s.xml".printf(name));

		if (frame != null && frame is Gtk.Dialog)
			frame = ((Gtk.Dialog)frame).get_content_area();

		try {
			builder.add_from_file(path);
			var obj = builder.get_object(name);
			if (obj == null) {
				GLib.critical("Couldn't find object named %s in %s", name, path);
			} else if (frame != null) {
				var widget = (Gtk.Widget)obj;
				frame.add(widget);
				widget.show();
			}
		} catch (GLib.Error err) {
			GLib.critical("Couldn't load %s: %s", path, err.message);
		}

		return builder;
	}
}

}
