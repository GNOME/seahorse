/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

namespace Seahorse {
namespace GpgME {

public class Revoke : Gtk.Dialog {

    private SubKey subkey;

    private Gtk.Combobox reason_combo;
    private Gtk.Entry description_entry;

    public enum RevokeReason {
        // No revocation reason
        NO_REASON = 0,
        // Key compromised
        COMPROMISED = 1,
        // Key replaced
        SUPERSEDED = 2,
        // Key no longer used
        NOT_USED = 3
    }

    private enum Column {
      TEXT,
      TOOLTIP,
      INT,
      N_COLUMNS
    }

    public void seahorse_gpgme_add_revoker_new (Key pkey, Gtk.Window parent) {
        g_return_if_fail (pkey != null);

        Key revoker = SEAHORSE_GPGME_KEY (seahorse_signer_get (parent));
        if (revoker == null)
            return;

        Gtk.Dialog dialog = new Gtk.MessageDialog(parent, Gtk.DialogFlags.MODAL,
                                                  Gtk.MessageType.WARNING, GTK_BUTTONS_YES_NO,
                                                  _("You are about to add %s as a revoker for %s. This operation cannot be undone! Are you sure you want to continue?"),
                                                  revoker.label, pkey.label);

        int response = dialog.run();
        dialog.destroy();

        if (response != Gtk.ResponseType.YES)
            return;

        GPG.Error err = KeyOperation.add_revoker(pkey, revoker);
        if (!err.is_success() || err.is_cancelled())
            Util.show_error(null, _("Couldn’t add revoker"), err.strerror());
    }

    public void seahorse_gpgme_revoke_new (GpgME.SubKey subkey, Gtk.Window parent) {
        Seahorse.Widget swidget = seahorse_widget_new ("revoke", parent);
        g_return_if_fail (swidget != null);

        this.title = _("Revoke: %s").printf(subkey.description);

        this.subkey = subkey;

        // Initialize List Store for the Combo Box
        Gtk.ListStore store = new Gtk.ListStore(Column.N_COLUMNS, typeof(string), typeof(string), typeof(int));

        Gtk.TreeIter iter;
        store.append(ref iter);
        store.set(iter, Column.TEXT, _("No reason"),
                        Column.TOOLTIP, _("No reason for revoking key"),
                        Column.INT, RevokeReason.NO_REASON, -1);

        store.append(ref iter);
        store.set(iter, Column.TEXT, _("Compromised"),
                        Column.TOOLTIP, _("Key has been compromised"),
                        Column.INT, RevokeReason.COMPROMISED, -1);

        store.append(ref iter);
        store.set(iter, Column.TEXT, _("Superseded"),
                        Column.TOOLTIP, _("Key has been superseded"),
                        Column.INT, RevokeReason.SUPERSEDED, -1);

        store.append(ref iter);
        store.set(iter, Column.TEXT, _("Not Used"),
                        Column.TOOLTIP, _("Key is no longer used"),
                        Column.INT, RevokeReason.NOT_USED, -1);

        // Finish Setting Up Combo Box
        this.reason_combo = builder.get_object("reason");
        this.reason_combo.model = store;
        this.reason_combo.active = 0;

        Gtk.CellRenderer renderer = new Gtk.CellRendererText();
        this.reason_combo.pack_start(renderer, true);
        this.reason_combo.set_attributes(renderer, "text", Column.TEXT, null);
    }

    // XXX G_MODULE_EXPORT
    private void on_gpgme_revoke_ok_clicked(Gtk.Button button) {
        Gtk.TreeIter iter;
        this.reason_combo.get_active_iter(out iter);

        Value val;
        this.reason_combo.model.get_value(iter, Column.INT, out val);
        RevokeReason reason = val.get_int();

        GPG.ErrorCode err = KeyOperation.revoke_subkey(this.subkey, reason, this.description_entry.text);
        if (!err.is_success() || err.is_cancelled())
            Util.show_error(null, _("Couldn’t revoke subkey"), err.strerror());
        swidget.destroy();
    }
}

}
}
