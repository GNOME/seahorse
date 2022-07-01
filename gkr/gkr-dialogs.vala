/*
 * Seahorse
 *
 * Copyright (C) 2009 Stefan Walter
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

namespace Seahorse.Gkr.Dialog {

private void update_wait_cursor(Gtk.Widget widget) {
    GLib.Cancellable? cancellable = widget.get_data("gkr-request");

    // No request active?
    if (cancellable == null) {
        widget.set_cursor(null);
        return;
    }

    // Get the wait cursor. Create a new one and cache it on the widget
    Gdk.Cursor? cursor = widget.get_data("wait-cursor");
    if (cursor == null) {
        cursor = new Gdk.Cursor.from_name ("wait", null);
        widget.set_data("wait-cursor", cursor);
    }

    // Indicate that we're loading stuff
    widget.set_cursor(cursor);
}

public GLib.Cancellable begin_request(Gtk.Widget dialog) {
    // Cancel any old operation going on
    complete_request (dialog, true);

    // Start the operation and tie it to the widget so that it will get
    // cancelled if the widget is destroyed before the operation is complete
    var cancellable = new GLib.Cancellable ();
    dialog.set_data_full ("gkr-request", cancellable.ref(), (data) => {
        GLib.Cancellable? c = (GLib.Cancellable?)data;
        c.cancel();
        c.unref();
    });

    if (dialog.get_realized())
        update_wait_cursor (dialog);
    else
        dialog.realize.connect(update_wait_cursor);

    dialog.set_sensitive(false);
    return cancellable;
}

public void complete_request(Gtk.Widget dialog, bool cancel) {
    GLib.Cancellable? cancellable = dialog.steal_data ("gkr-request");
    if (cancellable != null && cancel)
        cancellable.cancel();
    if (dialog.get_realized())
        update_wait_cursor (dialog);
    dialog.set_sensitive(true);
}
}
