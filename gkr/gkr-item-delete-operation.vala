/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2020 Niels De Graef
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

public class Seahorse.Gkr.ItemDeleteOperation : DeleteOperation {

    public ItemDeleteOperation(Gkr.Item item) {
        add_item(item);
    }

    public void add_item(Gkr.Item item) {
        if (contains(item))
            return;
        this.items.add(item);
    }

    public override async bool execute(Cancellable? cancellable = null) throws GLib.Error {
        debug("Deleting %u passwords", this.items.length);
        foreach (unowned var deletable in this.items) {
            var item = (Gkr.Item) deletable;
            yield item.delete(cancellable);
        }
        return true;
    }
}
