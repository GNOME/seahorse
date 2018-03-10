/*
 * Seahorse
 *
 * Copyright (C) 2018 Niels De Graef
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
 * An entry specifically for password: the buffer is saved in a
 * {@link Gcr.SecureEntryBuffer}, a secondary icon allows the user to hide or
 * show the password.
 */
public class Seahorse.Gkr.PasswordEntry : Gtk.Entry {

    public bool has_changed { get; set; default = false; }

    public PasswordEntry() {
        GLib.Object (
            buffer: new Gcr.SecureEntryBuffer(),
            visibility: false,
            visible: true,
            hexpand: true,
            secondary_icon_name: "dialog-password-symbolic",
            secondary_icon_tooltip_text: _("Show/Hide password"),
            secondary_icon_sensitive: true
        );

        this.icon_press.connect((pos, event) => {
            this.visibility = !this.visibility;
        });
        this.changed.connect(() => {
            this.has_changed = true;
        });
    }

    public void set_initial_password (string? password) {
        this.text = password;
        this.has_changed = false;
    }
}
