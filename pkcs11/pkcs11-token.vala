/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2013 Red Hat, Inc.
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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

namespace Seahorse {
namespace Pkcs11 {

public class Token : GLib.Object, Gcr.Collection, Place, Lockable {

	public bool unlockable {
		get {
			this.ensure_token_info();
			if ((this._info.flags & CKF.LOGIN_REQUIRED) == 0)
				return false;
			if ((this._info.flags & CKF.USER_PIN_INITIALIZED) == 0)
				return false;
			return !is_session_logged_in(this._session);
		}
	}

	public bool lockable {
		get {
			this.ensure_token_info();
			if ((this._info.flags & CKF.LOGIN_REQUIRED) == 0)
				return false;
			if ((this._info.flags & CKF.USER_PIN_INITIALIZED) == 0)
				return false;
			return is_session_logged_in(this._session);
		}
	}

	public Gck.TokenInfo info {
		get { return this.ensure_token_info(); }
	}

	public Gck.Session session {
		get { return this._session; }
		set {
			this._session = session;
			notify_property("session");
			notify_property("lockable");
			notify_property("unlockable");
		}
	}

	public Gck.Slot slot {
		get { return this._slot; }
		construct { this._slot = value; }
	}

	public string label {
		owned get {
			var token = this._slot.get_token_info();
			if (token == null)
				return C_("Label", "Unknown");
			return token.label;
		}
	}

	public string description {
		owned get {
			var token = this._slot.get_token_info();
			if (token == null)
				return "";
			return token.manufacturer_id;
		}
	}

	public string uri {
		owned get { return this._uri; }
	}

	public GLib.Icon icon {
		owned get {
			var token = this._slot.get_token_info();
			if (token == null)
				return new GLib.ThemedIcon(Gtk.Stock.DIALOG_QUESTION);
			return Gcr.icon_for_token(token);
		}
	}

	public Gtk.ActionGroup? actions {
		owned get { return null; }
	}

	public Flags object_flags {
		get { return 0; }
	}

	public unowned GLib.Array<ulong> mechanisms {
		get {
			if (this._mechanisms == null)
				this._mechanisms = this._slot.get_mechanisms();
			return this._mechanisms;
		}
	}

	private Gck.Slot _slot;
	private string _uri;
	private Gck.TokenInfo? _info;
	private GLib.Array<ulong> _mechanisms;
	private Gck.Session? _session;
	private GLib.HashTable<ulong?, GLib.Object> _object_for_handle;
	private GLib.HashTable<Gck.Attribute, GLib.GenericArray<GLib.Object>> _objects_for_id;
	private GLib.HashTable<GLib.Object, Gck.Attribute> _id_for_object;
	private GLib.HashTable<GLib.Object, GLib.Object> _objects_visible;

	public Token(Gck.Slot slot) {
		GLib.Object(
			slot: slot
		);
	}

	construct {
		this._object_for_handle = new GLib.HashTable<ulong?, GLib.Object>(ulong_hash, ulong_equal);
		this._objects_for_id = new GLib.HashTable<Gck.Attribute, GLib.GenericArray<GLib.Object>>(Gck.Attribute.hash, Gck.Attribute.equal);
		this._id_for_object = new GLib.HashTable<GLib.Object, unowned Gck.Attribute>(GLib.direct_hash, GLib.direct_equal);
		this._objects_visible = new GLib.HashTable<GLib.Object, GLib.Object>(GLib.direct_hash, GLib.direct_equal);

		/* TODO: Does this happen in the background? It really should. */
		this.load.begin(null);

		var data = new Gck.UriData();
		this.ensure_token_info();
		data.token_info = this._info;
		this._uri = Gck.uri_build(data, Gck.UriFlags.FOR_TOKEN);
	}

	public override void dispose() {
		this._slot = null;
		this._session = null;
	}

	public async bool lock(GLib.TlsInteraction? interaction,
	                       GLib.Cancellable? cancellable) throws GLib.Error {
		if (!is_session_logged_in(this._session))
			return true;

		yield this._session.logout_async(cancellable);
		return yield this.load(cancellable);
	}

	public async bool unlock(GLib.TlsInteraction? interaction,
	                         GLib.Cancellable? cancellable) throws GLib.Error {
		if (is_session_logged_in (this._session))
			return true;
		if (this._session != null) {
			return yield this._session.login_interactive_async(CKU.USER, interaction, cancellable);
		} else {
			var options = calculate_session_options();
			this._session = yield this._slot.open_session_async(options | Gck.SessionOptions.LOGIN_USER,
			                                                    cancellable);
			return true;
		}
	}

