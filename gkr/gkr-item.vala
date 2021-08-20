/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2020 Niels De Graef
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

namespace Seahorse.Gkr {

public enum Use {
    OTHER,
    NETWORK,
    WEB,
    PGP,
    SSH,
}

/**
 * The Seahorse-specific class for entries in a libsecret keyring.
 *
 * Since libsecret creates the object for us, we can't really add stuff using a
 * constructor, so to add specific info for certain categories of items, we
 * stuff everything into the {@link ItemInfo} field.
 */
public class Item : Secret.Item, Deletable, Viewable {

    public string description {
        owned get {
            ensure_item_info();
            return this._info.description;
        }
    }

    public Use use {
        get {
            var schema_name = get_attribute("xdg:schema");
            if (schema_name != null && schema_name == NETWORK_PASSWORD)
                return Use.NETWORK;
            return Use.OTHER;
        }
    }

    public bool has_secret {
        get { return _item_secret != null; }
    }

    public Keyring place {
        owned get { return (Keyring)_place.get(); }
        set { _place.set(value); }
    }

    public Flags object_flags {
        get { return Flags.DELETABLE | Flags.PERSONAL; }
    }

    public GLib.Icon icon {
        owned get {
            ensure_item_info();
            return this._info.icon ?? new GLib.ThemedIcon (ICON_PASSWORD);
        }
    }

    public new string label {
        owned get {
            ensure_item_info ();
            return this._info.label;
        }
    }

    public string markup {
        owned get {
            ensure_item_info ();
            var result = new StringBuilder("");
            result.append(Markup.escape_text(this._info.label ?? ""));
            if (this._info.details != null && this._info.details != "") {
                result.append("<span size='small' rise='0' foreground='#555555'>\n");
                result.append(this._info.details);
                result.append("</span>");
            }
            return result.str;
        }
    }

    public Usage usage {
        get { return Usage.CREDENTIALS; }
    }

    public bool deletable {
        get { return true; }
    }

    private Secret.Value? _item_secret = null;
    private ItemInfo? _info = null;
    private GLib.WeakRef _place;
    private GLib.Cancellable? _req_secret = null;

    construct {
        g_properties_changed.connect((changed_properties, invalidated_properties) => {
            this._info = null;
            freeze_notify();
            notify_property("has-secret");
            notify_property("use");
            notify_property("label");
            notify_property("icon");
            notify_property("markup");
            notify_property("description");
            notify_property("object-flags");
            thaw_notify();
        });
    }

    public override void dispose() {
        if (this._req_secret != null)
            this._req_secret.cancel();
        this._req_secret = null;
        base.dispose();
    }

    public Deleter create_deleter() {
        return new ItemDeleter(this);
    }

    public Gtk.Window? create_viewer(Gtk.Window? parent) {
        return new ItemProperties(this, parent);
    }

    private void ensure_item_info() {
        if (this._info != null)
            return;

        unowned string? item_type = null;
        var attrs = this.attributes;
        if (attrs != null)
            item_type = attrs.lookup("xdg:schema");
        item_type = map_item_type_to_specific(item_type, attrs);
        assert (item_type != null);

        var label = base.get_label();

        this._info = item_type_to_item_info(item_type, label, attrs);

        // If we look at the schema, we might be able to deduce the app (icon)
        if (this._info.icon == null)
            this._info.query_installed_apps(item_type);
    }

    private ItemInfo item_type_to_item_info(string item_type, string? label, HashTable<string, string>? attrs) {
        switch (item_type) {
            case GENERIC_SECRET:
                return new ItemInfo(label, attrs);
            case KEYRING_NOTE:
                return new ItemInfo(label, attrs, _("Stored note"));
            case CHAINED_KEYRING:
                return new ItemInfo(label, attrs, _("Keyring password"));
            case ENCRYPTION_KEY:
                return new ItemInfo(label, attrs, _("Encryption key password"));
            case PK_STORAGE:
                return new ItemInfo(label, attrs, _("Key storage password"));
            case NETWORK_MANAGER_SECRET:
                return new ItemInfo(label, attrs, _("Network Manager secret"));
            case NETWORK_PASSWORD:
                return new GkrNetworkPassInfo(label, attrs);
            case EPIPHANY_PASSWORD:
                return new EpiphanyPassInfo(label, attrs);
            case CHROME_PASSWORD:
                return new ChromePassInfo(label, attrs);
            case GOA_PASSWORD:
                return new GoaInfo(label, attrs);
            case TELEPATHY_PASSWORD:
                return new TelepathyInfo(label, attrs);
            case EMPATHY_PASSWORD:
                return new EmpathyInfo(label, attrs);
            case NETWORK_MANAGER_CONNECTION:
                return new NmConnectionInfo(label, attrs);
            default:
                break;
        }

        return new ItemInfo(label, attrs);
    }

    private async void load_item_secret() throws GLib.Error {
        if (this._req_secret != null)
            return;

        this._req_secret = new GLib.Cancellable();

        yield load_secret(this._req_secret);
        this._req_secret = null;
        this._item_secret = base.get_secret();
        notify_property("has-secret");
    }

    public new void refresh() {
        base.refresh();

        if (this._item_secret != null)
            return;

        load_item_secret.begin((obj, res) => {
            try {
                load_item_secret.end(res);
            } catch (GLib.Error e) {
                warning("Couldn't load secret: %s", e.message);
            }
        });
    }

