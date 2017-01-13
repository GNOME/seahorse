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

public class Seahorse.Ssh.KeyProperties : Gtk.Dialog {

    private Key key;

    // Used to make sure we don't start calling command unnecessarily
    private bool updating_ui = false;

    private Gtk.Image key_image;
    private Gtk.Entry comment_entry;
    private Gtk.Label id_label;
    private Gtk.Label trust_message;
    private Gtk.ToggleButton trust_check;

    private Gtk.Button passphrase_button;
    private Gtk.Button export_button;

    private Gtk.Label fingerprint_label;
    private Gtk.Label algo_label;
    private Gtk.Label location_label;
    private Gtk.Label strength_label;

    public KeyProperties(Key key, Gtk.Window parent) {
        GLib.Object(border_width: 5,
                    title: _("Key Properties"),
                    resizable: false,
                    transient_for: parent);
        this.key = key;

        load_ui();
        update_ui();

        // A public key only
        if (key.usage != Seahorse.Usage.PRIVATE_KEY) {
            passphrase_button.visible = false;
            export_button.visible = false;
        }

        this.key.notify.connect(() => update_ui());
    }

    // FIXME: normally we would do this using GtkTemplate, but this is quite hard with the current build setup
    private void load_ui() {
        Gtk.Builder builder = new Gtk.Builder();
        try {
            string path = "/org/gnome/Seahorse/seahorse-ssh-key-properties.ui";
            builder.add_from_resource(path);
        } catch (GLib.Error err) {
            GLib.critical("%s", err.message);
        }
        Gtk.Container content = (Gtk.Container) builder.get_object("ssh-key-properties");
        ((Gtk.Container)this.get_content_area()).add(content);

        this.key_image = (Gtk.Image) builder.get_object("key-image");
        this.comment_entry = (Gtk.Entry) builder.get_object("comment-entry");
        this.id_label = (Gtk.Label) builder.get_object("id-label");
        this.trust_message = (Gtk.Label) builder.get_object("trust-message");
        this.trust_check = (Gtk.ToggleButton) builder.get_object("trust-check");
        this.passphrase_button = (Gtk.Button) builder.get_object("passphrase-button");
        this.export_button = (Gtk.Button) builder.get_object("export-button");
        this.fingerprint_label = (Gtk.Label) builder.get_object("fingerprint-label");
        this.algo_label = (Gtk.Label) builder.get_object("algo-label");
        this.location_label = (Gtk.Label) builder.get_object("location-label");
        this.strength_label = (Gtk.Label) builder.get_object("strength-label");

        // Signals
        this.comment_entry.activate.connect(on_ssh_comment_activate);
        this.comment_entry.focus_out_event.connect(on_ssh_comment_focus_out);
        this.trust_check.toggled.connect(on_ssh_trust_toggled);
        this.passphrase_button.clicked.connect(on_ssh_passphrase_button_clicked);
        this.export_button.clicked.connect(on_ssh_export_button_clicked);
    }

    private void update_ui() {
        this.updating_ui = true;

        // Image
        this.key_image.set_from_icon_name(Seahorse.ICON_KEY_SSH, Gtk.IconSize.DIALOG);
        // Name and title
        this.comment_entry.text = this.key.label;
        this.title = this.key.label;
        // Key id
        this.id_label.label = this.key.identifier;
        // Put in message
        string template = this.trust_message.label;
        this.trust_message.set_markup(template.printf(Environment.get_user_name()));

        // Setup the check
        this.trust_check.active = (this.key.trust >= Seahorse.Validity.FULL);

        this.fingerprint_label.label = this.key.fingerprint;
        this.algo_label.label = this.key.get_algo().to_string() ?? _("Unknown type");
        this.location_label.label = this.key.get_location();
        this.strength_label.label = "%u".printf(this.key.get_strength());

        this.updating_ui = false;
    }

    public override void response(int response) {
        destroy();
    }

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

    private bool on_ssh_comment_focus_out(Gtk.Widget widget, Gdk.EventFocus event) {
        on_ssh_comment_activate((Gtk.Entry) widget);
        return false;
    }

    private void on_ssh_trust_toggled(Gtk.ToggleButton button) {
        if (updating_ui)
            return;

        button.sensitive = false;

        Source source = (Source) key.place;
        source.authorize_async.begin(key, button.active, (obj, res) => {
            try {
                source.authorize_async.end(res);
            } catch (GLib.Error e) {
                Seahorse.Util.show_error(this, _("Couldn’t change authorization for key."), e.message);
            }

            button.sensitive = true;
        });
    }

    private void on_ssh_passphrase_button_clicked (Gtk.Button button) {
        button.sensitive = false;

        ChangePassphraseOperation op = new ChangePassphraseOperation();
        op.change_passphrase_async.begin(key, null, (obj, res) => {
            try {
                op.change_passphrase_async.end(res);
            } catch (GLib.Error e) {
                Seahorse.Util.show_error(this, _("Couldn't change passphrase for key."), e.message);
            }

            button.sensitive = true;
        });
    }

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
}
