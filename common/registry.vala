/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2013 Stefan Walter
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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

namespace Seahorse {

public class Registry : GLib.Object {
	private GLib.HashTable<string, GLib.HashTable<GLib.Object, GLib.Object>> _objects;

	private static Registry _singleton;

	private static Registry instance() {
		if (_singleton == null)
			_singleton = new Registry();
		return _singleton;
	}

	public static void cleanup() {
		_singleton = null;
	}

	public static void register_object(GLib.Object object,
	                                   string category) {
		Registry registry = Registry.instance();
		GLib.HashTable<GLib.Object, GLib.Object>? oset;

		oset = registry._objects.lookup(category);
		if (oset == null) {
			oset = new GLib.HashTable<GLib.Object, GLib.Object>(GLib.direct_hash, GLib.direct_equal);
			registry._objects.replace(category, oset);
		}

		oset.replace(object, object);
	}

	public static GLib.Object? object_instance(string category) {
		Registry registry = Registry.instance();
		GLib.HashTable<GLib.Object, GLib.Object>? oset;

		oset = registry._objects.lookup(category);
		if (oset == null)
			return null;

		var iter = HashTableIter<GLib.Object, GLib.Object> (oset);
		unowned GLib.Object key;
		unowned GLib.Object value;
		while (iter.next (out key, out value)) {
			return value;
		}

		return null;
	}

	public static GLib.List<GLib.Object> object_instances(string category) {
		Registry registry = Registry.instance();
		GLib.HashTable<GLib.Object, GLib.Object>? oset;
		GLib.List<GLib.Object> insts = null;

		oset = registry._objects.lookup(category);
		if (oset == null)
			return insts;

		var iter = HashTableIter<GLib.Object, GLib.Object> (oset);
		unowned GLib.Object key;
		unowned GLib.Object value;
		while (iter.next (out key, out value)) {
			insts.append(value);
		}

		return insts;
	}

	construct {
		_objects = new GLib.HashTable<weak string, GLib.HashTable<GLib.Object, GLib.Object>>(GLib.str_hash, GLib.str_equal);
	}
}

}
