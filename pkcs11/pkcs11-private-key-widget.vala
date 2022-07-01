/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2020 Niels De Graef
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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Author: Stef Walter <stefw@redhat.com>
 */

[GtkTemplate (ui = "/org/gnome/Seahorse/pkcs11-private-key-widget.ui")]
public class Seahorse.Pkcs11.PrivateKeyWidget : Adw.PreferencesPage {

    public Pkcs11.PrivateKey key { construct; get; }

    [GtkChild] private unowned Adw.ActionRow name_row;
    [GtkChild] private unowned Adw.ActionRow description_row;
    [GtkChild] private unowned Adw.ActionRow key_type_row;

    construct {
        update_name();
        set_row_value(this.description_row, _("Private Key"));
        set_row_value(this.key_type_row, this.key.get_key_type());

        this.key.notify["label"].connect((obj, pspec) => { update_name(); });
        update_name();
    }

    public PrivateKeyWidget(Pkcs11.PrivateKey key) {
        GLib.Object(key: key);
    }

    private void update_name() {
        set_row_value(this.name_row, this.key.get_cka_label(), _("Unnamed key"));
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
