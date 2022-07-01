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

/**
 * An interface for items that can be deleted (note however that specific
 * instances might not be, so use the deletable property).
 *
 * To actually delete the object, you'll have to call create_delete_operation()
 * which can then take care of it (possibly with others).
 */
public interface Seahorse.Deletable : GLib.Object {

    /** Returns whether this object is actually deletable */
    public abstract bool deletable { get; }

    public static bool can_delete(GLib.Object object) {
        if (object is Deletable)
            return ((Deletable) object).deletable;
        return false;
    }

    /**
     * Creates a {@link DeleteOperation} that will perform the actual deletion
     * of this object.
     */
    public abstract DeleteOperation create_delete_operation();
}
