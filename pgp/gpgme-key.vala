/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2017 Niels De Graef
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

public class Seahorse.GpgME.Key : Pgp.Key, Deletable, Exportable {
    /* enum { */
    /*     PROP_0, */
    /*     PROP_PUBKEY, */
    /*     PROP_SECKEY, */
    /*     PROP_VALIDITY, */
    /*     PROP_TRUST */
    /* }; */

    /**
     * GPGME Public Key that this object represents
     */
    private GPG.Key? pubkey;

    /**
     * GPGME Secret Key that this object represents
     */
    private GPG.Key? seckey;
    // Whether we have a secret key or not
    private bool has_secret;

    // What to load our public key as
    private GPG.KeylistMode list_mode;
    // Photos were loaded
    private bool photos_loaded;

    // Loading is blocked while this flag is set
    private int block_loading;

    public bool deletable {
        get { return Seahorse.Flags.DELETABLE in this.flags; }
    }

    public bool exportable {
        get { return Seahorse.Flags.EXPORTABLE in this.flags; }
    }

    public Seahorse.Validity validity {
        get {
            if (!require_key_public(GPG.KeylistMode.LOCAL)
                || this.pubkey == null || this.pubkey.uids == null)
                return Seahorse.Validity.UNKNOWN;
            if (this.pubkey.revoked)
                return Seahorse.Validity.REVOKED;
            if (this.pubkey.disabled)
                return Seahorse.Validity.DISABLED;
            return this.pubkey.uids.validity.to_seahorse_validity();
        }
    }

    public Seahorse.Validity trust {
        get {
            if (!require_key_public(GPG.KeylistMode.LOCAL))
                return Seahorse.Validity.UNKNOWN;

            return this.pubkey.owner_trust.to_seahorse_validity();
        }
    }

    public Key (Seahorse.Place sksrc, GPG.Key pubkey, GPG.Key seckey){
        GLib.Object(place: sksrc,
                    pubkey: pubkey,
                    seckey: seckey);

        realize();
    }

    public GPG.Key? get_public() {
        if (require_key_public (self, GPG.KeylistMode.LOCAL))
            return this._pubkey;
        return null;
    }

    public void set_public(GPG.Key? key) {
        if (this.pubkey)
            gpgme_key_unref (this.pubkey);
        this.pubkey = key;
        if (this.pubkey) {
            gpgme_key_ref (this.pubkey);
            this.list_mode |= this.pubkey.keylist_mode;
        }

        realize();
        notify_property("fingerprint");
        notify_property("validity");
        notify_property("trust");
        notify_property("expires");
        notify_property("length");
        notify_property("algo");
    }

    public GPG.Key get_private() {
        if (require_key_private())
            return this._seckey;
        return null;	
    }

    public void set_private(GPG.Key? key) {
        if (this._seckey != null)
            gpgme_key_unref (this.seckey);
        this.seckey = key;
        if (this._seckey != null)
            gpgme_key_ref (this.seckey);

        realize();
    }

    public void refresh_matching() {
        if (this.subkeys.keyid == null)
            return;

        Key? key = Backend.get_default_keyring().lookup(this.subkeys.keyid);
        if (key != null)
            key.refresh();
    }

    /* -----------------------------------------------------------------------------
     * INTERNAL HELPERS
     */

    static bool load_gpgme_key(string keyid, GPG.KeylistMode mode, int secret, GPG.Key key) {
        GPG.Context ctx;
        GPG.Error error = GPG.Context.Context(out ctx);
        if (!error.is_success())
            return false;

        ctx.set_keylist_mode(mode);
        error = ctx.op_keylist_start (keyid, secret);
        if (!error.is_success()) {
            ctx.op_keylist_next(key);
            ctx.op_keylist_end();
        }

        // XXX exists?
        // gpgme_release (ctx);

        Error *gerror = null;
        if (seahorse_gpgme_propagate_error (error, &gerror)) {
            message("couldn't load GPGME key: %s", gerror->message);
            g_clear_error (&gerror);
            return false;
        }

        return true;
    }

    private void load_key_public(GPG.KeylistMode list_mode) {
        if (this.block_loading)
            return;

        list_mode |= this.list_mode;

        GPG.Key? key = null;
        if (load_gpgme_key(get_keyid(), list_mode, false, out key)) {
            this.list_mode = list_mode;
            set_public(key);
            gpgme_key_unref (key);
        }
    }

    private bool require_key_public(GPG.KeylistMode list_mode) {
        if (!this.pubkey || (this.list_mode & list_mode) != list_mode)
            load_key_public (self, list_mode);
        return this.pubkey && (this.list_mode & list_mode) == list_mode;
    }

    private void load_key_private() {
        if (!this.has_secret || this.block_loading)
            return;

        GPG.Key? key = null;
        if (load_gpgme_key (get_keyid(), GPG.KeylistMode.LOCAL, true, out key)) {
            set_private(key);
            gpgme_key_unref (key);
        }
    }

    private bool require_key_private() {
        if (!this.seckey)
            load_key_private();
        return this.seckey != null;
    }

    private bool require_key_uids() {
        return require_key_public(GPG.KeylistMode.LOCAL);
    }

    private bool require_key_subkeys() {
        return require_key_public(GPG.KeylistMode.LOCAL);
    }

    private void load_photos() {
        if (this.block_loading)
            return;

        GPG.Error gerr = KeyOperation.photos_load(this);
        if (!gerr.is_success())
            message("couldn't load key photos: %s", gpgme_strerror (gerr));
        else
            this.photos_loaded = true;
    }

    private bool require_photos() {
        if (!this.photos_loaded)
            load_photos();
        return this.photos_loaded;
    }

