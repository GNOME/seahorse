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

namespace Seahorse {
namespace Ssh {

public class Upload : Gtk.Dialog {

    private unowned List<Key> keys;

    private Gtk.Entry user_entry;
    private Gtk.Entry host_entry;
    private Gtk.Button ok_button;

    public Upload(List<Key> keys, Gtk.Window? parent) {
        GLib.Object(border_width: 5,
                    title: _("Set Up Computer for SSH Connection"),
                    resizable: false,
                    default_width: 400,
                    skip_taskbar_hint: true,
                    skip_pager_hint: true,
                    window_position: Gtk.WindowPosition.CENTER_ON_PARENT,
                    modal: true,
                    transient_for: parent);

        this.keys = keys;

        load_ui();

        // Default to the users current name
        this.user_entry.text = Environment.get_user_name();
        // Focus the host
        this.host_entry.grab_focus();

        on_upload_input_changed();
    }

    // FIXME: normally we would do this using GtkTemplate, but make is abdandoning me :(
    private void load_ui() {
        Gtk.Builder builder = new Gtk.Builder();
        try {
            string path = "/org/gnome/Seahorse/seahorse-ssh-upload.xml";
            builder.add_from_resource(path);
        } catch (GLib.Error err) {
            GLib.critical("%s", err.message);
        }
        Gtk.Container content = (Gtk.Container) builder.get_object("ssh-upload");
        ((Gtk.Container)this.get_content_area()).add(content);

        this.host_entry = (Gtk.Entry) builder.get_object("host-entry");
        this.host_entry.changed.connect(on_upload_input_changed);
        this.user_entry = (Gtk.Entry) builder.get_object("user-entry");
        this.user_entry.changed.connect(on_upload_input_changed);

        this.ok_button = (Gtk.Button) add_button(_("Set up"), Gtk.ResponseType.OK);
    }

    private void upload_keys() {
        string user = this.user_entry.text.strip();
        string host_port = this.host_entry.text.strip();

        if (!user.validate() || host_port == "" || !host_port.validate())
            return;

        // Port is anything past a colon
        string[] host_port_split = host_port.split(":", 2);
        string host = host_port_split[0];
        string? port = (host_port_split.length == 2)? host_port_split[1] : null;

        Cancellable cancellable = new Cancellable();

        // Start the upload process
        UploadOperation op = new UploadOperation();
        op.upload_async.begin(keys, user, host, port, cancellable, (obj, res) => {
            try {
                op.upload_async.end(res);
            } catch (GLib.Error e) {
                Seahorse.Util.show_error(this, _("Couldn’t configure Secure Shell keys on remote computer."), e.message);
            }
        });

        Seahorse.Progress.show(cancellable, _("Configuring Secure Shell Keys…"), false);
    }

    private void on_upload_input_changed () {
        string user = this.user_entry.text;
        string host = this.host_entry.text;

        if (!user.validate() || !host.validate())
            return;

        // Take off port if necessary
        int port_pos = host.index_of_char(':');
        if (port_pos > 0) {
            host = host.substring(0, port_pos);
        }

        this.ok_button.sensitive = (host.strip() != "") && (user.strip() != "");
    }

    /**
     * Prompt a dialog to upload keys.
     *
     * @param keys The set of SSH keys that should be uploaded
     */
    public static void prompt(List<Key> keys, Gtk.Window? parent) {
        if (keys == null)
            return;

        Upload upload_dialog = new Upload(keys, parent);
        for (;;) {
            switch (upload_dialog.run()) {
            case Gtk.ResponseType.HELP:
                /* TODO: Help */
                continue;
            case Gtk.ResponseType.ACCEPT:
                upload_dialog.upload_keys();
                break;
            default:
                break;
            };

            break;
        }

        upload_dialog.destroy();
    }

}

}
}
