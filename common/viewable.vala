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

    /**
     * Creates a Panel widget that can be used in a PanelWindow, or in another
     * widget (such as an import dialog).
     */
    public abstract Seahorse.Panel create_panel();

    /** Presents this item to the user */
    public virtual void view(Gtk.Window? parent = null) {
        Gtk.Window? window = get_data("viewable-window");
        if (window == null) {
            var panel = create_panel();
            window = new PanelWindow(panel, parent);

            set_data("viewable-window", window);
            window.close_request.connect((w) => {
                set_data_full("viewable-window", null, null);
                return false;
            });
        }

        window.present();
    }
}
