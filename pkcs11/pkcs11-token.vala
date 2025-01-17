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

public class Seahorse.Pkcs11.Token : GLib.Object, GLib.ListModel, Place, Lockable, Viewable {

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

    private Gck.TokenInfo? _info;
    public Gck.TokenInfo info {
        get { return this.ensure_token_info(); }
    }

    private Gck.Session? _session;
    public Gck.Session session {
        get { return this._session; }
        set {
            this._session = session;
            notify_property("session");
            notify_property("lockable");
            notify_property("unlockable");
        }
    }

    private Gck.Slot _slot;
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
        set {
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

    private string _uri;
    public string uri {
        owned get { return this._uri; }
    }

    public Place.Category category {
        get { return Place.Category.CERTIFICATES; }
    }

    public GLib.ActionGroup? actions {
        owned get { return null; }
    }

    public unowned string? action_prefix {
        get { return null; }
    }

    public MenuModel? menu_model {
        owned get { return null; }
    }

    private GLib.Array<ulong> _mechanisms;
    public unowned GLib.Array<ulong> mechanisms {
        get {
            if (this._mechanisms == null)
                this._mechanisms = this._slot.get_mechanisms();
            return this._mechanisms;
        }
    }

    private GLib.HashTable<ulong?, GLib.Object> _object_for_handle
        = new GLib.HashTable<ulong?, GLib.Object>(ulong_hash, ulong_equal);

    private GenericArray<CertKeyPair> pairs = new GenericArray<CertKeyPair>();

    public Token(Gck.Slot slot) {
        GLib.Object(
            slot: slot
        );
    }

    construct {
        /* TODO: Does this happen in the background? It really should. */
        this.load.begin(null);

        var data = new Gck.UriData();
        this.ensure_token_info();
        data.token_info = this._info;
        this._uri = data.build(Gck.UriFlags.FOR_TOKEN);
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
        if (is_session_logged_in(this._session))
            return true;
        if (this._session != null) {
            yield this._session.login_interactive_async(CKU.USER, interaction, cancellable);
            return yield load(cancellable);
        } else {
            var options = calculate_session_options();
            this._session = yield this._slot.open_session_async(options | Gck.SessionOptions.LOGIN_USER,
                                                                interaction,
                                                                cancellable);
            return yield load(cancellable);
        }
    }

    public GLib.Type get_item_type() {
        return typeof(CertKeyPair);
    }

    public uint get_n_items() {
        return this.pairs.length;
    }

    public GLib.Object? get_item(uint i) {
        if (i >= this.pairs.length)
            return null;
        return this.pairs[i];
    }

    public bool is_write_protected() {
        this.ensure_token_info();

        return ((this._info.flags & CKF.WRITE_PROTECTED) == CKF.WRITE_PROTECTED);
    }

    public void remove_object(Gck.Object object) {
        GLib.List<Gck.Object> objects = null;
        objects.append(object);
        remove_objects(objects.copy());
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

    private unowned CertKeyPair? find(GLib.Object object,
                                      out uint position) {
        for (uint i = 0; i < this.pairs.length; i++) {
            unowned var pair = this.pairs[i];

            if (pair.certificate == object || pair.private_key == object) {
                position = i;
                return pair;
            }
        }

        return null;
    }

    private unowned CertKeyPair? find_by_id(Gck.Attribute* id,
                                            out uint position) {
        for (uint i = 0; i < this.pairs.length; i++) {
            unowned var pair = this.pairs[i];

            if (pair.id == id) {
                position = i;
                return pair;
            }
        }

        return null;
    }

    private void receive_objects(GLib.List<GLib.Object> objects) {
        foreach (unowned var object in objects) {
            if (!(object is Gck.Object && object is Gck.ObjectCache))
                continue;

            var handle = ((Gck.Object)object).handle;
            var attrs = ((Gck.ObjectCache)object).attributes;

            var prev = this._object_for_handle.lookup(handle);
            if (prev == null) {
                this._object_for_handle.insert(handle, object);
            } else if (prev != object) {
                object.set("attributes", attrs);
                object = prev;
            }

            unowned Gck.Attribute? id = null;
            if (attrs != null)
                id = attrs.find(CKA.ID);

            // Set ID for pair
            uint position;
            var pair = find_by_id(id, out position);
            if (pair != null) {
                if (object is Certificate) {
                    pair.certificate = (Certificate) object;
                } else if (object is PrivateKey) {
                    pair.private_key = (PrivateKey) object;
                } else {
                    critical("Unsupported item of type %s", object.get_type().name());
                }
                // Trigger an "items-changed" signal so that the UI may react
                items_changed(position, 1, 1);
            } else {
                if (object is Certificate) {
                    pair = new CertKeyPair.for_cert(this, (Certificate) object);
                } else if (object is PrivateKey) {
                    pair = new CertKeyPair.for_private_key(this, (PrivateKey) object);
                } else {
                    critical("Unsupported item of type %s", object.get_type().name());
                }
                pair.id = id;
                this.pairs.add(pair);
                items_changed(this.pairs.length - 1, 0, 1);
            }
        }
    }

    private void remove_objects(GLib.List<weak GLib.Object> objects) {
        foreach (unowned var object in objects) {
            uint position;
            var pair = find(object, out position);
            if (pair == null) {
                warning("Trying to remove object from non-existent pair");
                continue;
            }

            if (pair.is_pair) {
                if (object is Certificate) {
                    pair.certificate = null;
                } else {
                    pair.private_key = null;
                }
                items_changed(position, 1, 1);
            } else {
                this.pairs.remove_index(position);
                items_changed(position, 1, 0);
            }
        }

        /* Remove the ownership of these */
        foreach (unowned var object in objects) {
            var handle = ((Gck.Object)object).handle;
            this._object_for_handle.remove(handle);
        }
    }

    private Gck.SessionOptions calculate_session_options() {
        this.ensure_token_info();
        if ((this._info.flags & CKF.WRITE_PROTECTED) == CKF.WRITE_PROTECTED)
            return Gck.SessionOptions.READ_ONLY;
        else
            return Gck.SessionOptions.READ_WRITE;
    }

    public Seahorse.Panel create_panel() {
        return new TokenPanel(this);
    }

    public async bool load(GLib.Cancellable? cancellable) throws GLib.Error {
        var checks = new GLib.HashTable<ulong?, GLib.Object>(ulong_hash, ulong_equal);

        /* Make note of all the objects that were there */
        update_token_info();
        for (uint i = 0; i < get_n_items(); i++) {
            var pair = (CertKeyPair) get_item(i);
            if (pair.certificate != null)
                checks.insert(pair.certificate.handle, pair.certificate);
            if (pair.private_key != null)
                checks.insert(pair.private_key.handle, pair.private_key);
        }

        if (this._session == null) {
            var options = calculate_session_options();
            this._session = yield this._slot.open_session_async(options, null, cancellable);
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
            CKA.SENSITIVE,
            CKA.DECRYPT,
            CKA.SIGN,
            CKA.SUBJECT,
            CKA.START_DATE,
            CKA.END_DATE,
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
