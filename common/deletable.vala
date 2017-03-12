/*
 * Seahorse
 *
 * Copyright (C) 2004,2005 Stefan Walter
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
 */

namespace Seahorse {

public interface Deletable : GLib.Object {
	public abstract Deleter create_deleter();
	public abstract bool deletable { get; }

	public static bool can_delete(GLib.Object obj) {
		if (obj is Deletable)
			return ((Deletable)obj).deletable;
		return false;
	}

	public static int delete_with_prompt_wait(GLib.List<GLib.Object> objects,
	                                          Gtk.Window? parent) throws GLib.Error {

		var loop = new GLib.MainLoop(null, false);
		GLib.AsyncResult? result = null;
		int count = 0;

		/* A table for monitoring which objects are still pending */
		var pending = new GLib.GenericSet<weak GLib.Object>(GLib.direct_hash, GLib.direct_equal);
		foreach (var obj in objects)
			pending.add(obj);

		foreach (var obj in objects) {
			if (!pending.contains(obj))
				continue;

			if (!Deletable.can_delete (obj)) {
				pending.remove(obj);
				continue;
			}

			var deleter = ((Deletable)obj).create_deleter();

			/* Now go and add all pending to each exporter */
			foreach (var x in objects) {
				if (x == obj)
					continue;
				if (pending.contains(x))
					deleter.add_object(x);
			}

			/* Now show a prompt choosing between the exporters */
			var ret = deleter.prompt(parent);
			if (!ret)
				break;

			deleter.delete.begin(null, (obj, res) => {
				result = res;
				loop.quit();
			});

			loop.run();

			if (deleter.delete.end(result)) {
				foreach (var x in deleter.get_objects()) {
					pending.remove(x);
					count++;
				}
			} else {
				break;
			}
		}

		return count;
	}
}

}
