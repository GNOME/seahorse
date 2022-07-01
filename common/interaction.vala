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

    /** The parent window */
    public weak Gtk.Window? parent { get; set; }

    public Interaction(Gtk.Window? parent) {
        this.parent = parent;
    }

    public override async TlsInteractionResult ask_password_async(TlsPassword password,
                                                                  Cancellable? cancellable = null)
            throws GLib.Error {

        debug("Asking user for password");
        string description = calc_description(password);

        var dialog = new PassphrasePrompt(null, description, null, null, false);
        var pass = yield dialog.prompt(this.parent, cancellable);
        if (pass != null) {
            debug("Setting password");
            password.set_value_full((uint8[])gcr_secure_memory_strdup(pass),
                                    gcr_secure_memory_free);
        } else {
            debug("Failed to set password, cancelled");
            throw new GLib.IOError.CANCELLED("The password request was cancelled by the user");
        }

        return TlsInteractionResult.HANDLED;
    }

    private string calc_description (GLib.TlsPassword password) {
        return _("Enter PIN or password for: %s").printf(password.description);
    }
}
