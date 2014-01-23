/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2012 Stefan Walter
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

namespace Seahorse
{

public interface Viewable : GLib.Object {
	public abstract Gtk.Window? create_viewer(Gtk.Window? parent);

	public static bool can_view(GLib.Object object) {
		return object is Viewable;
	}

	public static bool view(GLib.Object object,
	                        Gtk.Window? parent) {
		if (!Viewable.can_view(object))
			return false;

		Gtk.Window? window = null;

		window = object.get_data("viewable-window");
		if (window == null) {
			var viewable = (Viewable)object;
			window = viewable.create_viewer(parent);
			if (window == null)
				return false;

			object.set_data("viewable-window", window);
			window.destroy.connect(() => {
				object.set_data_full("viewable-window", null, null);
			});
		}

		window.present();
		window.show();
		return true;
	}
}

}
