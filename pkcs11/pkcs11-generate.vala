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

[GtkTemplate (ui = "/org/gnome/Seahorse/pkcs11-generate.ui")]
public class Seahorse.Pkcs11.Generate : Gtk.ApplicationWindow {

    [GtkChild] private unowned Adw.EntryRow label_row;
    [GtkChild] private unowned Adw.ComboRow token_row;

    private Gck.Mechanism? mechanism;
    [GtkChild] private unowned Adw.ComboRow mechanism_row;
    [GtkChild] private unowned GLib.ListStore mechanism_items;

    [GtkChild] private unowned Adw.SpinRow key_size_row;

    private Cancellable? cancellable;
    private Gck.Attributes? pub_attrs;
    private Gck.Attributes? prv_attrs;

    // Known mechanisms with their label
    private struct MechanismEntry {
        ulong mechanism_type;
        string label;
    }
    private const MechanismEntry[] AVAILABLE_MECHANISMS = {
        { CKM.RSA_PKCS_KEY_PAIR_GEN, N_("RSA") },
    };

    // Helper object for GListModel
    public class MechanismListItem : GLib.Object {
        public ulong mechanism_type { get; construct set; }
        public string label { get; construct set; }

        public MechanismListItem(ulong type, string label) {
            GLib.Object(mechanism_type: type, label: label);
        }
    }

    static construct {
        install_action("create", null, (Gtk.WidgetActionActivateFunc) create_action);

        typeof(MechanismListItem).ensure();
    }

    construct {
        // The tokens
        var backend = Pkcs11.Backend.get();
        var filter = new Pkcs11.TokenFilter();
        filter.only_writable = true;
        filter.mechanism = CKM.RSA_PKCS_KEY_PAIR_GEN;
        var writable_tokens_model = new Gtk.FilterListModel(backend, filter);
        this.token_row.model = writable_tokens_model;
        // Hide the row when no writable tokens available
        // (another row will pop up with an explanatory label)
        this.token_row.visible = (writable_tokens_model.get_n_items() > 0);
        writable_tokens_model.items_changed.connect((model, pos, removed, added) => {
            this.token_row.visible = (model.get_n_items() > 0);
        });

        // Adapt the available mechanisms on the selected token
        this.token_row.notify["selected-item"].connect(on_selected_token_changed);
        on_selected_token_changed(this.token_row, null);

        validate_input();
    }

    public Generate(Gtk.Window? parent) {
        GLib.Object(transient_for: parent);
    }

    private void validate_input() {
        action_set_enabled("create",
                           this.token_row.selected_item != null &&
                           this.mechanism_row.selected_item != null);
    }

    private void on_selected_token_changed(GLib.Object object, ParamSpec? spec) {
        var token = (Pkcs11.Token?) this.token_row.selected_item;

        var new_tokens = new GenericArray<MechanismListItem>();

        if (token != null) {
            for (uint i = 0; i < token.mechanisms.length; i++) {
                ulong type = token.mechanisms.index(i);

                // Only add types that havea known label
                unowned string? label = get_available_mechanism_label (type);
                if (label == null)
                    continue;

                new_tokens.add(new MechanismListItem(type, label));
            }
        }

        this.mechanism_items.splice(0, this.mechanism_items.get_n_items(), new_tokens.data);

        validate_input();
    }

    private unowned string? get_available_mechanism_label (ulong type) {
        foreach (unowned var mechanism in AVAILABLE_MECHANISMS) {
            if (mechanism.mechanism_type == type)
                return mechanism.label;
        }

        return null;
    }

    [GtkCallback]
    private void on_mechanism_selected(GLib.Object object, ParamSpec pspec)
            requires(this.token_row.selected_item != null) {
        var token = (Pkcs11.Token) this.token_row.selected_item;
        this.mechanism = null;

        var selected = (MechanismListItem?) this.mechanism_row.selected_item;
        if (selected != null) {
            this.mechanism = Gck.Mechanism();
            this.mechanism.type = selected.mechanism_type;

            var slot = token.slot;
            var info = slot.get_mechanism_info(this.mechanism.type);

            ulong min = info.min_key_size;
            ulong max = info.max_key_size;

            if (min < 512 && max >= 512)
                min = 512;
            if (max > 16384 && min <= 16384)
                max = 16384;
            this.key_size_row.set_range(min, max);
        }

        this.key_size_row.sensitive = (this.mechanism != null);

        validate_input();
    }

    private void create_action(string action_name, Variant? param) {
        create.begin();
    }

    private async void create()
            requires(this.token_row.selected_item != null) {
        close();

        var token = (Pkcs11.Token) this.token_row.selected_item;

        prepare_generate();

        var interaction = new Seahorse.Interaction(this.transient_for);
        Progress.show(this.cancellable, _("Generating key"), false);

        try {
            Gck.Session session = yield Gck.Session.open_async(token.slot,
                       Gck.SessionOptions.READ_WRITE | Gck.SessionOptions.LOGIN_USER,
                       interaction, this.cancellable);
            Gck.Object pub, priv;
            yield session.generate_key_pair_async(this.mechanism, this.pub_attrs, this.prv_attrs,
                                                  this.cancellable, out pub, out priv);
            yield token.load(this.cancellable);
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

        assert(this.cancellable == null);
        assert(this.mechanism != null);

        publi.add_ulong(CKA.CLASS, CKO.PUBLIC_KEY);
        priva.add_ulong(CKA.CLASS, CKO.PRIVATE_KEY);

        publi.add_boolean(CKA.TOKEN, true);
        priva.add_boolean(CKA.TOKEN, true);

        priva.add_boolean(CKA.PRIVATE, true);
        priva.add_boolean(CKA.SENSITIVE, true);

        string label = this.label_row.text;
        publi.add_string(CKA.LABEL, label);
        priva.add_string(CKA.LABEL, label);

        if (this.mechanism.type == CKM.RSA_PKCS_KEY_PAIR_GEN) {
            publi.add_boolean(CKA.ENCRYPT, true);
            publi.add_boolean(CKA.VERIFY, true);
            publi.add_boolean(CKA.WRAP, true);

            priva.add_boolean(CKA.DECRYPT, true);
            priva.add_boolean(CKA.SIGN, true);
            priva.add_boolean(CKA.UNWRAP, true);

            publi.add_data(CKA.PUBLIC_EXPONENT, rsa_public_exponent);
            publi.add_ulong(CKA.MODULUS_BITS, (int) this.key_size_row.value);
        } else {
            warning("currently no support for this mechanism");
        }

        this.prv_attrs = priva.end();
        this.pub_attrs = publi.end();

        publi.clear();
        priva.clear();
    }
}