    // This function is necessary because the uid stored in a GPG.UserID
    // struct is only usable with gpgme functions.  Problems will be caused if
    // that uid is used with functions found in seahorse-gpgme-key-op.h.  This
    // function is only to be called with uids from GPG.UserID structs.
    private void renumber_actual_uids () {
        ++this.block_loading;
        List<Pgp.Photo> photos = this.photos;
        List<Pgp.Uid> uids = this.uids;
        --this.block_loading;

        // First we build a bitmap of where all the photo uid indexes are
        bool[] index_map = g_array_new (false, true, sizeof (bool));
        foreach (Pgp.Photo photo in photos) {
            int index = ((GpgME.Photo) photo).get_index();
            if (index >= index_map->len)
                g_array_set_size (index_map, index + 1);
            g_array_index (index_map, bool, index) = true;
        }

        // Now for each UID we add however many photo indexes are below the gpgme index
        foreach (Pgp.Uid uid in uids) {
            int index = ((GpgME.Uid) uid).get_gpgme_index();
            for (uint i = 0; i < index_map.length && i < index; ++i) {
                if (g_array_index (index_map, bool, index))
                    ++index;
            }
            ((GpgME.Uid) uid).set_actual_index(index + 1);
        }
    }

    private void realize_uids() {
        List<GPG.UserID> results = new List<GPG.UserID>();
        bool changed = false;

        GPG.UserID guid = this.pubkey ? this.pubkey.uids : null;

        // Look for out of sync or missing UIDs
        this.uids.foreach((uid) => {
            // Bring this UID up to date
            if (guid && uid.is_same(guid)) {
                if (uid.get_userid() != guid) {
                    uid.pubkey = this.pubkey;
                    uid.userid = guid;
                    changed = true;
                }
                results = seahorse_object_list_append (results, uid);
                guid = guid->next;
            }
        });

        // Add new UIDs
        while (guid != null) {
            Uid uid = new Uid(self, guid);
            changed = true;
            results = seahorse_object_list_append (results, uid);
            g_object_unref (uid);
            guid = guid->next;
        }

        if (changed)
            this.uids = results;
    }

    private void realize_subkeys() {
        List? list = null;

        if (this.pubkey) {
            for (GPG.SubKey gsubkey = this.pubkey.subkeys; gsubkey != null; gsubkey = gsubkey->next)
                list.prepend(new GpgME.Subkey(this.pubkey, gsubkey));

            list.reverse();
        }

        base.set_subkeys(list);
    }

    void realize() {
        if (this.pubkey == null || this.pubkey.subkeys == null
            || !require_key_public(GPG.KeylistMode.LOCAL))
            return;

        // Update the sub UIDs
        realize_uids();
        realize_subkeys();

        // The flags
        Seahorse.Flags flags = Seahorse.Flags.EXPORTABLE | Seahorse.Flags.DELETABLE;

        if (!this.pubkey.disabled && !this.pubkey.expired &&
            !this.pubkey.revoked && !this.pubkey.invalid) {
            if (this.validity >= Seahorse.Validity.MARGINAL)
                flags |= Seahorse.Flags.IS_VALID;
            if (this.pubkey.can_encrypt)
                flags |= Seahorse.Flags.CAN_ENCRYPT;
            if (this.seckey && this.pubkey.can_sign)
                flags |= Seahorse.Flags.CAN_SIGN;
        }

        if (this.pubkey.expired)
            flags |= Seahorse.Flags.EXPIRED;

        if (this.pubkey.revoked)
            flags |= Seahorse.Flags.REVOKED;

        if (this.pubkey.disabled)
            flags |= Seahorse.Flags.DISABLED;

        if (this.trust >= Seahorse.Validity.MARGINAL
            && !(this.pubkey.revoked || this.pubkey.disabled || this.pubkey.expired))
            flags |= Seahorse.Flags.TRUSTED;

        // The type
        Usage usage;
        if (this.seckey != null) {
            this.usage = Usage.PRIVATE_KEY;
            flags |= Seahorse.Flags.PERSONAL;
        } else {
            this.usage = Usage.PUBLIC_KEY;
        }

        this.object_flags = flags;
        this.actions = Actions.instance;

        base.realize();
    }

    public void ensure_signatures() {
        require_key_public(GPG.KeylistMode.LOCAL | GPG.KeylistMode.SIGS);
    }

    public void refresh () {
        if (this.pubkey != null)
            load_key_public(this.list_mode);
        if (this.seckey != null)
            load_key_private();
        if (this.photos_loaded)
            load_photos();
    }

    public override List<GPG.UserID> get_uids() {
        require_key_uids();
        return base.get_uids();
    }

    private void set_uids(List<Uid> uids) {
        base.set_uids(uids);

        // Keep our own copy of the UID list
        this.uids = uids.copy();

        renumber_actual_uids ();
    }

    public override List<GPG.SubKey> get_subkeys() {
        require_key_subkeys();
        return base.get_subkeys();
    }

    public override List<Pgp.Photo> get_photos() {
        require_photos();
        return base.get_photos();
    }

    private void set_photos(List<Photo> photos) {
        this.photos_loaded = true;
        base.set_photos(photos);
        renumber_actual_uids();
    }

    public GLib.List<Exporter> create_exporters(ExporterType type) {
        List<Exporter> exporters = new List<Seahorse.Exporter>();

        if (type == ExporterType.TEXTUAL)
            exporters.append(new Exporter(this, false, false));
        exporters.append(new Exporter(this, true, false));

        return exporters;
    }

    public Seahorse.Deleter create_deleter() {
        if (this.seckey != null)
            return new SecretDeleter(this);

        return new KeyDeleter(this);
    }

    public bool compare(Key? other) {
        if (other == null)
            return null;

        return this.keyid == other.keyid;
    }
}
