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

namespace Seahorse {
namespace Gkr {

private class MyService : Secret.Service {
	public override GLib.Type get_collection_gtype() {
		return typeof(Keyring);
	}

	public override GLib.Type get_item_gtype() {
		return typeof(Item);
	}
}

public class Backend: GLib.Object , Gcr.Collection, Seahorse.Backend {
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
	private GLib.HashTable<string, Keyring> _keyrings;
	private GLib.HashTable<string, string> _aliases;
	private ActionGroup _actions;

	construct {
		return_val_if_fail(_instance == null, null);
		Backend._instance = this;

		this._actions = BackendActions.instance(this);
		this._keyrings = new GLib.HashTable<string, Keyring>(GLib.str_hash, GLib.str_equal);
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

	public override void dispose() {
		this._aliases.remove_all();
		this._keyrings.remove_all();
		base.dispose();
	}

	public void refresh_collections() {
		var seen = new GLib.GenericSet<string>(GLib.str_hash, GLib.str_equal);
		var keyrings = this._service.get_collections();

		string object_path;
		foreach (var keyring in keyrings) {
			object_path = keyring.get_object_path();

			/* Don't list the session keyring */
			if (this._aliases.lookup("session") == object_path)
				continue;

			var uri = "secret-service://%s".printf(object_path);
			seen.add(uri);
			if (this._keyrings.lookup(uri) == null) {
				this._keyrings.insert(uri, (Keyring)keyring);
				emit_added(keyring);
			}
		}

		/* Remove any that we didn't find */
		var iter = GLib.HashTableIter<string, Keyring>(this._keyrings);
		string uri;
		while (iter.next(out uri, null)) {
			if (!seen.contains(uri)) {
				var keyring = this._keyrings.lookup(uri);
				iter.remove();
				emit_removed(keyring);
			}
		}

		if (!_loaded) {
			_loaded = true;
			notify_property("loaded");
		}
	}

	public uint get_length() {
		return this._keyrings.size();
	}

	public GLib.List<weak GLib.Object> get_objects() {
		return get_keyrings();
	}

    public bool contains(GLib.Object object) {
        var keyring = object as Gkr.Keyring;
        if (keyring == null)
            return false;

        return this._keyrings.lookup(keyring.uri) == keyring;
    }

	public Place? lookup_place(string uri) {
		return this._keyrings.lookup(uri);
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

	public GLib.List<unowned Keyring> get_keyrings() {
		return this._keyrings.get_values();
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
        { "keyring-new",      on_new_keyring },
        { "keyring-item-new", on_new_item    },
        { "copy-secret",      on_copy_secret },
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

    private void on_new_keyring(SimpleAction action, Variant? param) {
        var dialog = new KeyringAdd(this.catalog);

        int response = dialog.run();
        if (response == Gtk.ResponseType.ACCEPT)
            this.catalog.activate_action("focus-place", "secret-service");
        dialog.destroy();
    }

    private void on_new_item(SimpleAction action, Variant? param) {
        var dialog = new ItemAdd(this.catalog);

        int response = dialog.run();
        if (response == Gtk.ResponseType.ACCEPT)
            this.catalog.activate_action("focus-place", "secret-service");
        dialog.destroy();
    }

    private void on_copy_secret(SimpleAction action, Variant? param) {
        return_if_fail (this.catalog != null);

        var selected = this.catalog.get_selected_objects();
        // We checked for this in set_actions_for_selected_objects()
        return_if_fail (selected.length() == 1);

        var selected_item = selected.data as Gkr.Item;
        return_if_fail (selected_item != null);

        var clipboard = Gtk.Clipboard.get_default(this.catalog.get_display());
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
}
