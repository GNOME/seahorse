/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2017 Niels De Graef
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

// XXX class or namespace?
// Also, maybe should be in GpgME.Keyring/ServerSource classes?
namespace Seahorse.Transfer {

public void transfer_keys_async(Place from, Place to, List keys, Cancellable? cancellable) {
    if (keys == null)
        return;

    string[] keyids = new string[keys.length];

    if (from is GpgME.Keyring) {
        keys = seahorse_object_list_copy (keys);
    } else {
        int i = 0;
        foreach (Pgp.Key key in keys) {
            keyids[i] = key.keyid;
            i++;
        }
    }

    Seahorse.Progress.prep(cancellable, from,
                           (from is GpgME.Keyring)? _("Exporting data") : _("Retrieving data"));
    Seahorse.Progress.prep(cancellable, to,
                           (to is GpgME.Keyring)? _("Importing data") : _("Sending data"));

    debug("starting export");

    // We delay and continue from a callback
    g_timeout_add_seconds_full (G_PRIORITY_DEFAULT, 0,
                                on_timeout_start_transfer,
                                g_object_ref (res), g_object_unref);
}

public void transfer_keyids_async(ServerSource from, Place to, string[] keyids,
                                  Cancellable? cancellable) throws GLib.Error {
    if (keyids == null || keyids.length == 0)
        return;

    Seahorse.Progress.prep(cancellable, from,
                           (from is GpgME.Keyring)? _("Exporting data") : _("Retrieving data"));
    Seahorse.Progress.prep(cancellable, to,
                           (to is GpgME.Keyring)? _("Importing data") : _("Sending data"));

    debug("starting export");

    /* We delay and continue from a callback */
    // XXX do this with Thread.run()
    g_timeout_add_seconds_full (G_PRIORITY_DEFAULT, 0,
                                on_timeout_start_transfer,
                                g_object_ref (res), g_object_unref);
}

private async void on_timeout_start_transfer() {
    GError *error = null;

    Seahorse.Progress.begin (closure.cancellable, from);

    uint8[] stream_data;
    if (from is ServerSource) {
        stream_data = yield ((ServerSource) from).export_async(keyids, cancellable);
    } else if (from is GpgME.Keyring) {
        Seahorse.Exporter exporter = new GpgME.Exporter.multiple(keys, true);
        stream_data = yield exporter.export(cancellable);
    } else {
        warning("unsupported source for transfer: %s", from.get_type().name());
        return;
    }

    debug("export done");
    Seahorse.Progress.end(closure.cancellable, from);

    if (error == null) // XXX correct?
        g_cancellable_set_error_if_cancelled (cancellable, &error);

    if (error != null) {
        debug("stopped after export");
        g_simple_async_result_take_error (res, error);
        return; // XXX or throw?
    }

    Seahorse.Progress.begin(cancellable, to);

    if (stream_data.length == 0) {
        debug("nothing to import");
        Seahorse.Progress.end(cancellable, to);
        return;
    }

    debug("starting import");
    InputStream input = new MemoryInputStream.from_data(stream_data);
    if (to is GpgMe.Keyring)
        yield ((GpgMe.Keyring) to).import_async(input, cancellable);
    else
        yield ((ServerSource) to).import_async(input, cancellable);
    debug("import done");

    Seahorse.Progress.end(closure.cancellable, &closure.to);

    if (results != null)
        g_cancellable_set_error_if_cancelled (closure.cancellable, &error);

    if (error != null)
        g_simple_async_result_take_error (res, error);
}

}
