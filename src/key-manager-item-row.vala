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
 * Represents an item in the KeyManager's (i.e. the main window) list of items.
 */
[GtkTemplate (ui = "/org/gnome/Seahorse/key-manager-item-row.ui")]
public class Seahorse.KeyManagerItemRow : Gtk.ListBoxRow {

    private Seahorse.Item? _item;
    public Seahorse.Item? item {
        get { return this._item; }
        set {
            if (this._item == value)
                return;

            unbind();
            this._item = value;
            bind();
        }
    }
    private ulong notify_handler = 0;

    [GtkChild] private unowned Gtk.Image icon;
    [GtkChild] private unowned Gtk.Label title_label;
    [GtkChild] private unowned Gtk.Label subtitle_label;
    [GtkChild] private unowned Gtk.Label description_label;

    public KeyManagerItemRow(Seahorse.Item item) {
        Object(item: item);
    }

    private void bind() {
        this.notify_handler = item.notify.connect((obj, pspec) => {
            update_details();
        });
        update_details();
    }

    private void update_details() {
        if (this.item.icon != null)
            this.icon.gicon = this.item.icon;

        this.title_label.label = this.item.title;

        var subtitle = this.item.subtitle;
        if (subtitle != null && subtitle != "") {
            this.subtitle_label.label = this.item.subtitle;
            this.subtitle_label.visible = true;
        } else {
            this.subtitle_label.label = "";
            this.subtitle_label.visible = false;
        }

        this.description_label.label = this.item.description;
    }

    private void unbind() {
        if (this.notify_handler != 0)
            this.item.disconnect(this.notify_handler);
        this.notify_handler = 0;
    }
}
