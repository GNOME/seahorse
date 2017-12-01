/*
 * Seahorse
 *
 * Copyright (C) 2004-2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

public class Seahorse.Pgp.Signer : Gtk.Dialog {

    private Gtk.ComboBox signer_select;

    public Pgp.Key seahorse_signer_get (GtkWindow *parent) {
        Gcr.Collection collection = seahorse_keyset_pgp_signers_new ();
        uint nkeys = collection.get_length (collection);

        // If no signing keys then we can't sign
        if (nkeys == 0) {
            // TODO: We should be giving an error message that allows them to generate or import a key
            Seahorse.Util.show_error(null, _("No keys usable for signing"),
                    _("You have no personal PGP keys that can be used to sign a document or message."));
            return null;
        }

        // If only one key (probably default) then return it immediately
        if (nkeys == 1)
            return collection.get_objects().data;

        Seahorse.Widget swidget = seahorse_widget_new ("signer", parent);

        Gtk.ComboBox combo = GTK_WIDGET (seahorse_widget_get_widget (swidget, "signer-select"));
        g_return_val_if_fail (combo != null, null);
        ComboKeys.attach(combo, collection, null);

        GLib.Settings settings = Application.pgp_settings();

        // Select the last key used
        string id = settings.get_string("last-signer");
        string keyid;
        if (!id || !id[0])
            keyid = null;
        else if (id.has_prefix("openpgp:"))
            keyid = id + strlen ("openpgp:");
        else
            keyid = id;
        ComboKeys.set_active_id(combo, keyid);

        show();

        bool done = false, ok = false;
        while (!done) {
            switch (run()) {
                case Gtk.ResponseType.HELP:
                    break;
                case Gtk.ResponseType.OK:
                    ok = true;
                default:
                    done = true;
                    break;
            }
        }

        Pgp.Key? key = null;
        if (ok) {
            key = ComboKeys.get_active(combo);

            // Save this as the last key signed with
            settings.set_string("last-signer", (key != null)? key.keyid : null);
        }

        destroy();
        return key;
    }
}
