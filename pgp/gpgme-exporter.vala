/*
 * Seahorse
 *
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

namespace Seahorse {
namespace GpgME {

public class Exporter : GLib.Object, Seahorse.Exporter {

    private GLib.Object parent;
    private List<Pgp.Key> keys;

    /**
     * Armor encoding
     */
    public bool armor { get; set; }

    /**
     * Secret key export
     */
    public bool secret { get; set; }

    //XXX
    public string filename { owned get { return get_filename(); } }

    public string content_type {
        get {
            return this.armor ? "application/pgp-keys" : "application/pgp-keys+armor";
        }
    }

    public Gtk.FileFilter file_filter {
        owned get {
            Gtk.FileFilter filter = new Gtk.FileFilter();

            if (this.armor) {
                filter.set_name(_("Armored PGP keys"));
                filter.add_mime_type("application/pgp-keys+armor");
                filter.add_pattern("*.asc");
            } else {
                filter.set_name(_("PGP keys"));
                filter.add_mime_type("application/pgp-keys");
                filter.add_pattern("*.pgp");
                filter.add_pattern("*.gpg");
            }

            return filter;
        }
    }

    // XXX Gpg keys or PGP keys?
    public Exporter(Pgp.Key key, bool armor, bool secret) {
        GLib.Object(armor: armor,
                    secret: secret);
        g_return_val_if_fail (secret == false || armor == true, null);

        add_object(key);
    }

    // XXX Gpg keys or PGP keys?
    public Exporter.multiple(List<GpgME.Key> keys, bool armor) {
        GLib.Object(armor: armor,
                    secret: false);

        foreach (Pgp.Key key in keys)
            add_object(key);
    }

    static gchar *
    seahorse_gpgme_exporter_get_filename (SeahorseExporter* exporter)
    {
        const gchar *basename = null;
        gchar *filename;

        g_return_val_if_fail (this.keys, null);

        /* Multiple objects */
        if (this.keys.next)
            basename = _("Multiple Keys");
        else if (this.keys.data)
            basename = seahorse_object_get_nickname (this.keys.data);
        if (basename == null)
            basename = _("Key Data");

        if (this.armor)
            filename = g_strconcat (basename, ".asc", null);
        else
            filename = g_strconcat (basename, ".pgp", null);
        g_strstrip (filename);
        g_strdelimit (filename, SEAHORSE_BAD_FILENAME_CHARS, '_');
        return filename;
    }

    public unowned GLib.List<weak GLib.Object> get_objects() {
        return this.keys;
    }

    public bool add_object(GLib.Object object) {
        Key key = object as Key;
        if (key == null || (this.secret && key.privkey == null))
            return false;

        this.keys.append(key);
        notify_property("filename");
        return true;
    }

    public async uint8[] export(GLib.Cancellable? cancellable) throws GLib.Error {
        GPG.ErrorCode gerr = 0;
        GPG.Context gctx = seahorse_gpgme_keyring_new_context(out gerr);
        MemoryOutputStream output = new MemoryOutputStream.resizable();
        GPG.Data data = seahorse_gpgme_data_output (output);
        string[] keyids = new string[this.keys.length];
        int at = -1;

        if (seahorse_gpgme_propagate_error (gerr, &error)) {
            g_simple_async_result_take_error (res, error);
            g_simple_async_result_complete_in_idle (res);
            return;
        }

        gctx.set_armor(this.armor);

        foreach (Pgp.Key key in this.keys) {
            Seahorse.Progress.prep(cancellable, key.keyid, null);
            keyids += key.keyid;
        }

        if (this.secret) {
            if (!this.armor)
                return;

            gerr = GpgME.Operation.export_secret(gctx, keyids, data);
            if (seahorse_gpgme_propagate_error (gerr, &error))
                g_simple_async_result_take_error (res, error);
            g_simple_async_result_complete_in_idle (res);
            return;
        }

        GSource *gsource = seahorse_gpgme_gsource_new (cancellable);
        gsource.set_callback(on_keyring_export_complete);

        /* Get things started */
        if (on_keyring_export_complete (0, res))
            gsource.attach(g_main_context_default ());

        g_source_unref (gsource);
    }

    static bool on_keyring_export_complete (GPG.ErrorCode gerr) {
        GError *error = null;

        if (seahorse_gpgme_propagate_error (gerr, &error)) {
            g_simple_async_result_take_error (res, error);
            g_simple_async_result_complete (res);
            return false; /* don't call again */
        }

        if (closure.at >= 0)
            Seahorse.Progress.end(cancellable, keyids[at]);

        assert(at < keyids.length);
        at++;

        if (closure.at == keyids.length) {
            g_simple_async_result_complete (res);
            return false; /* don't run this again */
        }

        /* Do the next key in the list */
        gerr = GpgME.Operation.export_start(gctx, keyids[at], 0, data);

        if (seahorse_gpgme_propagate_error (gerr, &error)) {
            g_simple_async_result_take_error (res, error);
            g_simple_async_result_complete (res);
            return false; /* don't run this again */
        }

        Seahorse.Progress.begin(cancellable, keyids[at]);
        return true; /* call this source again */
    }

    static uint8[] seahorse_gpgme_exporter_export_finish(AsyncResult *result, out size_t size) {
        if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
            return null;

        closure = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
        g_output_stream_close (G_OUTPUT_STREAM (closure.output), null, null);
        *size = g_memory_output_stream_get_data_size (closure.output);
        return g_memory_output_stream_steal_data (closure.output);
    }
}

}
}
