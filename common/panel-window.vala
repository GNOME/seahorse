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
 * Basically a window that wraps a {@link Seahorse.Panel}
 */
[GtkTemplate (ui = "/org/gnome/Seahorse/panel-window.ui")]
public class Seahorse.PanelWindow : Adw.Window {

    [GtkChild] private unowned Adw.HeaderBar header;
    [GtkChild] private unowned Adw.WindowTitle window_title;

    [GtkChild] private unowned Adw.ToastOverlay toast_overlay;
    [GtkChild] private unowned Adw.Clamp clamp;

    public PanelWindow(Seahorse.Panel panel,
                       Gtk.Window? parent = null) {
        GLib.Object(transient_for: parent);

        this.clamp.child = panel;

        // Create header
        panel.bind_property("title",
                            this.window_title, "title",
                            BindingFlags.SYNC_CREATE);
        panel.bind_property("subtitle",
                            this.window_title, "subtitle",
                            BindingFlags.SYNC_CREATE);

        if (panel.menu != null) {
            var menu_button = new Gtk.MenuButton();
            menu_button.icon_name = "open-menu-symbolic";
            menu_button.menu_model = panel.menu;
            this.header.pack_start(menu_button);
        }

        insert_action_group("panel", panel.actions);

        panel.notification.connect(on_panel_notification);
    }

    private void on_panel_notification(Seahorse.Panel panel, Adw.Toast toast) {
        this.toast_overlay.add_toast(toast);
    }
}
