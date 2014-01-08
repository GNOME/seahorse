/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

namespace Seahorse {

public interface Backend : Gcr.Collection {
	public abstract string name { get; }
	public abstract string label { get; }
	public abstract string description { get; }
	public abstract Gtk.ActionGroup actions { owned get; }

	public abstract Place lookup_place(string uri);

	public void register() {
		Registry.register_object(this, "backend");
	}

	public static GLib.List<Backend> get_registered() {
		return (GLib.List<Seahorse.Backend>)Registry.object_instances("backend");
	}
}

}
