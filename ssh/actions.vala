/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2016 Niels De Graef
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

public class Seahorse.Ssh.Actions : ActionGroup {

    private const ActionEntry KEYS_ACTIONS[] = {
        { "generate-key",    on_ssh_generate_key },
        { "remote-upload",   on_ssh_upload       },
    };

    construct {
        add_action_entries(KEYS_ACTIONS, this);
    }

    private Actions() {
        GLib.Object(prefix: "ssh");
    }

    private static Actions _instance = null;
    public static unowned Actions instance() {
        if (_instance == null)
            _instance = new Actions();

        return _instance;
    }

    public override void set_actions_for_selected_objects(List<GLib.Object> objects) {
        bool is_ssh_key = false;

        foreach (var object in objects) {
            if (object is Ssh.Key) {
                is_ssh_key = true;
                break;
            }
        }

        ((SimpleAction) lookup_action("remote-upload")).set_enabled(is_ssh_key);
    }

    private void on_ssh_generate_key(SimpleAction action, Variant? param) {
        var dialog = new GenerateKeyDialog(Backend.instance.get_dot_ssh());

        dialog.key_created.connect((key) => {
            this.catalog.activate_action("focus-place", "openssh");
        });
        dialog.present(this.catalog);
    }

    private void on_ssh_upload(SimpleAction action, Variant? param) {
        List<Key> keys = new List<Key>();

        if (this.catalog != null) {
            foreach (Seahorse.Item item in this.catalog.get_selected_items()) {
                unowned var key = item as Key;
                if (key != null)
                    keys.prepend(key);
            }
        }

        var dialog = new UploadRemoteDialog.for_multiple(keys);
        dialog.present(this.catalog);
    }
}