	public bool contains (GLib.Object object) {
		return this._objects_visible.lookup(object) != null;
	}

	public uint get_length() {
		return this._objects_visible.size();
	}

	public GLib.List<weak GLib.Object> get_objects() {
		return this._objects_visible.get_values();
	}

	public bool is_deletable(Gck.Object object) {
		this.ensure_token_info();

		if ((this._info.flags & CKF.WRITE_PROTECTED) == CKF.WRITE_PROTECTED)
			return false;

		Gck.Attributes? attributes;
		object.get("attributes", out attributes);

		if (attributes != null) {
			bool ret = true;
			attributes.find_boolean(CKA.MODIFIABLE, out ret);
			return ret;
		}

		return false;
	}

	public void remove_object(Gck.Object object) {
		GLib.List<Gck.Object> objects = null;
		objects.append(object);
		remove_objects(objects);
	}

	public bool has_mechanism(ulong mechanism) {
		return Gck.mechanisms_check(this.mechanisms, mechanism, Gck.INVALID);
	}

	private static bool is_session_logged_in(Gck.Session? session) {
		if (session == null)
			return false;
		var info = session.get_info();
		return (info != null) &&
		       (info.state == CKS.RW_USER_FUNCTIONS ||
		        info.state == CKS.RO_USER_FUNCTIONS ||
		        info.state == CKS.RW_SO_FUNCTIONS);
	}

	private unowned Gck.TokenInfo ensure_token_info() {
		if (this._info == null)
			this.update_token_info();
		return this._info;
	}

	private void update_token_info() {
		var info = this._slot.get_token_info();
		if (info != null) {
			this._info = info;
			this.notify_property("info");
			this.notify_property("lockable");
			this.notify_property("unlockable");
		}
	}

	private void update_id_map(GLib.Object object,
	                           Gck.Attribute* id) {
		bool add = false;
		bool remove = false;

		var pid = this._id_for_object.lookup(object);
		if (id == null) {
			if (pid != null) {
				id = pid;
				remove = true;
			}
		} else {
			if (pid == null) {
				add = true;
			} else if (!id->equal(pid)) {
				remove = true;
				add = true;
			}
		}

		if (add) {
			unowned GLib.GenericArray<GLib.Object>? objects;
			objects = this._objects_for_id.lookup(id);
			if (objects == null) {
				var objs = new GLib.GenericArray<GLib.Object>();
				this._objects_for_id.insert(id, objs);
				objects = objs;
			}
			objects.add(object);
			this._id_for_object.insert(object, id);
		}

		/* Remove this object from the map */
		if (remove) {
			if (!this._id_for_object.remove(object))
				GLib.assert_not_reached();
			var objects = this._objects_for_id.lookup(id);
			GLib.assert(objects != null);
			GLib.assert(objects.length > 0);
			if (objects.length == 1) {
				if (!this._objects_for_id.remove(id))
					GLib.assert_not_reached();
			} else {
				if (!objects.remove(object))
					GLib.assert_not_reached();
			}
		}
	}

	private GLib.Object? lookup_id_map(GLib.Type object_type,
	                                   Gck.Attribute* id) {
		if (id == null)
			return null;
		var objects = this._objects_for_id.lookup(id);
		if (objects == null)
			return null;
		for (var i = 0; i < objects.length; i++) {
			if (objects[i].get_type().is_a(object_type))
				return objects[i];
		}
		return null;
	}

	private void update_visibility(GLib.List<GLib.Object> objects,
	                               bool visible) {
		foreach (var object in objects) {
			bool have = (this._objects_visible.lookup(object) != null);
			if (!have && visible) {
				this._objects_visible.insert(object, object);
				this.emit_added(object);
			} else if (have && !visible) {
				if (!this._objects_visible.remove(object))
					GLib.assert_not_reached();
				this.emit_removed(object);
			}
		}

	}

	private static bool make_certificate_key_pair(Certificate certificate,
	                                              PrivateKey private_key) {
		if (certificate.partner != null || private_key.partner != null)
			return false;
		certificate.partner = private_key;
		private_key.partner = certificate;
		return true;
	}

	private static GLib.Object? break_certificate_key_pair(GLib.Object object) {
		GLib.Object? pair = null;
		if (object is Certificate) {
			var certificate = (Certificate)object;
			pair = certificate.partner;
			certificate.partner = null;
		} else if (object is PrivateKey) {
			var private_key = (PrivateKey)object;
			pair = private_key.partner;
			private_key.partner = null;
		}
		return pair;
	}

