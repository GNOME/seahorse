/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2018 Niels De Graef
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

[GtkTemplate (ui = "/org/gnome/Seahorse/seahorse-gkr-add-item.ui")]
public class Seahorse.Gkr.ItemAdd : Gtk.Dialog {
    [GtkChild]
    private unowned Gtk.ComboBox item_keyring_combo;
    [GtkChild]
    private unowned Gtk.Container password_area;
    private Gtk.Entry password_entry;
    [GtkChild]
    private unowned Gtk.Entry item_entry;
    [GtkChild]
    private unowned Gtk.LevelBar password_strength_bar;
    [GtkChild]
    private unowned Gtk.Image password_strength_icon;

    private PasswordQuality.Settings pwquality = new PasswordQuality.Settings();

    construct {
        // Load up a list of all the keyrings, and select the default
        var store = new Gtk.ListStore(2, typeof(string), typeof(Secret.Collection));
        this.item_keyring_combo.set_model(store);

        var cell = new Gtk.CellRendererText();
        this.item_keyring_combo.pack_start(cell, true);
        this.item_keyring_combo.add_attribute(cell, "text", 0);

        foreach (var keyring in Backend.instance().get_keyrings()) {
            Gtk.TreeIter iter;
            store.append(out iter);
            store.set(iter, 0, keyring.label,
                            1, keyring);
            if (keyring.is_default)
                this.item_keyring_combo.set_active_iter(iter);
        }

        set_response_sensitive(Gtk.ResponseType.ACCEPT, false);

        this.password_entry = new PasswordEntry();
        this.password_entry.visibility = false;
        this.password_entry.changed.connect(on_password_entry_changed);
        this.password_area.add(this.password_entry);
        this.password_entry.show();
    }

    public ItemAdd(Gtk.Window? parent) {
        GLib.Object(
            transient_for: parent,
            use_header_bar: 1
        );
    }

    [GtkCallback]
    private void on_add_item_entry_changed (Gtk.Editable entry) {
        set_response_sensitive(Gtk.ResponseType.ACCEPT, this.item_entry.text != "");
    }

    private void on_password_entry_changed (Gtk.Editable entry) {
        void* auxerr;
        int score = this.pwquality.check(entry.get_chars(), null, null, out auxerr);

        if (score < 0) {
            PasswordQuality.Error err = ((PasswordQuality.Error) score);
            this.password_strength_icon.tooltip_text = dgettext("libpwquality", err.to_string(auxerr));
            this.password_strength_icon.show();
        } else {
            this.password_strength_icon.hide();
        }

        this.password_strength_bar.value = ((score / 25) + 1).clamp(1, 5);
    }

    public override void response(int resp) {
        if (resp != Gtk.ResponseType.ACCEPT)
            return;

        Gtk.TreeIter iter;
        if (!this.item_keyring_combo.get_active_iter(out iter))
            return;

        Secret.Collection collection;
        this.item_keyring_combo.model.get(iter, 1, out collection, -1);

        var keyring = (Keyring) collection;
        var cancellable = new Cancellable();
        var interaction = new Interaction(this);
        var item_buffer = this.item_entry.buffer;
        var secret_buffer = this.password_entry.buffer;

        keyring.unlock.begin(interaction, cancellable, (obj, res) => {
            try {
                if (keyring.unlock.end(res)) {
                    create_secret(item_buffer, secret_buffer, collection);
                }
            } catch (Error e) {
                Util.show_error(this, _("Couldn’t unlock"), e.message);
            }
        });
    }

    private void create_secret(Gtk.EntryBuffer item_buffer,
                               Gtk.EntryBuffer secret_buffer,
                               Secret.Collection collection) {
        var secret = new Secret.Value(secret_buffer.text, -1, "text/plain");
        var cancellable = Dialog.begin_request(this);
        var attributes = new HashTable<string, string>(GLib.str_hash, GLib.str_equal);

        /* TODO: Workaround for https://bugzilla.gnome.org/show_bug.cgi?id=697681 */
        var schema = new Secret.Schema("org.gnome.keyring.Note", Secret.SchemaFlags.NONE);

        Secret.Item.create.begin(collection, schema, attributes,
                                 item_buffer.text, secret, Secret.ItemCreateFlags.NONE,
                                 cancellable, (obj, res) => {
            try {
                /* Clear the operation without cancelling it since it is complete */
                Dialog.complete_request(this, false);

                Secret.Item.create.end(res);
            } catch (GLib.Error err) {
                Util.show_error(this, _("Couldn’t add item"), err.message);
            }
        });
    }
}
