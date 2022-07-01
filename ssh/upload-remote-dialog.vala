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

[GtkTemplate (ui = "/org/gnome/Seahorse/ssh-upload-remote-dialog.ui")]
public class Seahorse.Ssh.UploadRemoteDialog : Adw.Dialog {

    private unowned List<Key> keys;

    [GtkChild] private unowned Adw.NavigationView navigation_view;
    [GtkChild] private unowned Adw.NavigationPage configuring_page;

    [GtkChild] private unowned Adw.EntryRow host_row;
    [GtkChild] private unowned Adw.EntryRow user_row;

    private Cancellable? cancellable = null;

    /** Emitted when the key(s) have been successfully uploaded */
    public signal void key_uploaded();

    /** Emitted when there was an error on upload */
    public signal void error(string message);

    static construct {
        install_action("submit", null, (Gtk.WidgetActionActivateFunc) action_submit);
    }

    construct {
        // Default to the users current name
        this.user_row.text = Environment.get_user_name();
        check_submit();
    }

    public UploadRemoteDialog(Key key) {
        this.keys.append(key);
    }

    public UploadRemoteDialog.for_multiple(List<Key> keys) {
        this.keys = keys;
    }

    private void action_submit(string action_name, Variant? param) {
        string user = this.user_row.text.strip();
        string host_port = this.host_row.text.strip();

        if (!user.validate() || host_port == "" || !host_port.validate())
            return;

        // Port is anything past a colon
        string[] host_port_split = host_port.split(":", 2);
        string host = host_port_split[0];
        string? port = (host_port_split.length == 2)? host_port_split[1] : null;

        this.cancellable = new Cancellable();

        // Start the upload process
        UploadOperation op = new UploadOperation();
        op.upload_async.begin(keys, user, host, port, this.cancellable, (obj, res) => {
            close();
            try {
                op.upload_async.end(res);
                key_uploaded();
            } catch (GLib.Error e) {
                error(e.message);
            }
        });

        this.navigation_view.push(this.configuring_page);
    }

    [GtkCallback]
    private void on_upload_input_changed(Gtk.Editable editable) {
        check_submit();
    }

    private void check_submit() {
        string user = this.user_row.text;
        string host = this.host_row.text;

        if (!user.validate() || !host.validate())
            return;

        // Take off port if necessary
        int port_pos = host.index_of_char(':');
        if (port_pos > 0) {
            host = host.substring(0, port_pos);
        }

        action_set_enabled("submit", (host.strip() != "") && (user.strip() != ""));
    }

    [GtkCallback]
    private void on_navigation_view_popped(Adw.NavigationView nav_view,
                                           Adw.NavigationPage page) {
        if (this.cancellable == null)
            return;

        this.cancellable.cancel();
        this.cancellable = null;
    }
}
