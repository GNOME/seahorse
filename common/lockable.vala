/*
 * Seahorse
 *
 * Copyright (C) 2004,2005 Stefan Walter
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

public interface Lockable : GLib.Object {
	public abstract bool lockable { get; }
	public abstract bool unlockable { get; }

	public abstract async bool lock(GLib.TlsInteraction? interaction,
	                                GLib.Cancellable? cancellable) throws GLib.Error;

	public abstract async bool unlock(GLib.TlsInteraction? interaction,
	                                  GLib.Cancellable? cancellable) throws GLib.Error;

	public static bool can_lock(GLib.Object object) {
		if (object is Lockable)
			return ((Lockable)object).lockable;
		return false;
	}

	public static bool can_unlock(GLib.Object object) {
		if (object is Lockable)
			return ((Lockable)object).unlockable;
		return false;
	}
}

}
