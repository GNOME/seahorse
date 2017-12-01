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

public class Seahorse.GpgME.AddUid : Gtk.Dialog {

    private Key key;

    private Gtk.Entry name_entry;
    private Gtk.Entry email_entry;
    private Gtk.Entry comment_entry;
    private Gtk.Button ok_button;

    /**
     * Creates a new dialog for adding a user ID to key.
     */
    public AddUid(Key key, Gtk.Window parent) {
        this.key = key;
        this.transient_for = parent;

        this.title = _("Add user ID to %s").printf(this.key.label);

        load_ui();
    }

    private void load_ui() {
        Gtk.Builder builder = new Gtk.Builder();
        try {
            string path = "/org/gnome/Seahorse/gpgme-add-uid.ui";
            builder.add_from_resource(path);
        } catch (GLib.Error err) {
            GLib.critical("%s", err.message);
        }
        Gtk.Container content = (Gtk.Container) builder.get_object("add-uid");
        ((Gtk.Container)this.get_content_area()).add(content);

        this.name_entry = (Gtk.Entry) builder.get_object("name_entry");
        this.email_entry = (Gtk.Entry) builder.get_object("email_entry");
        this.comment_entry = (Gtk.Entry) builder.get_object("comment_entry");
        this.ok_button = (Gtk.Button) builder.get_object("ok_button");

        // Signals
        this.name_entry.changed.connect(() => check_ok_button());
        this.email_entry.changed.connect(() => check_ok_button());
        this.ok_button.clicked.connect(on_ok_button_clicked);
    }

    private void check_ok_button() {
        this.ok_button.sensitive = this.name_entry.text.length >= 5
                                   && (this.email_entry.text.length == 0
                                       || Regex.match_simple ("?*@?*", this.email_entry.text));
    }

    private void on_ok_button_clicked(Gtk.Button button) {
        GPG.Error err = KeyOperation.add_uid(this.key,
                                             this.name_entry.text,
                                             this.email_entry.text,
                                             this.comment_entry.text);
        if (!err.is_success() || err.is_cancelled())
            Util.show_error(null, _("Couldn’t add user id"), err.strerror());
        else
            destroy();
    }
}
