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

/**
 * The base interface for passwords/keys and others that are handled by Seahorse.
 */
public interface Seahorse.Item : GLib.Object, Seahorse.Viewable {

    /** The place this item originates from, or null if none/unknown */
    public abstract Place? place { owned get; set; }

    /** Icon for this Object */
    public abstract GLib.Icon? icon { get; }

    /** A displayable label */
    public abstract string title { get; }

    /** If available, can be used to display a bit more info with the title */
    public abstract string? subtitle { owned get; }

    /** A generic description of the type of item */
    public abstract string description { get; }

    /** How this item is used */
    public abstract Usage usage { get; }

    /** This item's flags */
    public abstract Flags item_flags { get; }
}
