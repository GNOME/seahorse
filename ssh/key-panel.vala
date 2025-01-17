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

[GtkTemplate (ui = "/org/gnome/Seahorse/ssh-key-panel.ui")]
public class Seahorse.Ssh.KeyPanel : Seahorse.Panel {

    public Key key { get; construct set; }

    // Used to make sure we don't start calling command unnecessarily
    private bool updating_ui = false;

    [GtkChild] private unowned Adw.EntryRow comment_row;

    [GtkChild] private unowned Adw.ActionRow algo_row;
    [GtkChild] private unowned Adw.ActionRow key_length_row;
    [GtkChild] private unowned Adw.ActionRow location_row;
    [GtkChild] private unowned Adw.ActionRow fingerprint_row;

    [GtkChild] private unowned Gtk.Label pubkey_label;

    [GtkChild] private unowned Adw.SwitchRow trust_check;

    private const ActionEntry[] ACTIONS = {
        { "copy-public-key", action_copy_public_key },
        { "generate-qr-code", action_generate_qr_code },
        { "change-passphrase", action_change_passphrase },
        { "export-secret-key", action_export_secret_key },
        { "remote-upload", action_upload_key },
        { "delete-key", action_delete_key },
    };

    construct {
        this.actions.add_action_entries(ACTIONS, this);
        insert_action_group("panel", this.actions);

        update_ui();

        // A public key only
        if (key.usage != Seahorse.Usage.PRIVATE_KEY) {
            var action = (SimpleAction) this.actions.lookup_action("change-passphrase");
            action.set_enabled(false);
            action = (SimpleAction) this.actions.lookup_action("export-secret-key");
            action.set_enabled(false);
        }

        this.key.notify.connect((obj, pspec) => update_ui());
    }

    public KeyPanel(Key key) {
        GLib.Object(key: key);
    }

    public override void dispose() {
        unowned Gtk.Widget? child = null;
        while ((child = get_first_child()) != null)
            child.unparent();
        base.dispose();
    }

    private void update_ui() {
        this.updating_ui = true;

        // Name and title
        this.comment_row.text = this.key.title;

        // Setup the check
        this.trust_check.active = (this.key.trust >= Seahorse.Validity.FULL);

        this.fingerprint_row.subtitle = "<span font=\"monospace\">%s</span>".printf(this.key.fingerprint);
        this.algo_row.subtitle = this.key.algo.to_string() ?? _("Unknown type");
        this.location_row.subtitle = this.key.location;
        this.key_length_row.subtitle = "%u".printf(this.key.strength);
        this.pubkey_label.label = this.key.pubkey;

        this.updating_ui = false;
    }

    [GtkCallback]
    public void on_ssh_comment_apply(Adw.EntryRow entry) {
        // Make sure not the same
        if (key.key_data.comment != null && entry.text == key.key_data.comment)
            return;

        entry.sensitive = false;

        unowned var toplevel = get_root() as Gtk.Window;

        RenameOperation op = new RenameOperation();
        op.rename_async.begin(key, entry.text, toplevel, (obj, res) => {
            try {
                op.rename_async.end(res);
            } catch (GLib.Error e) {
                Util.show_error(toplevel, _("Couldn’t rename key."), e.message);
                entry.text = key.key_data.comment ?? "";
            }

            entry.sensitive = true;
        });
    }

    [GtkCallback]
    private void on_ssh_trust_changed(GLib.Object button, GLib.ParamSpec p) {
        if (updating_ui)
            return;

        this.trust_check.sensitive = false;

        Source source = (Source) key.place;

        unowned var toplevel = get_root() as Gtk.Window;
        source.authorize_async.begin(key, trust_check.active, (obj, res) => {
            try {
                source.authorize_async.end(res);
            } catch (GLib.Error e) {
                Util.show_error(toplevel, _("Couldn’t change authorization for key."), e.message);
            }

            trust_check.sensitive = true;
        });
    }

    private void action_change_passphrase(SimpleAction action, Variant? param) {
        var op = new ChangePassphraseOperation();
        op.change_passphrase_async.begin(this.key, null, (obj, res) => {
            try {
                op.change_passphrase_async.end(res);
            } catch (GLib.Error e) {
                Seahorse.Util.show_error(this, _("Couldn’t change passphrase for key."), e.message);
            }
        });
    }

    private void action_upload_key(SimpleAction action, Variant? param) {
        var dialog = new UploadRemoteDialog(this.key);
        dialog.key_uploaded.connect(() => {
            simple_notification(_("Remote Access successfully configured"));
        });
        dialog.error.connect((message) => {
            simple_notification(_("Error setting up remote access"));
            warning(message);
        });
        dialog.present(get_root() as Gtk.Window);
    }

    private void action_delete_key(SimpleAction action, Variant? param) {
        unowned var toplevel = get_root() as Gtk.Window;

        var delete_op = this.key.create_delete_operation();
        delete_op.execute_interactively.begin(toplevel, null, (obj, res) => {
            try {
                delete_op.execute_interactively.end(res);
            } catch (GLib.IOError.CANCELLED e) {
                debug("Deletion of key cancelled by user");
            } catch (GLib.Error e) {
                Util.show_error(toplevel, _("Couldn’t delete key"), e.message);
            }
        });
    }

    private void action_export_secret_key(SimpleAction action, Variant? param) {
        export.begin(true, (obj, res) => {
            export.end(res);
        });
    }

    private async void export(bool secret)
            requires (this.key is Exportable) {

        unowned var toplevel = get_root() as Gtk.Window;

        var export_op = new Ssh.KeyExportOperation(this.key, secret);
        try {
            var prompted = yield export_op.prompt_for_file(toplevel, null);
            if (!prompted) {
                debug("no file picked by user");
                return;
            }

            yield export_op.execute(null);
        } catch (GLib.IOError.CANCELLED e) {
            debug("Exporting of key cancelled by user");
        } catch (GLib.Error e) {
            Util.show_error(toplevel, _("Couldn’t export key"), e.message);
        }
    }

    private void action_copy_public_key(SimpleAction action, Variant? param) {
        get_clipboard().set_text(this.key.pubkey);
        simple_notification(_("Copied public key to clipboard"));
    }

    private void action_generate_qr_code(SimpleAction action, Variant? param) {
        var qr_code_dialog = new QrCodeDialog(this.key.pubkey);
        qr_code_dialog.title = this.key.title;
        qr_code_dialog.present(get_root() as Gtk.Window);
    }
}
