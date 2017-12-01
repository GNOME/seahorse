/* 
 * Seahorse
 * 
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

namespace Seahorse {
namespace GpgME {

public class Deleter : Seahorse.Deleter {
    private List<Key> keys;

    public Deleter(Key item) {
        this.keys = new List<Key>();

        if (!add_object(item))
            assert_not_reached();
    }

    public override Gtk.Dialog create_confirm(Gtk.Window? parent) {
        uint num = this.keys.length;
        string prompt;
        if (num == 1) {
            prompt = _("Are you sure you want to permanently delete %s?")
                        .printf(this.keys.data.label);
        } else {
            prompt = ngettext("Are you sure you want to permanently delete %d keys?",
                              "Are you sure you want to permanently delete %d keys?",
                              num).printf(num);
        }

        return new Seahorse.DeleteDialog(parent, "%s", prompt);
    }

    public override unowned List<weak GLib.Object> get_objects() {
        return this.keys;
    }

    public override bool add_object(GLib.Object object) {
        if (!(object is Key))
            return false;

        this.keys.append((Key) object);
        return true;
    }

    public override async bool delete(GLib.Cancellable? cancellable) throws GLib.Error {
        foreach (Key key in this.keys) {
            if (cancellable != null && cancellable.is_cancelled())
                break;

            GPG.ErrorCode error = KeyOperation.delete(key);
            if (seahorse_gpgme_propagate_error (gerr, &error)) {
                g_simple_async_result_take_error (res, error);
                return false;
            }
        }

        return true;
    }
}

}
}
