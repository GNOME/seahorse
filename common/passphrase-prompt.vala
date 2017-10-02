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

public class Seahorse.PassphrasePrompt : Gtk.Dialog {
    // gnome hig small space in pixels
    private const int HIG_SMALL = 6;
    // gnome hig large space in pixels
    private const int HIG_LARGE = 12;

    private Gtk.Entry secure_entry;
    private Gtk.Entry? confirm_entry;
    private Gtk.CheckButton? check_option;

#if ! _DEBUG
    private bool keyboard_grabbed;
#endif

    public PassphrasePrompt (string? title, string? description, string prompt, string? check, bool confirm) {
        GLib.Object(
            title: title,
            modal: true,
            icon_name: Gtk.Stock.DIALOG_AUTHENTICATION
        );

        Gtk.Box wvbox = new Gtk.Box(Gtk.Orientation.VERTICAL, HIG_LARGE * 2);
        get_content_area().add(wvbox);
        wvbox.set_border_width(HIG_LARGE);

        Gtk.Box chbox = new Gtk.Box(Gtk.Orientation.HORIZONTAL, HIG_LARGE);
        wvbox.pack_start (chbox, false, false);

        // The image
        Gtk.Image img = new Gtk.Image.from_stock(Gtk.Stock.DIALOG_AUTHENTICATION, Gtk.IconSize.DIALOG);
        img.set_alignment(0.0f, 0.0f);
        chbox.pack_start(img, false, false);

        Gtk.Box box = new Gtk.Box(Gtk.Orientation.VERTICAL, HIG_SMALL);
        chbox.pack_start (box);

        // The description text
        if (description != null) {
            Gtk.Label desc_label = new Gtk.Label(utf8_validate (description));
            desc_label.set_alignment(0.0f, 0.5f);
            desc_label.set_line_wrap(true);
            box.pack_start(desc_label, true, false);
        }

        Gtk.Grid grid = new Gtk.Grid();
        grid.set_row_spacing(HIG_SMALL);
        grid.set_column_spacing(HIG_LARGE);
        box.pack_start(grid, false, false);

        // The first entry (if we have one)
        if (confirm) {
            Gtk.Label prompt_label = new Gtk.Label(utf8_validate (prompt));
            prompt_label.set_alignment(0.0f, 0.5f);
            grid.attach(prompt_label, 0, 0);

            this.confirm_entry = new Gtk.Entry.with_buffer(new Gcr.SecureEntryBuffer());
            this.confirm_entry.set_visibility(false);
            this.confirm_entry.set_size_request(200, -1);
            this.confirm_entry.activate.connect(confirm_callback);
            this.confirm_entry.changed.connect(entry_changed);
            grid.attach(this.confirm_entry, 1, 0);
            this.confirm_entry.grab_focus();
        }

        // The second and main entry
        Gtk.Label confirm_label = new Gtk.Label(utf8_validate (confirm? _("Confirm:") : prompt));
        confirm_label.set_alignment(0.0f, 0.5f);
        grid.attach(confirm_label, 0, 1);

        this.secure_entry = new Gtk.Entry.with_buffer(new Gcr.SecureEntryBuffer());
        this.secure_entry.set_size_request(200, -1);
        this.secure_entry.set_visibility(false);
        this.secure_entry.activate.connect(() => {
            if (get_widget_for_response(Gtk.ResponseType.ACCEPT).sensitive)
                response(Gtk.ResponseType.ACCEPT);
        });
        grid.attach(secure_entry, 1, 1);
        if (confirm)
            this.secure_entry.changed.connect(entry_changed);
        else
            this.secure_entry.grab_focus();

        // The checkbox
        if (check != null) {
            this.check_option = new Gtk.CheckButton.with_mnemonic(check);
            grid.attach(this.check_option, 1, 2);
        }

        grid.show_all();

        Gtk.Button cancel_button = new Gtk.Button.from_stock(Gtk.Stock.CANCEL);
        add_action_widget(cancel_button, Gtk.ResponseType.REJECT);
        cancel_button.set_can_default(true);

        Gtk.Button ok_button = new Gtk.Button.from_stock(Gtk.Stock.OK);
        add_action_widget(ok_button, Gtk.ResponseType.ACCEPT);
        ok_button.set_can_default(true);
        ok_button.grab_default();

        // Signals
        this.map_event.connect(grab_keyboard);
        this.unmap_event.connect(ungrab_keyboard);
        this.window_state_event.connect(window_state_changed);
        this.key_press_event.connect(key_press);

        set_position(Gtk.WindowPosition.CENTER);
        set_resizable(false);
        set_keep_above(true);
        show_all();
        get_window().focus(Gdk.CURRENT_TIME);

        if (confirm)
            entry_changed (null);
    }

    // Kept for backwards compatibility with the C code
    public static PassphrasePrompt show_dialog(string? title, string? description, string? prompt,
                                               string? check, bool confirm) {
        return new PassphrasePrompt(title, description, prompt ?? _("Password:"), check, confirm);
    }

    public string get_text() {
        return this.secure_entry.text;
    }

    public bool checked() {
        return this.check_option.active;
    }

    // Convert passed text to utf-8 if not valid
    private string? utf8_validate(string? text) {
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

    private bool key_press (Gtk.Widget widget, Gdk.EventKey event) {
        // Close the dialog when hitting "Esc".
        if (event.keyval == Gdk.Key.Escape) {
            response(Gtk.ResponseType.REJECT);
            return true;
        }

        return false;
    }

    private bool grab_keyboard (Gtk.Widget win, Gdk.Event event) {
#if ! _DEBUG
        if (!this.keyboard_grabbed)
            if (Gdk.keyboard_grab(win.get_window(), false, event.get_time()) != 0)
                message("could not grab keyboard");
        this.keyboard_grabbed = true;
#endif
        return false;
    }

    /* ungrab_keyboard - remove grab */
    private bool ungrab_keyboard (Gtk.Widget win, Gdk.Event event) {
#if ! _DEBUG
        if (this.keyboard_grabbed)
            Gdk.keyboard_ungrab(event.get_time());
        this.keyboard_grabbed = false;
#endif
        return false;
    }

    /* When enter is pressed in the confirm entry, move */
    private void confirm_callback(Gtk.Widget widget) {
        this.secure_entry.grab_focus();
    }

    private void entry_changed (Gtk.Editable? editable) {
        set_response_sensitive(Gtk.ResponseType.ACCEPT,
                               this.secure_entry.text == this.confirm_entry.text);
    }

    private bool window_state_changed (Gtk.Widget win, Gdk.EventWindowState event) {
        Gdk.WindowState state = win.get_window().get_state();

        if (Gdk.WindowState.WITHDRAWN in state ||
            Gdk.WindowState.ICONIFIED in state ||
            Gdk.WindowState.FULLSCREEN in state ||
            Gdk.WindowState.MAXIMIZED in state)
                ungrab_keyboard (win, event);
        else
            grab_keyboard (win, event);

        return false;
    }

}