    public new Secret.Value? get_secret() {
        if (this._item_secret == null) {
            load_item_secret.begin((obj, res) => {
                try {
                    load_item_secret.end(res);
                } catch (GLib.Error e) {
                    warning("Couldn't load secret: %s", e.message);
                }
            });
        }
        return this._item_secret;
    }

    public string? get_attribute(string name) {
        if (this.attributes == null)
            return null;
        return this.attributes.lookup(name);
    }

    public new async bool set_secret(Secret.Value value,
                                     GLib.Cancellable? cancellable) throws GLib.Error {
        yield base.set_secret(value, cancellable);
        _item_secret = value;
        notify_property("has-secret");
        return true;
    }

    public async void copy_secret_to_clipboard(Gtk.Clipboard clipboard) throws GLib.Error {
        if (this._item_secret == null)
            yield load_item_secret();

        if (this._item_secret == null) {
            debug("Can't copy to clipboard: secret is NULL");
            return;
        }

        unowned string? password = this._item_secret.get_text();
        if (password == null)
            return;

        clipboard.set_text(password, -1);
        debug("Succesfully copied secret to clipboard");
    }
}

private const string GENERIC_SECRET = "org.freedesktop.Secret.Generic";
private const string NETWORK_PASSWORD = "org.gnome.keyring.NetworkPassword";
private const string KEYRING_NOTE = "org.gnome.keyring.Note";
private const string CHAINED_KEYRING = "org.gnome.keyring.ChainedKeyring";
private const string ENCRYPTION_KEY = "org.gnome.keyring.EncryptionKey";
private const string PK_STORAGE = "org.gnome.keyring.PkStorage";
private const string CHROME_PASSWORD = "x.internal.Chrome";
private const string EPIPHANY_PASSWORD = "x.internal.Epiphany";
private const string GOA_PASSWORD = "org.gnome.OnlineAccounts";
private const string TELEPATHY_PASSWORD = "org.freedesktop.Telepathy";
private const string EMPATHY_PASSWORD = "org.freedesktop.Empathy";
private const string NETWORK_MANAGER_SECRET = "org.freedesktop.NetworkManager";
private const string NETWORK_MANAGER_CONNECTION = "org.freedesktop.NetworkManager.Connection";

private struct MappingEntry {
    unowned string item_type;
    unowned string mapped_type;
    unowned string? match_attribute;
    unowned string? match_pattern;
}

private const MappingEntry[] MAPPING_ENTRIES = {
    /* Map some known schema's to their application IDs */
    { "org.gnome.Polari.Identify", "org.gnome.Polari" },
    { "_chrome_dummy_schema_for_unlocking", "google-chrome" },
    { "chrome_libsecret_os_crypt_password_v2", "chromium-browser" },

    /* Browsers */
    { "org.epiphany.FormPassword", EPIPHANY_PASSWORD },
    { "chrome_libsecret_password_schema", CHROME_PASSWORD },
    { GENERIC_SECRET, CHROME_PASSWORD, "application", "chrome*" },

    { GENERIC_SECRET, GOA_PASSWORD, "goa-identity", null },
    { GENERIC_SECRET, TELEPATHY_PASSWORD, "account", "*/*/*" },
    { GENERIC_SECRET, EMPATHY_PASSWORD, "account-id", "*/*/*" },
    /* Network secret for Auto anna/802-11-wireless-security/psk */
    { GENERIC_SECRET, NETWORK_MANAGER_SECRET, "connection-uuid", null },
};

private unowned string map_item_type_to_specific(string? item_type,
                                                 HashTable<string, string>? attrs) {
    if (item_type == null)
        return GENERIC_SECRET;
    if (attrs == null)
        return item_type;

    foreach (var mapping in MAPPING_ENTRIES) {
        if (item_type != mapping.item_type)
            continue;

        if (mapping.match_attribute == null)
            return mapping.mapped_type;

        unowned string? value = null;
        if (attrs != null)
            value = attrs.lookup(mapping.match_attribute);
        if (value != null && mapping.match_pattern != null) {
            if (GLib.PatternSpec.match_simple(mapping.match_pattern, value))
                return mapping.mapped_type;
        } else if (value != null) {
            return mapping.mapped_type;
        }
    }

    return item_type;
}

class ItemDeleter : Deleter {
    private GLib.List<Item> _items;

    public override Gtk.Dialog create_confirm(Gtk.Window? parent) {
        var num = this._items.length();
        if (num == 1) {
            var label = ((Secret.Item)_items.data).label;
            return new DeleteDialog(parent, _("Are you sure you want to delete the password “%s”?"), label);
        } else {
            return new DeleteDialog(parent, ngettext("Are you sure you want to delete %d password?",
                                                     "Are you sure you want to delete %d passwords?", num), num);
        }
    }

    public ItemDeleter(Item item) {
        if (!add_object(item))
            assert_not_reached();
    }

    public override unowned GLib.List<GLib.Object> get_objects() {
        return this._items;
    }

    public override bool add_object (GLib.Object obj) {
        if (obj is Item) {
            this._items.append((Item)obj);
            return true;
        }
        return false;
    }

    public override async bool delete(GLib.Cancellable? cancellable) throws GLib.Error {
        var items = _items.copy();
        foreach (var item in items)
            yield item.delete(cancellable);
        return true;
    }
}

}
