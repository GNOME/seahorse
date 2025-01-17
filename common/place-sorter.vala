/*
 * Seahorse
 *
 * Copyright (C) 2023 Niels De Graef
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

public class Seahorse.PlaceSorter : Gtk.Sorter {

  public override Gtk.SorterOrder get_order() {
    return Gtk.SorterOrder.PARTIAL;
  }

  public override Gtk.Ordering compare(GLib.Object? item1, GLib.Object? item2) {
    unowned var obj1 = (Seahorse.Item) item1;
    unowned var obj2 = (Seahorse.Item) item2;

    var place1 = obj1.place;
    var place2 = obj2.place;

    if (place1 == place2)
        return Gtk.Ordering.EQUAL;

    return Gtk.Ordering.from_cmpfunc(strcmp(place1.uri, place2.uri));
  }
}
