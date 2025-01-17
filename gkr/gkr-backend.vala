/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

namespace Seahorse.Gkr {

private class MyService : Secret.Service {
    public override GLib.Type get_collection_gtype() {
        return typeof(Keyring);
    }

    public override GLib.Type get_item_gtype() {
        return typeof(Item);
    }
}

public class Backend: GLib.Object, GLib.ListModel, Seahorse.Backend {

    public string name {
        get { return NAME; }
    }

    public string label {
        get { return _("Passwords"); }
    }

    public string description {
        get { return _("Stored personal passwords, credentials and secrets"); }
    }

    public ActionGroup actions {
        owned get { return this._actions; }
    }

    public GLib.HashTable<string, string> aliases {
        get { return this._aliases; }
    }

    private bool _loaded;
    public bool loaded {
        get { return this._loaded; }
    }

    public Secret.Service? service {
        get { return this._service; }
    }

    private static Backend? _instance = null;
    private Secret.Service _service;
    private GenericArray<Gkr.Keyring> keyrings;
    private GLib.HashTable<string, string> _aliases;
    private ActionGroup _actions;

    construct {
        return_val_if_fail(_instance == null, null);
        Backend._instance = this;

        this._actions = BackendActions.instance(this);
        this.keyrings = new GenericArray<Keyring>();
        this._aliases = new GLib.HashTable<string, string>(GLib.str_hash, GLib.str_equal);

        Secret.Service.open.begin(typeof(MyService), null,
                                  Secret.ServiceFlags.OPEN_SESSION, null, (obj, res) => {
            try {
                this._service = Secret.Service.open.end(res);
                this._service.notify["collections"].connect((obj, pspec) => {
                    refresh_collections();
                });
                this._service.load_collections.begin(null, (obj, res) => {
                    try {
                        this._service.load_collections.end(res);
                        refresh_collections();
                    } catch (GLib.Error e) {
                        warning("couldn't load all secret collections: %s", e.message);
                    }
                });
                refresh_aliases();
            } catch (GLib.Error err) {
                GLib.warning("couldn't connect to secret service: %s", err.message);
            }
            notify_property("service");
        });
    }

    public Seahorse.Place? lookup_place(string uri) {
        for (uint i = 0; i < this.keyrings.length; i++) {
            if (this.keyrings[i].uri == uri)
                return this.keyrings[i];
        }
        return null;
    }

    public void refresh_collections() {
        var seen = new GLib.GenericSet<string>(GLib.str_hash, GLib.str_equal);
        var keyrings = this._service.get_collections();

        foreach (var keyring in keyrings) {
            unowned var object_path = keyring.get_object_path();

            /* Don't list the session keyring */
            if (this._aliases.lookup("session") == object_path)
                continue;

            var uri = "secret-service://%s".printf(object_path);
            seen.add(uri);
            if (lookup_place(uri) == null) {
                this.keyrings.add((Keyring) keyring);
                items_changed(this.keyrings.length - 1, 0, 1);
            }
        }

        /* Remove any that we didn't find */
        for (uint i = 0; i < this.keyrings.length; i++) {
            if (!seen.contains(this.keyrings[i].uri)) {
                this.keyrings.remove_index(i);
                items_changed(i, 1, 0);
                i--;
            }
        }

        if (!_loaded) {
            _loaded = true;
            notify_property("loaded");
        }
    }

    public GLib.Type get_item_type() {
        return typeof(Gkr.Keyring);
    }

    public uint get_n_items() {
        return this.keyrings.length;
    }

    public GLib.Object? get_item(uint position) {
        if (position >= this.keyrings.length)
            return null;
        return this.keyrings[position];
    }

    public static void initialize() {
        return_if_fail(Backend._instance == null);
        (new Backend()).register();
        return_if_fail(Backend._instance != null);
    }

    public static Backend instance() {
        return_val_if_fail(Backend._instance != null, null);
        return Backend._instance;
    }

    private async void read_alias(string name) {
        if (this._service == null)
            return;

        try {
            var object_path = this._service.read_alias_dbus_path_sync(name);
            if (object_path != null) {
                this._aliases[name] = object_path;
                notify_property("aliases");
            }
        } catch (GLib.Error err) {
            warning("Couldn't read secret service alias %s: %s", name, err.message);
        }
    }

    private void refresh_aliases() {
        read_alias.begin ("default");
        read_alias.begin ("session");
        read_alias.begin ("login");
    }

    public void refresh() {
        refresh_aliases();
        refresh_collections();
    }

    public bool has_alias(string alias,
                          Keyring keyring) {
        string object_path = keyring.get_object_path();
        return this._aliases.lookup(alias) == object_path;
    }
}

public class BackendActions : Seahorse.ActionGroup {
    public Backend backend { construct; get; }
    private static WeakRef _instance;
    private bool _initialized;

    private const ActionEntry[] BACKEND_ACTIONS = {
        { "keyring-new", action_new_keyring },
        { "keyring-item-new", action_new_item },
        { "copy-secret", action_copy_secret },
    };

    construct {
        this._initialized = false;

        this.backend.notify.connect_after((pspec) => {
            if (pspec.name == "service")
                return;
            if (this._initialized)
                return;
            if (this.backend.service == null)
                    return;

            this._initialized = true;
            add_action_entries(BACKEND_ACTIONS, this);
        });

        this.backend.notify_property("service");
    }

    private BackendActions(Backend backend) {
        GLib.Object(
            prefix: "gkr",
            backend: backend
        );
    }

    private void action_new_keyring(SimpleAction action, Variant? param) {
        var dialog = new AddKeyringDialog();
        dialog.keyring_added.connect((keyring) => {
            this.catalog.activate_action("focus-place", "secret-service");
        });
        dialog.present(this.catalog);
    }

    private void action_new_item(SimpleAction action, Variant? param) {
        var dialog = new AddItemDialog();
        dialog.item_added.connect((item) => {
            this.catalog.activate_action("focus-place", "secret-service");
        });
        dialog.present(this.catalog);
    }

    private void action_copy_secret(SimpleAction action, Variant? param)
            requires(this.catalog != null) {

        var selected = this.catalog.get_selected_items();
        // We checked for this in set_actions_for_selected_objects()
        return_if_fail (selected.length() == 1);

        var selected_item = selected.data as Gkr.Item;
        return_if_fail (selected_item != null);

        var clipboard = this.catalog.get_clipboard();
        selected_item.copy_secret_to_clipboard.begin(clipboard, (obj, res) => {
            try {
                selected_item.copy_secret_to_clipboard.end(res);
            } catch (GLib.Error e) {
                Util.show_error(this.catalog, "Couldn't copy secret", e.message);
            }
        });
    }

    public override void set_actions_for_selected_objects(List<GLib.Object> objects) {
        // Allow "copy secret" if there is only 1 Gkr.Item selected
        bool can_copy_secret = false;

        if (objects.length() == 1) {
            can_copy_secret = objects.data is Gkr.Item;
        }

        ((SimpleAction) lookup_action("copy-secret")).set_enabled(can_copy_secret);
    }

    public static ActionGroup instance(Backend backend) {
        BackendActions? actions = (BackendActions?)_instance.get();
        if (actions != null)
            return actions;
        actions = new BackendActions(backend);
        _instance.set(actions);
        return actions;
    }
}

}
