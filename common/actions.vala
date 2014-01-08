/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

namespace Seahorse {

public class Action {
	public static void pre_activate(Gtk.Action action,
	                                Catalog? catalog,
	                                Gtk.Window? window) {
		action.set_data("seahorse-action-window", window);
		action.set_data("seahorse-action-catalog", catalog);
	}

	public static void activate_with_window(Gtk.Action action,
	                                        Catalog? catalog,
	                                        Gtk.Window? window) {
		pre_activate(action, catalog, window);
		action.activate();
		post_activate(action);
	}

	public static void post_activate(Gtk.Action action) {
		action.set_data("seahorse-action-window", null);
		action.set_data("seahorse-action-catalog", null);
	}

	public static Gtk.Window? get_window(Gtk.Action action) {
		Gtk.Window? window = action.get_data("seahorse-action-window");
		return window;
	}

	public static Catalog? get_catalog(Gtk.Action action) {
		Catalog? catalog = action.get_data("seahorse-action-catalog");
		return catalog;
	}
}

public class Actions : Gtk.ActionGroup {
	public Catalog? catalog {
		owned get { return (Catalog)this._catalog.get(); }
		set { this._catalog.set(value); }
	}

	public string? definition {
		get { return this._definition; }
	}

	private unowned string? _definition;
	private WeakRef _catalog;

	Actions(string name) {
		GLib.Object(
			name: name
		);
	}

	public void register_definition (string definition) {
		this._definition = definition;
	}
}

}
