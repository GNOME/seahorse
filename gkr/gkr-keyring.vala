/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

namespace Seahorse {
namespace Gkr {

public class Keyring : Secret.Collection, Gcr.Collection, Place, Deletable, Lockable, Viewable {
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

	public GLib.Icon icon {
		owned get { return new GLib.ThemedIcon("folder"); }
	}
	public Gtk.ActionGroup actions {
		owned get {
			if (this._actions == null)
				this._actions = create_actions();
			return this._actions;
		}
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

	private GLib.HashTable<string, Item> _items;
	private Gtk.ActionGroup? _actions;

	construct {
		this._items = new GLib.HashTable<string, Item>(GLib.str_hash, GLib.str_equal);
		this.notify.connect((pspec) => {
			if (pspec.name == "items" || pspec.name == "locked")
				refresh_collection();
		});
		Backend.instance().notify.connect((pspec) => {
			notify_property ("is-default");
			notify_property ("description");
		});
	}

	public uint get_length() {
		return _items.size();
	}

	public GLib.List<weak GLib.Object> get_objects() {
		return _items.get_values();
	}

	public bool contains(GLib.Object obj) {
		if (obj is Item)
			return _items.lookup(((Item)obj).get_object_path()) != null;
		return false;
	}

	public Gtk.Window? create_viewer(Gtk.Window? parent) {
		return new KeyringProperties(this, parent);
	}

	public Deleter create_deleter() {
		return new KeyringDeleter(this);
	}

	public async bool lock(GLib.TlsInteraction? interaction,
			GLib.Cancellable? cancellable) throws GLib.Error {
		var objects = new GLib.List<GLib.DBusProxy>();
		objects.prepend(this);

		var service = get_service();
		GLib.List<GLib.DBusProxy> locked;
		yield service.lock(objects, cancellable, out locked);
		refresh_collection ();
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
		return unlocked.length() > 0;
	}

	public async bool load(GLib.Cancellable? cancellable) throws GLib.Error {
		refresh_collection();
		return true;
	}

	private void refresh_collection() {
		var seen = new GLib.HashTable<string, weak string>(GLib.str_hash, GLib.str_equal);

		GLib.List<Secret.Item> items = null;
		if (!get_locked())
			items = get_items();

		foreach (var item in items) {
			var object_path = item.get_object_path();
			seen.add(object_path);

			if (_items.lookup(object_path) == null) {
				item.set("place", this);
				_items.insert(object_path, (Item)item);
				emit_added(item);
			}
		}

		/* Remove any that we didn't find */
		var iter = GLib.HashTableIter<string, Item>(_items);
		string object_path;
		while (iter.next (out object_path, null)) {
			if (seen.lookup(object_path) == null) {
				var item = _items.lookup(object_path);
				item.set("place", null);
				iter.remove();
				emit_removed (item);
			}
		}
	}

	[CCode (instance_pos = -1)]
	private void on_keyring_default(Gtk.Action action) {
		var parent = Action.get_window(action);
		var service = this.service;

		service.set_alias.begin("default", this, null, (obj, res) => {
			try {
				service.set_alias.end(res);
				Backend.instance().refresh();
			} catch (GLib.Error err) {
				Util.show_error(parent, _("Couldn't set default keyring"), err.message);
			}
		});
	}

	[CCode (instance_pos = -1)]
	private void on_keyring_password (Gtk.Action action) {
		var parent = Action.get_window(action);
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
				service.prompt_at_dbus_path.begin(prompt_path, null, null, (obj, res) => {
					try {
						service.prompt_at_dbus_path.end(res);
					} catch (GLib.Error err) {
						Util.show_error(parent, _("Couldn't change keyring password"), err.message);
					}
					Backend.instance().refresh();
				});
			} catch (GLib.Error err) {
				Util.show_error(parent, _("Couldn't change keyring password"), err.message);
			}
		});
	}

	private static const Gtk.ActionEntry[] KEYRING_ACTIONS = {
		{ "keyring-default", null, N_("_Set as default"), null,
		  N_("Applications usually store new passwords in the default keyring."), on_keyring_default },
		{ "keyring-password", null, N_("Change _Password"), null,
		  N_("Change the unlock password of the password storage keyring"), on_keyring_password },
	};

	private Gtk.ActionGroup create_actions() {
		Gtk.ActionGroup actions = new Gtk.ActionGroup("KeyringActions");

		/* Add these actions, but none of them are visible until cloned */
		actions.set_translation_domain(Config.GETTEXT_PACKAGE);
		actions.add_actions(KEYRING_ACTIONS, this);

		var action = actions.get_action("keyring-default");
		this.bind_property("is-default", action, "sensitive",
		                   GLib.BindingFlags.INVERT_BOOLEAN | GLib.BindingFlags.SYNC_CREATE);

		return actions;
	}

}

class KeyringDeleter : Deleter {
	private Keyring? _keyring;
	private GLib.List<GLib.Object> _objects;

	public override Gtk.Dialog create_confirm(Gtk.Window? parent) {
		var dialog = new DeleteDialog(parent,
		                              _("Are you sure you want to delete the password keyring '%s'?"),
		                              this._keyring.label);

		dialog.check_label = _("I understand that all items will be permanently deleted.");
		dialog.check_require = true;

		return dialog;
	}

	public KeyringDeleter(Keyring keyring) {
		if (!add_object(keyring))
			GLib.assert_not_reached();
	}

	public override unowned GLib.List<weak GLib.Object> get_objects() {
		return this._objects;
	}

	public override bool add_object (GLib.Object obj) {
		if (this._keyring != null)
			return false;
		if (obj is Keyring) {
			this._keyring = (Keyring)obj;
			this._objects.append(obj);
			return true;
		}
		return false;
	}

	public override async bool delete(GLib.Cancellable? cancellable) throws GLib.Error {
		yield this._keyring.delete(cancellable);
		return true;
	}
}

}
}
