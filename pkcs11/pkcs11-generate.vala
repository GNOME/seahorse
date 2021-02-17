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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

// FIXME: damn broken bindings
extern Gcr.CollectionModel gcr_collection_model_new(Gcr.Collection collection,
                                                    Gcr.CollectionModelMode mode, ...);

[GtkTemplate (ui = "/org/gnome/Seahorse/seahorse-pkcs11-generate.ui")]
public class Seahorse.Pkcs11.Generate : Gtk.Dialog {

    [GtkChild]
    private unowned Gtk.Entry label_entry;

    private Pkcs11.Token? token;
    [GtkChild]
    private unowned Gtk.ComboBox token_box;
    private Gcr.CollectionModel? token_model;

    private Gck.Mechanism? mechanism;
    private Gtk.ListStore? mechanism_store;
    [GtkChild]
    private unowned Gtk.ComboBox mechanism_box;

    [GtkChild]
    private unowned Gtk.SpinButton key_bits;

    private Cancellable? cancellable;
    private Gck.Attributes? pub_attrs;
    private Gck.Attributes? prv_attrs;

    private enum Column {
        LABEL,
        TYPE,
        N_COLS
    }

    private struct Mechanism {
        ulong mechanism_type;
        unowned string label;
    }
    private const Mechanism[] AVAILABLE_MECHANISMS = {
        { Cryptoki.MechanismType.RSA_PKCS_KEY_PAIR_GEN, N_("RSA") },
    };

    construct {
        this.use_header_bar = 1;
        this.key_bits.set_range(0, int.MAX); /* updated later */
        this.key_bits.set_increments(128, 128);
        this.key_bits.set_value(2048);

        // The mechanism
        this.mechanism_store = new Gtk.ListStore(2, typeof(string), typeof(ulong));
        this.mechanism_store.set_default_sort_func(on_mechanism_sort);
        this.mechanism_store.set_sort_column_id(Gtk.TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, Gtk.SortType.ASCENDING);
        this.mechanism_box.set_model(this.mechanism_store);
        Gtk.CellRenderer renderer = new Gtk.CellRendererText();
        this.mechanism_box.pack_start(renderer, true);
        this.mechanism_box.add_attribute(renderer, "markup", Column.LABEL);
        this.mechanism_box.changed.connect(on_mechanism_changed);

        // The tokens
        Gcr.Collection collection = Pkcs11.Backend.get_writable_tokens(null, Cryptoki.MechanismType.RSA_PKCS_KEY_PAIR_GEN);
        this.token_model = gcr_collection_model_new(collection, Gcr.CollectionModelMode.LIST,
                                                    "icon", typeof(Icon), "label", typeof(string),
                                                    null);
        this.token_model.set_sort_column_id(1, Gtk.SortType.ASCENDING);
        this.token_box.set_model(this.token_model);
        Gtk.CellRendererPixbuf icon_renderer = new Gtk.CellRendererPixbuf();
        icon_renderer.stock_size = Gtk.IconSize.BUTTON;
        this.token_box.pack_start(icon_renderer, false);
        this.token_box.add_attribute(icon_renderer, "gicon", 0);
        renderer = new Gtk.CellRendererText();
        this.token_box.pack_start(renderer, true);
        this.token_box.add_attribute(renderer, "text", 1);
        this.token_box.changed.connect(on_token_changed);
        if (collection.get_length() > 0)
            this.token_box.active = 0;

        set_default_response (Gtk.ResponseType.OK);

        update_response ();
    }

    public Generate(Gtk.Window? parent) {
        GLib.Object(transient_for: parent);
    }

    ~Generate() {
        this.token = null;
        this.token_model = null;

        this.mechanism = null;
        this.mechanism_store = null;

        this.cancellable = null;
        this.pub_attrs = this.prv_attrs = null;
    }

    private void update_response() {
        set_response_sensitive(Gtk.ResponseType.OK, this.token != null && this.mechanism != null);
    }

    private unowned string? get_available_mechanism_label (ulong type) {
        foreach (Mechanism mechanism in AVAILABLE_MECHANISMS) {
            if (mechanism.mechanism_type == type)
                return mechanism.label;
        }

        return null;
    }

    private void on_token_changed(Gtk.ComboBox combo_box) {
        this.token = null;

        Gtk.TreeIter iter;
        if (combo_box.get_active_iter(out iter)) {
            this.token = (Pkcs11.Token) this.token_model.object_for_iter(iter);
        }

        bool valid = this.mechanism_store.get_iter_first(out iter);
        if (this.token != null) {
            for (uint i = 0; i < this.token.mechanisms.length; i++) {
                ulong type = this.token.mechanisms.index(i);
                unowned string? label = get_available_mechanism_label (type);
                if (label == null)
                    continue;
                while (valid) {
                    ulong otype;
                    this.mechanism_store.get(iter, Column.TYPE, out otype);
                    if (otype == type)
                        break;
                    valid = this.mechanism_store.remove(ref iter);
                }
                if (!valid)
                    this.mechanism_store.append(out iter);
                this.mechanism_store.set(iter, Column.TYPE, type,
                                               Column.LABEL, label);
                valid = this.mechanism_store.iter_next(ref iter);
            }
        }
        while (valid)
            valid = this.mechanism_store.remove(ref iter);

        /* Select first mechanism if none are selected */
        if (!this.mechanism_box.get_active_iter(out iter))
            if (this.mechanism_store.get_iter_first(out iter))
                this.mechanism_box.set_active_iter(iter);

        update_response ();
    }

