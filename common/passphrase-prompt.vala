/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004 - 2006 Stefan Walter
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

// TODO move these into a namespace or class
public const int SEAHORSE_PASS_BAD = 0x00000001;
public const int SEAHORSE_PASS_NEW = 0x01000000;

[GtkTemplate (ui = "/org/gnome/Seahorse/passphrase-prompt.ui")]
public class Seahorse.PassphrasePrompt : Adw.AlertDialog {

    /** Whether the user needs to confirm their password */
    public bool confirm { get; set; default = true; }

    [GtkChild] private unowned Gtk.PasswordEntry pass_entry;
    [GtkChild] private unowned Gtk.PasswordEntry confirm_entry;
    [GtkChild] private unowned Gtk.Label confirm_label;
    [GtkChild] private unowned Gtk.CheckButton check_option;
    [GtkChild] private unowned Gtk.Label error_label;

    public PassphrasePrompt(string? title, string? description, string? prompt, string? check, bool confirm) {
        GLib.Object(
            heading: utf8_validate(title) ?? _("Enter Password"),
            body: utf8_validate(description) ?? _("Please enter your password"),
            confirm: confirm
        );

        this.pass_entry.grab_focus();

        if (!confirm) {
            this.confirm_label.visible = false;
            this.confirm_entry.visible = false;
        }

        if (check != null) {
            this.check_option.label = check;
        } else {
            this.check_option.visible = false;
        }
    }

    // Kept for backwards compatibility with the C code
    public static PassphrasePrompt show_dialog(string? title, string? description, string? prompt,
                                               string? check, bool confirm) {
        return new PassphrasePrompt(title, description, prompt ?? _("Password:"), check, confirm);
    }

    /**
     * Prompts the user for a password. Returns null if cancelled
     */
    public async string? prompt(Gtk.Widget? parent, Cancellable? cancellable) {
        var response = yield choose(parent, cancellable);
        if (response == "submit")
            return this.pass_entry.text;
        return null;
    }

    public string get_text() {
        return this.pass_entry.text;
    }

    public bool checked() {
        return this.check_option.active;
    }

    // Convert passed text to utf-8 if not valid
    private static string? utf8_validate(string? text) {
        if (text == null)
            return null;

        if (text.validate())
            return text;

        string? result = text.locale_to_utf8(-1, null, null);
        if (result == null) {
            // Convert unknown characters into "?"
            char* p = (char*) text;

            while (!((string)p).validate (-1, out p))
                *p = '?';

            result = text;
        }
        return result;
    }

    [GtkCallback]
    private void on_pass_entry_activate(Gtk.Widget widget) {
        if (this.confirm)
            this.confirm_entry.grab_focus();
        else
            try_submit();
    }

    [GtkCallback]
    private void on_confirm_entry_activate(Gtk.Widget widget) {
        try_submit();
    }

    private void try_submit() {
        if (ok()) {
            response("submit");
        } else {
            this.error_label.visible = true;
        }
    }

    private bool ok() {
        return !this.confirm || this.pass_entry.text == this.confirm_entry.text;
    }

    [GtkCallback]
    private void on_entry_changed(Gtk.Editable? editable) {
        set_response_enabled("submit", ok());
    }

}
