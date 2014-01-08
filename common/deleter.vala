/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2012 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

namespace Seahorse {

public abstract class Deleter : GLib.Object {
	public abstract Gtk.Dialog create_confirm(Gtk.Window? parent);

	public abstract unowned GLib.List<weak GLib.Object> get_objects();

	public abstract bool add_object (GLib.Object obj);

	public abstract async bool delete(GLib.Cancellable? cancellable) throws GLib.Error;

	public bool prompt(Gtk.Window? parent) {
		var prompt = this.create_confirm(parent);
		int res = prompt.run();
		prompt.destroy();
		return res == Gtk.ResponseType.OK || res == Gtk.ResponseType.ACCEPT;
	}
}

}
