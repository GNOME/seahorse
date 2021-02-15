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
public class Seahorse.KeyManagerItemRow : Gtk.ListBoxRow {

    public GLib.Object object { get; construct set; }

    construct {
        var grid = new Gtk.Grid();
        grid.get_style_context().add_class("seahorse-item-listbox-row");
        add(grid);

        GLib.Icon? icon = null;
        object.get("icon", out icon);
        if (icon != null) {
            var img = new Gtk.Image.from_gicon(icon, Gtk.IconSize.DND);
            img.margin_end = 12;
            img.pixel_size = 32;
            grid.attach(img, 0, 0, 1, 2);
        }

        var markup_label = new Gtk.Label(null);
        object.bind_property("markup", markup_label, "label", BindingFlags.SYNC_CREATE);
        markup_label.use_markup = true;
        markup_label.halign = Gtk.Align.START;
        markup_label.xalign = 0.0f;
        markup_label.hexpand = true;
        markup_label.ellipsize = Pango.EllipsizeMode.END;
        grid.attach(markup_label, 1, 0);

        var description_label = new Gtk.Label(null);
        object.bind_property("description", description_label, "label", BindingFlags.SYNC_CREATE);
        description_label.xalign = 1.0f;
        description_label.valign = Gtk.Align.START;
        description_label.get_style_context().add_class("seahorse-item-listbox-row-description");
        grid.attach(description_label, 2, 0);

        show_all();
    }

    public KeyManagerItemRow(GLib.Object object) {
        GLib.Object(object: object);
    }
}
