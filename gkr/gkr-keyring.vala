/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

public class Seahorse.Gkr.Keyring : Secret.Collection, GLib.ListModel, Place,
                                    Deletable, Lockable, Viewable {

    private const ActionEntry[] KEYRING_ACTIONS = {
        { "set-default",     on_action_set_default },
        { "change-password", on_action_change_password },
    };

    public string description {
        owned get {
            if (Backend.instance().has_alias ("login", this))
                return _("A keyring that is automatically unlocked on login");
            return _("A keyring used to store passwords");
        }
    }

    public string uri {
        owned get {
            var object_path = base.get_object_path ();
            return "secret-service://%s".printf(object_path);
        }
    }

    public Place.Category category {
        get { return Place.Category.PASSWORDS; }
    }

    private GLib.ActionGroup? _actions = null;
    public GLib.ActionGroup? actions {
        owned get { return this._actions; }
    }

    public unowned string? action_prefix {
        get { return "gkr-keyring"; }
    }

    private MenuModel? _menu_model = null;
    public MenuModel? menu_model {
        owned get { return this._menu_model; }
    }

    public bool is_default {
        get { return Backend.instance().has_alias ("default", this); }
    }

    public bool lockable {
        get { return !get_locked(); }
    }

    public bool unlockable {
        get { return get_locked(); }
    }

    public bool deletable {
        get { return true; }
    }

    private GenericArray<Gkr.Item> _items = new GenericArray<Gkr.Item>();

    construct {
        this.notify.connect((pspec) => {
            if (pspec.name == "items" || pspec.name == "locked")
                refresh_collection();
        });
        Backend.instance().notify.connect((pspec) => {
            notify_property ("is-default");
            notify_property ("description");
        });

        this._actions = create_actions();
        this._menu_model = create_menu_model();
    }

    public GLib.Type get_item_type() {
        return typeof(Gkr.Item);
    }

    public uint get_n_items() {
        return this._items.length;
    }

    public GLib.Object? get_item(uint index) {
        if (index >= this._items.length)
            return null;
        return this._items[index];
    }

    private Gkr.Item? lookup_by_path(string object_path) {
        for (uint i = 0; i < this._items.length; i++) {
            if (object_path == this._items[i].get_object_path())
                return this._items[i];
        }
        return null;
    }

    public Seahorse.Panel create_panel() {
        return new KeyringPanel(this);
    }

    public DeleteOperation create_delete_operation() {
        return new KeyringDeleteOperation(this);
    }

    public async bool lock(GLib.TlsInteraction? interaction,
            GLib.Cancellable? cancellable) throws GLib.Error {
        var objects = new GLib.List<GLib.DBusProxy>();
        objects.prepend(this);

        var service = get_service();
        GLib.List<GLib.DBusProxy> locked;
        yield service.lock(objects, cancellable, out locked);
        refresh_collection ();
        notify_property("lockable");
        notify_property("unlockable");
        return locked.length() > 0;
    }

    public async bool unlock(GLib.TlsInteraction? interaction,
                             GLib.Cancellable? cancellable) throws GLib.Error {
        var objects = new GLib.List<GLib.DBusProxy>();
        objects.prepend(this);

        var service = get_service();
        GLib.List<GLib.DBusProxy> unlocked;
        yield service.unlock(objects, cancellable, out unlocked);
        refresh_collection ();
        notify_property("lockable");
        notify_property("unlockable");
        return unlocked.length() > 0;
    }

    public async bool load(GLib.Cancellable? cancellable) throws GLib.Error {
        refresh_collection();
        return true;
    }

    private void refresh_collection() {
        var seen = new GLib.GenericSet<string>(GLib.str_hash, GLib.str_equal);

        GLib.List<Secret.Item> items = null;
        if (!get_locked())
            items = get_items();

        // NOTE: this is not _items
        foreach (var item in items) {
            unowned var object_path = item.get_object_path();
            seen.add(object_path);

            if (lookup_by_path(object_path) == null) {
                item.set("place", this);
                this._items.add((Item) item);
                items_changed(this._items.length - 1, 0, 1);
            }
        }

        /* Remove any that we didn't find */
        for (uint i = 0; i < this._items.length; i++) {
            unowned var object_path = this._items[i].get_object_path();
            if (!seen.contains(object_path)) {
                var item = lookup_by_path(object_path);
                item.place = null;
                this._items.remove(item);
                items_changed(i, 1, 0);
                i--;
            }
        }
    }

    public void on_action_set_default(SimpleAction action, Variant? param) {
        set_as_default();
    }

    public void set_as_default() {
        var parent = null;
        var service = this.service;

        service.set_alias.begin("default", this, null, (obj, res) => {
            try {
                service.set_alias.end(res);
                Backend.instance().refresh();
            } catch (GLib.Error err) {
                Util.show_error(parent, _("Couldn’t set default keyring"), err.message);
            }
        });
    }

    public void on_action_change_password(SimpleAction action, Variant? param) {
        change_password();
    }

    public void change_password() {
        var parent = null;
        var service = this.service;
        service.get_connection().call.begin(service.get_name(),
                                            service.get_object_path(),
                                            "org.gnome.keyring.InternalUnsupportedGuiltRiddenInterface",
                                            "ChangeWithPrompt",
                                            new GLib.Variant("(o)", this.get_object_path()),
                                            new GLib.VariantType("(o)"),
                                            GLib.DBusCallFlags.NONE, -1, null, (obj, res) => {
            try {
                var retval = service.get_connection().call.end(res);
                string prompt_path;
                retval.get("(o)", out prompt_path);
                service.prompt_at_dbus_path.begin(prompt_path.dup(), null, null, (obj, res) => {
                    try {
                        service.prompt_at_dbus_path.end(res);
                    } catch (GLib.Error err) {
                        Util.show_error(parent, _("Couldn’t change keyring password"), err.message);
                    }
                    Backend.instance().refresh();
                });
            } catch (GLib.Error err) {
                Util.show_error(parent, _("Couldn’t change keyring password"), err.message);
            }
        });
    }

    private SimpleActionGroup create_actions() {
        var group = new SimpleActionGroup();
        group.add_action_entries (KEYRING_ACTIONS, this);

        var action = group.lookup_action("set-default");
        bind_property("is-default", action, "enabled", BindingFlags.INVERT_BOOLEAN);

        return group;
    }

    private GLib.Menu create_menu_model() {
        var menu = new GLib.Menu();
        unowned string prefix = this.action_prefix;

        menu.insert(0, _("_Set as default"),  prefix + ".set-default");
        menu.insert(1, _("Change _Password"), prefix + ".change-password");
        return menu;
    }
}
