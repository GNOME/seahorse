/*
 * Seahorse
 *
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

public interface Seahorse.Viewable : GLib.Object {

    public static bool can_view(GLib.Object object) {
        return object is Viewable;
    }

    public abstract Seahorse.Panel create_panel();

    public static bool view(GLib.Object object,
                            Gtk.Window? parent) {
        if (!Viewable.can_view(object))
            return false;

        Gtk.Window? window = null;

        window = object.get_data("viewable-window");
        if (window == null) {
            unowned var viewable = (Viewable) object;
            var panel = viewable.create_panel();
            window = new PanelWindow(panel, parent);

            object.set_data("viewable-window", window);
            window.close_request.connect(() => {
                object.set_data_full("viewable-window", null, null);
                return false;
            });
        }

        window.present();
        return true;
    }
}
