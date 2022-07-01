/*
 * Seahorse
 *
 * Copyright (C) 2004,2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2012 Stefan Walter
 * Copyright (C) 2023 Niels De Graef
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
 * An interface for items that can be exported.
 *
 * To actually export the object, you'll have to call create_export_operation()
 * which can then take care of it (possibly exporting it with other items at
 * the same time).
 */
public interface Seahorse.Exportable : GLib.Object {

    /** Returns whether this object is actually exportable */
    public abstract bool exportable { get; }

    /** Helper to know if a given object is exportable */
    public static bool can_export(GLib.Object object) {
        if (object is Exportable)
            return ((Exportable)object).exportable;
        return false;
    }

    /**
     * Creates a {@link ExportOperation} that will do the actual export of this
     * file.
     */
    public abstract ExportOperation create_export_operation()
        requires (this.exportable);
}
