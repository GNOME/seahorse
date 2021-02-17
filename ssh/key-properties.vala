/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2016 Niels De Graef
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

[GtkTemplate (ui = "/org/gnome/Seahorse/seahorse-ssh-key-properties.ui")]
public class Seahorse.Ssh.KeyProperties : Gtk.Dialog {

    private Key key;

    // Used to make sure we don't start calling command unnecessarily
    private bool updating_ui = false;

    [GtkChild]
    private unowned Gtk.Entry comment_entry;
    [GtkChild]
    private unowned Gtk.Switch trust_check;
    [GtkChild]
    private unowned Gtk.Button passphrase_button;
    [GtkChild]
    private unowned Gtk.Button export_button;
    [GtkChild]
    private unowned Gtk.Button copy_button;
    [GtkChild]
    private unowned Gtk.Label fingerprint_label;
    [GtkChild]
    private unowned Gtk.Label algo_label;
    [GtkChild]
    private unowned Gtk.Label location_label;
    [GtkChild]
    private unowned Gtk.Label pubkey_label;
    [GtkChild]
    private unowned Gtk.Label key_length_label;

    public KeyProperties(Key key, Gtk.Window parent) {
        GLib.Object(
            use_header_bar: 1,
            transient_for: parent
        );
        this.key = key;

        update_ui();

        // A public key only
        if (key.usage != Seahorse.Usage.PRIVATE_KEY) {
            this.passphrase_button.visible = false;
            this.export_button.visible = false;
            this.copy_button.visible = false;
        }

        this.key.notify.connect(() => update_ui());
    }

    private void update_ui() {
        this.updating_ui = true;

        // Name and title
        this.comment_entry.text = this.key.label;

        // Setup the check
        this.trust_check.active = (this.key.trust >= Seahorse.Validity.FULL);

        this.fingerprint_label.label = this.key.fingerprint;
        this.algo_label.label = this.key.get_algo().to_string() ?? _("Unknown type");
        this.location_label.label = this.key.get_location();
        this.key_length_label.label = "%u".printf(this.key.get_strength());
        this.pubkey_label.label = this.key.pubkey;

        this.updating_ui = false;
    }

    public override void response(int response) {
        destroy();
    }

    [GtkCallback]
    public void on_ssh_comment_activate(Gtk.Entry entry) {
        // Make sure not the same
        if (key.key_data.comment != null && entry.text == key.key_data.comment)
            return;

        entry.sensitive = false;

        RenameOperation op = new RenameOperation();
        op.rename_async.begin(key, entry.text, this, (obj, res) => {
            try {
                op.rename_async.end(res);
            } catch (GLib.Error e) {
                Seahorse.Util.show_error(this, _("Couldn’t rename key."), e.message);
                entry.text = key.key_data.comment ?? "";
            }

            entry.sensitive = true;
        });
    }

    [GtkCallback]
    private bool on_ssh_comment_focus_out(Gtk.Widget widget, Gdk.EventFocus event) {
        on_ssh_comment_activate((Gtk.Entry) widget);
        return false;
    }

    [GtkCallback]
    private void on_ssh_trust_changed(GLib.Object button, GLib.ParamSpec p) {
        if (updating_ui)
            return;

        this.trust_check.sensitive = false;

        Source source = (Source) key.place;
        source.authorize_async.begin(key, trust_check.active, (obj, res) => {
            try {
                source.authorize_async.end(res);
            } catch (GLib.Error e) {
                Seahorse.Util.show_error(this, _("Couldn’t change authorization for key."), e.message);
            }

            trust_check.sensitive = true;
        });
    }

    [GtkCallback]
    private void on_ssh_passphrase_button_clicked (Gtk.Button button) {
        button.sensitive = false;

        ChangePassphraseOperation op = new ChangePassphraseOperation();
        op.change_passphrase_async.begin(key, null, (obj, res) => {
            try {
                op.change_passphrase_async.end(res);
            } catch (GLib.Error e) {
                Seahorse.Util.show_error(this, _("Couldn’t change passphrase for key."), e.message);
            }

            button.sensitive = true;
        });
    }

    [GtkCallback]
    private void on_delete_clicked(Gtk.Button button) {
        var deleter = this.key.create_deleter();
        var ret = deleter.prompt(this);

        if (!ret)
            return;

        deleter.delete.begin(null, (obj, res) => {
            try {
                deleter.delete.end(res);
                this.destroy();
            } catch (GLib.Error e) {
                var dialog = new Gtk.MessageDialog(this,
                    Gtk.DialogFlags.MODAL,
                    Gtk.MessageType.ERROR,
                    Gtk.ButtonsType.OK,
                    _("Error deleting the SSH key."));
                dialog.run();
                dialog.destroy();
            }
        });
    }

    [GtkCallback]
    private void on_ssh_export_button_clicked (Gtk.Widget widget) {
        List<Exporter> exporters = new List<Exporter>();
        exporters.append(new Exporter(key, true));

        Seahorse.Exporter exporter;
        string directory = null;
        File file;
        if (Seahorse.Exportable.prompt(exporters, this, ref directory, out file, out exporter)) {
            exporter.export_to_file.begin(file, true, null, (obj, res) => {
                try {
                    exporter.export_to_file.end(res);
                } catch (GLib.Error e) {
                    Seahorse.Util.show_error(this, _("Couldn’t export key"), e.message);
                }
            });
        }
    }

    [GtkCallback]
    private void on_ssh_copy_button_clicked (Gtk.Widget widget) {
        var display = widget.get_display ();
        var clipboard = Gtk.Clipboard.get_for_display (display, Gdk.SELECTION_CLIPBOARD);

        clipboard.set_text(this.key.pubkey, -1);
    }
}
