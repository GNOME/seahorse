/*
 * Seahorse
 *
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
 *
 * Author: Niels De Graef <nielsdegraef@gmail.com>
 */

/**
 * A Panel is the basic widget that is meant to be used for showing information
 * of a specific item in Seahorse. It can then be either integrated in some
 * component, or wrapped around with a simple window.
 */
public abstract class Seahorse.Panel : Gtk.Widget {

    /**
     * The title for this panel, usually the label of the item being shown
     */
    public string title { get; protected set; default = ""; }

    /**
     * The optional subtitle for this panel, for example a longer description
     */
    public string? subtitle { get; protected set; default = null; }

    /**
     * An optional menu model that can be exposed to the user (for example as a
     * menu button in a header bar). Note that by convention, we'll use the
     * "panel" prefix for actions.
     */
    public GLib.MenuModel? menu { get; protected set; default = null; }

    /**
     * An action group that is exported by the panel so it can be used for
     * example in the menu model expose by the menu property.
     */
    public SimpleActionGroup actions {
        get; protected set; default = new SimpleActionGroup();
    }

    /**
     * Fired if the parent wrapper should show a notification
     */
    public signal void notification(Adw.Toast toast);

    /** Little helper method */
    protected void simple_notification(string message) {
        var toast = new Adw.Toast(message);
        notification(toast);
    }

    construct {
        set_css_name("seahorse-panel");
    }
}
