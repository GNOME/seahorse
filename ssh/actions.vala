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

namespace Seahorse {
namespace Ssh {

public class Actions : Seahorse.Actions {

    private const Gtk.ActionEntry KEYS_ACTIONS[] = {
        { "remote-ssh-upload", null, N_("Configure Key for _Secure Shell…"), null,
            N_("Send public Secure Shell key to another machine, and enable logins using that key."),
            on_ssh_upload }
    };

    public const string UI_DEFINITION = """
    <ui>
      <menubar>
        <placeholder name="RemoteMenu">
          <menu name="Remote" action="remote-menu">
            <menuitem action="remote-ssh-upload"/>
          </menu>
        </placeholder>
      </menubar>
      <popup name="ObjectPopup">
        <menuitem action="remote-ssh-upload"/>
      </popup>
    </ui>""";

    construct {
        set_translation_domain(Config.GETTEXT_PACKAGE);
        add_actions(KEYS_ACTIONS, this);
        register_definition(UI_DEFINITION);
    }

    private Actions(string name) {
        base(name);
    }

    private static Actions _instance = null;

    public static unowned Actions instance() {
        if (_instance == null) {
            _instance = new Actions("SshKey");
        }

        return _instance;
    }

    private void on_ssh_upload (Gtk.Action action) {
        List<Key> keys = new List<Key>();

        if (this.catalog != null) {
            foreach (GLib.Object el in this.catalog.get_selected_objects()) {
                Key key = el as Key;
                if (key != null)
                    keys.prepend(key);
            }
        }

        Upload.prompt(keys, Seahorse.Action.get_window(action));
    }
}

}
}