    private void on_mechanism_changed(Gtk.ComboBox? widget) {
        this.mechanism = null;

        Gtk.TreeIter iter;
        if (widget.get_active_iter(out iter)) {
            this.mechanism = Gck.Mechanism();
            this.mechanism_store.get(iter, Column.TYPE, out this.mechanism.type);

            Gck.Slot slot = token.slot;
            Gck.MechanismInfo info = slot.get_mechanism_info(this.mechanism.type);

            ulong min = info.min_key_size;
            ulong max = info.max_key_size;

            if (min < 512 && max >= 512)
                min = 512;
            if (max > 16384 && min <= 16384)
                max = 16384;
            this.key_bits.set_range(min, max);
        }

        this.key_bits.sensitive = (this.mechanism != null);

        update_response();
    }

    private int on_mechanism_sort (Gtk.TreeModel? model, Gtk.TreeIter a, Gtk.TreeIter b) {
        string label_a, label_b;
        model.get(a, Column.LABEL, out label_a);
        model.get(b, Column.LABEL, out label_b);
        return label_a.collate(label_b);
    }

    public override void response (int response_id) {
        if (response_id == Gtk.ResponseType.OK)
            generate.begin();

        hide();
    }

    private async void generate() {
        assert(this.token != null);

        prepare_generate();

        GLib.TlsInteraction interaction = new Seahorse.Interaction(this.transient_for);
        Progress.show(this.cancellable, _("Generating key"), false);

        try {
            Gck.Session session = yield Gck.Session.open_async(this.token.slot,
                       Gck.SessionOptions.READ_WRITE | Gck.SessionOptions.LOGIN_USER,
                       interaction, this.cancellable);
            Gck.Object pub, priv;
            yield session.generate_key_pair_async(this.mechanism, this.pub_attrs, this.prv_attrs,
                                                  this.cancellable, out pub, out priv);
            yield this.token.load(this.cancellable);
            this.cancellable = null;
            this.pub_attrs = this.prv_attrs = null;
        } catch (Error e) {
            Util.show_error(null, _("Couldn’t generate private key"), e.message);
        }
    }

    private void prepare_generate() {
        const uchar[] rsa_public_exponent = { 0x01, 0x00, 0x01 }; /* 65537 in bytes */

        Gck.Builder publi = new Gck.Builder(Gck.BuilderFlags.SECURE_MEMORY);
        Gck.Builder priva = new Gck.Builder(Gck.BuilderFlags.SECURE_MEMORY);

        assert (this.cancellable == null);
        assert (this.mechanism != null);

        publi.add_ulong(Cryptoki.Attribute.CLASS, Cryptoki.ObjectClass.PUBLIC_KEY);
        priva.add_ulong(Cryptoki.Attribute.CLASS, Cryptoki.ObjectClass.PRIVATE_KEY);

        publi.add_boolean(Cryptoki.Attribute.TOKEN, true);
        priva.add_boolean(Cryptoki.Attribute.TOKEN, true);

        priva.add_boolean(Cryptoki.Attribute.PRIVATE, true);
        priva.add_boolean(Cryptoki.Attribute.SENSITIVE, true);

        string label = this.label_entry.text;
        publi.add_string(Cryptoki.Attribute.LABEL, label);
        priva.add_string(Cryptoki.Attribute.LABEL, label);

        if (this.mechanism.type == Cryptoki.MechanismType.RSA_PKCS_KEY_PAIR_GEN) {
            publi.add_boolean(Cryptoki.Attribute.ENCRYPT, true);
            publi.add_boolean(Cryptoki.Attribute.VERIFY, true);
            publi.add_boolean(Cryptoki.Attribute.WRAP, true);

            priva.add_boolean(Cryptoki.Attribute.DECRYPT, true);
            priva.add_boolean(Cryptoki.Attribute.SIGN, true);
            priva.add_boolean(Cryptoki.Attribute.UNWRAP, true);

            publi.add_data(Cryptoki.Attribute.PUBLIC_EXPONENT, rsa_public_exponent);
            publi.add_ulong(Cryptoki.Attribute.MODULUS_BITS, this.key_bits.get_value_as_int());
        } else {
            warning("currently no support for this mechanism");
        }

        this.prv_attrs = priva.steal();
        this.pub_attrs = publi.steal();

        publi.clear();
        priva.clear();
    }
}