	private void receive_objects(GLib.List<GLib.Object> objects) {
		var show = new GLib.List<GLib.Object>();
		var hide = new GLib.List<GLib.Object>();

		foreach (var object in objects) {
			if (!(object is Gck.Object && object is Gck.ObjectCache))
				continue;
			var handle = ((Gck.Object)object).handle;
			var attrs = ((Gck.ObjectCache)object).attributes;

			var prev = this._object_for_handle.lookup(handle);
			if (prev == null) {
				this._object_for_handle.insert(handle, object);
				object.set("place", this);
			} else if (prev != object) {
				object.set("attributes", attrs);
				object = prev;
			}

			unowned Gck.Attribute? id = null;
			if (attrs != null)
				id = attrs.find(CKA.ID);
			this.update_id_map(object, id);

			if (object is Certificate) {
				var pair = this.lookup_id_map(typeof(PrivateKey), id);
				if (pair != null && make_certificate_key_pair((Certificate)object, (PrivateKey)pair))
					hide.prepend(pair);
				show.prepend(object);
			} else if (object is PrivateKey) {
				var pair = this.lookup_id_map(typeof(Certificate), id);
				if (pair != null && make_certificate_key_pair((Certificate)pair, (PrivateKey)object))
					hide.prepend(object);
				else
					show.prepend(object);
			} else {
				show.prepend(object);
			}
		}

		update_visibility(hide, false);
		update_visibility(show, true);
	}

	private void remove_objects(GLib.List<GLib.Object> objects) {
		var depaired = new GLib.List<GLib.Object>();
		var hide = new GLib.List<GLib.Object>();

		foreach (var object in objects) {
			var pair = break_certificate_key_pair(object);
			if (pair != null)
				depaired.prepend(pair);
			update_id_map(object, null);
			hide.prepend(object);
		}

		/* Remove the ownership of these */
		foreach (var object in objects) {
			var handle = ((Gck.Object)object).handle;
			object.set("place", null);
			this._object_for_handle.remove(handle);
		}

		update_visibility(hide, false);

		/* Add everything that was paired */
		receive_objects(depaired);
	}

	private Gck.SessionOptions calculate_session_options() {
		this.ensure_token_info();
		if ((this._info.flags & CKF.WRITE_PROTECTED) == CKF.WRITE_PROTECTED)
			return Gck.SessionOptions.READ_ONLY;
		else
			return Gck.SessionOptions.READ_WRITE;
	}

	public async bool load(GLib.Cancellable? cancellable) throws GLib.Error {
		var checks = new GLib.HashTable<ulong?, GLib.Object>(ulong_hash, ulong_equal);

		/* Make note of all the objects that were there */
		this.update_token_info();
		foreach (var object in this.get_objects()) {
			var handle = ((Gck.Object)object).handle;
			checks.insert(handle, object);
		}

		if (this._session == null) {
			var options = this.calculate_session_options();
			this._session = yield this._slot.open_session_async(options, cancellable);
		}

		var builder = new Gck.Builder(Gck.BuilderFlags.NONE);
		builder.add_boolean(CKA.TOKEN, true);
		builder.add_ulong(CKA.CLASS, CKO.CERTIFICATE);

		const ulong[] CERTIFICATE_ATTRS = {
			CKA.VALUE,
			CKA.ID,
			CKA.LABEL,
			CKA.CLASS,
			CKA.CERTIFICATE_CATEGORY,
			CKA.MODIFIABLE
		};

		var enumerator = this._session.enumerate_objects(builder.end());
		enumerator.set_object_type(typeof(Certificate), CERTIFICATE_ATTRS);

		builder = new Gck.Builder(Gck.BuilderFlags.NONE);
		builder.add_boolean(CKA.TOKEN, true);
		builder.add_ulong(CKA.CLASS, CKO.PRIVATE_KEY);

		const ulong[] KEY_ATTRS = {
			CKA.MODULUS_BITS,
			CKA.ID,
			CKA.LABEL,
			CKA.CLASS,
			CKA.KEY_TYPE,
			CKA.MODIFIABLE,
		};

		var chained = this._session.enumerate_objects(builder.end());
		chained.set_object_type(typeof(PrivateKey), KEY_ATTRS);
		enumerator.set_chained(chained);

		for (;;) {
			var objects = yield enumerator.next_async(16, cancellable);

			/* Otherwise we're done, remove everything not found */
			if (objects == null) {
				remove_objects(checks.get_values());
				return true;
			}

			this.receive_objects(objects);

			/* Remove all objects that were found from the check table */
			foreach (var object in objects) {
				var handle = ((Gck.Object)object).handle;
				checks.remove(handle);
			}
		}
	}
}

}
}
