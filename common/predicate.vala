/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2017 Niels De Graef
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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

[CCode (has_target = false)]
public delegate bool Seahorse.PredicateFunc(GLib.Object obj, void* custom_target);

public struct Seahorse.Predicate {
    public Type type;
    public Usage usage;
    public Flags flags;
    public Flags nflags;
    public PredicateFunc custom;
    public void* custom_target;

    /**
     * Matches a seahorse object and a predicate
     *
     * @param obj The predicate to match
     *
     * @return false if predicate does not match the Object, true else
     */
    public bool match(GLib.Object obj) {
        // Check all the fields
        if (this.type != 0 && !(obj.get_type().is_a(this.type) && this.type.is_a(obj.get_type())))
            return false;

        if (this.usage != Usage.NONE) {
            Usage obj_usage = Usage.NONE;
            obj.get("usage", out obj_usage, null);

            if (this.usage != obj_usage)
                return false;
        }

        if (this.flags != 0 || this.nflags != 0) {
            Flags obj_flags = Flags.NONE;
            obj.get("object-flags", out obj_flags, null);

            if (this.flags != Flags.NONE && (obj_flags in this.flags))
                return false;
            if (this.nflags != Flags.NONE && (obj_flags in this.nflags))
                return false;
        }

        // And any custom stuff
        if (this.custom != null && !this.custom(obj, this.custom_target))
            return false;

        return true;
    }
}
