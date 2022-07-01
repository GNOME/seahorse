/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

[GtkTemplate (ui = "/org/gnome/Seahorse/pkcs11-token-panel.ui")]
public class Seahorse.Pkcs11.TokenPanel : Seahorse.Panel {

    public Pkcs11.Token token { construct; get; }

    [GtkChild] private unowned Adw.ActionRow name_row;
    [GtkChild] private unowned Adw.ActionRow manufacturer_row;
    [GtkChild] private unowned Adw.ActionRow model_row;
    [GtkChild] private unowned Adw.ActionRow serial_nr_row;
    [GtkChild] private unowned Adw.ActionRow hardware_version_row;
    [GtkChild] private unowned Adw.ActionRow firmware_version_row;

    construct {
        this.token.bind_property("label", this.name_row, "subtitle", GLib.BindingFlags.SYNC_CREATE);

        unowned var info = token.info;
        if (info == null) {
            warning("Token has no info");
            return;
        }

        set_row_value(this.manufacturer_row, info.manufacturer_id);
        set_row_value(this.model_row, info.model);
        set_row_value(this.serial_nr_row, info.serial_number);

        set_row_value(this.hardware_version_row,
                      "%d.%d".printf(info.hardware_version_major,
                                     info.hardware_version_minor));
        if (info.firmware_version_major != 0 && info.firmware_version_minor != 0) {
            set_row_value(this.firmware_version_row,
                          "%d.%d".printf(info.firmware_version_major,
                                         info.firmware_version_minor));
        }
    }

    public TokenPanel(Token token) {
        GLib.Object(token: token);
    }

    public override void dispose() {
        unowned Gtk.Widget? child = null;
        while ((child = get_first_child()) != null)
            child.unparent();
        base.dispose();
    }

    private void set_row_value(Adw.ActionRow row,
                               string? value,
                               string fallback = _("Unknown")) {
        if (value != null && value != "") {
            row.subtitle = value;
            row.remove_css_class("dim-label");
            row.subtitle_selectable = true;
        } else {
            row.subtitle = fallback;
            row.add_css_class("dim-label");
            row.subtitle_selectable = false;
        }
    }
}
