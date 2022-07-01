/*
 * Seahorse
 *
 * Copyright (C) 2004,2005 Stefan Walter
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

public interface Seahorse.Lockable : GLib.Object {
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
