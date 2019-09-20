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


// FIXME: there are no bindings generated for these functions
extern string gcr_secure_memory_strdup(string str);
extern void gcr_secure_memory_free(void* str);

public class Seahorse.Interaction : GLib.TlsInteraction {

    /**
     * The parent window
     */
    public weak Gtk.Window? parent { get; set; }

    public Interaction(Gtk.Window? parent) {
        this.parent = parent;
    }

    public override TlsInteractionResult ask_password(TlsPassword password,
                                                      Cancellable? cancellable = null)
            throws GLib.Error {
        string description = calc_description (password);
        PassphrasePrompt dialog = PassphrasePrompt.show_dialog(null, description, null, null, false);

        if (this.parent != null)
            dialog.transient_for = this.parent;

        int response = dialog.run();

        if (response == Gtk.ResponseType.ACCEPT)
            password.set_value_full((uint8[])gcr_secure_memory_strdup(dialog.get_text()),
                                    gcr_secure_memory_free);

        dialog.destroy();

        if (response != Gtk.ResponseType.ACCEPT)
            throw new GLib.IOError.CANCELLED("The password request was cancelled by the user");

        return TlsInteractionResult.HANDLED;
    }

    private string calc_description (GLib.TlsPassword password) {
        return _("Enter PIN or password for: %s").printf(password.description);
    }
}
