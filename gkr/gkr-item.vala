/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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

public enum Use {
	OTHER,
	NETWORK,
	WEB,
	PGP,
	SSH,
}

private struct DisplayInfo {
	string? item_type;
	string? label;
	string? details;
	string? description;
}

[CCode (has_target = false)]
private delegate void DisplayCustom(string label,
                                    GLib.HashTable<string, string> item_attrs,
                                    ref DisplayInfo info);

private struct DisplayEntry {
	string item_type;
	string description;
	DisplayCustom? custom_func;
}

public class Item : Secret.Item, Deletable, Viewable {
	public string description {
		owned get {
			ensure_display_info ();
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
		owned get { return new GLib.ThemedIcon (ICON_PASSWORD); }
	}

	public new string label {
		owned get {
			ensure_display_info ();
			return this._info.label;
		}
	}

	public string markup {
		owned get {
			ensure_display_info ();
			var result = new GLib.StringBuilder("");
			result.append(GLib.Markup.escape_text(this._info.label));
			result.append("<span size='small' rise='0' foreground='#555555'>\n");
			result.append(this._info.details);
			result.append("</span>");
			return result.str;
		}
	}

	public Usage usage {
		get { return Usage.CREDENTIALS; }
	}

	public Gtk.ActionGroup? actions {
		get { return null; }
	}

	public bool deletable {
		get { return true; }
	}

	private Secret.Value? _item_secret = null;
	private DisplayInfo? _info = null;
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

	private void ensure_display_info() {
		if (this._info != null)
			return;

		this._info = DisplayInfo();

		var attrs = this.attributes;
		var item_type = get_attribute_string (attrs, "xdg:schema");
		item_type = map_item_type_to_specific (item_type, attrs);
		assert (item_type != null);
		this._info.item_type = item_type;

		var label = base.get_label();
		foreach (var entry in DISPLAY_ENTRIES) {
			if (entry.item_type == item_type) {
				if (entry.custom_func != null)
					entry.custom_func(label, attrs, ref this._info);
				if (this._info.description == null)
					this._info.description = _(entry.description);
				break;
			}
		}

		if (this._info.label == null)
			this._info.label = label;
		if (this._info.label == null)
			this._info.label = "";
		if (this._info.details == null)
			this._info.details = "";
	}

	private void load_item_secret() {
		if (this._req_secret == null) {
			this._req_secret = new GLib.Cancellable();
			load_secret.begin(this._req_secret, (obj, res) => {
				try {
					this._req_secret = null;
					load_secret.end(res);
					this._item_secret = base.get_secret();
					notify_property("has-secret");
				} catch (GLib.Error err) {
					GLib.warning("Couldn't retrieve secret: %s", err.message);
				}
			});
		}
	}


	public new void refresh() {
		base.refresh();
		if (this._item_secret != null)
			load_item_secret ();
	}

	public new Secret.Value? get_secret() {
		if (this._item_secret == null)
			load_item_secret ();
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
}

private const string GENERIC_SECRET = "org.freedesktop.Secret.Generic";
private const string NETWORK_PASSWORD = "org.gnome.keyring.NetworkPassword";
private const string KEYRING_NOTE = "org.gnome.keyring.Note";
private const string CHAINED_KEYRING = "org.gnome.keyring.ChainedKeyring";
private const string ENCRYPTION_KEY = "org.gnome.keyring.EncryptionKey";
private const string PK_STORAGE = "org.gnome.keyring.PkStorage";
private const string CHROME_PASSWORD = "x.internal.Chrome";
private const string GOA_PASSWORD = "org.gnome.OnlineAccounts";
private const string TELEPATHY_PASSWORD = "org.freedesktop.Telepathy";
private const string EMPATHY_PASSWORD = "org.freedesktop.Empathy";
private const string NETWORK_MANAGER_SECRET = "org.freedesktop.NetworkManager";

private string? get_attribute_string(GLib.HashTable<string, string>? attrs,
                                     string name) {
	if (attrs == null)
		return null;
	return attrs.lookup(name);
}

private int get_attribute_int(GLib.HashTable<string, string>? attrs,
                              string name)
{
	if (attrs == null)
		return 0;

	var value = attrs.lookup(name);
	if (value != null)
		return int.parse(value);

	return 0;
}

private bool is_network_item(GLib.HashTable<string, string>? attrs,
                             string match)
{
	var protocol = get_attribute_string (attrs, "protocol");
	return protocol != null && protocol.ascii_casecmp(match) == 0;
}

private bool is_custom_network_label(string? server,
                                     string? user,
                                     string? object,
                                     int port,
                                     string? display)
{
	/*
	 * For network passwords gnome-keyring generates in a funky looking display
	 * name that's generated from login credentials. We simulate that generating
	 * here and return FALSE if it matches. This allows us to display user
	 * customized display names and ignore the generated ones.
	 */

	if (server == null)
		return true;

	var generated = new GLib.StringBuilder("");
	if (user != null)
		generated.append_printf("%s@", user);
	generated.append(server);
	if (port != 0)
		generated.append_printf(":%d", port);
	if (object != null)
		generated.append_printf ("/%s", object);

	return (generated.str == display);
}

private string? calc_network_label(GLib.HashTable<string, string>? attrs,
                                   bool always)
{
	/* HTTP usually has a the realm as the "object" display that */
	if (is_network_item (attrs, "http") && attrs != null) {
		var val = get_attribute_string (attrs, "object");
		if (val != null && val != "")
			return val;

		/* Display the server name as a last resort */
		if (always) {
			val = get_attribute_string (attrs, "server");
			if (val != null && val != "")
				return val;
		}
	}

	return null;
}

private void network_custom(string? display,
                            GLib.HashTable<string, string>? attrs,
                            ref DisplayInfo info)
{
	var server = get_attribute_string (attrs, "server");
	var protocol = get_attribute_string (attrs, "protocol");
	var object = get_attribute_string (attrs, "object");
	var user = get_attribute_string (attrs, "user");
	var port = get_attribute_int (attrs, "port");

	if (protocol == null)
		return;

	/* If it's customized by the application or user then display that */
	if (!is_custom_network_label (server, user, object, port, display))
		info.label = calc_network_label (attrs, true);
	if (info.label == null)
		info.label = display;

	string symbol = "@";
	if (user == null) {
		user = "";
		symbol = "";
	}

	if (object == null)
		object = "";

	if (server != null && protocol != null) {
		info.details = GLib.Markup.printf_escaped("%s://%s%s%s/%s",
		                                          protocol, user, symbol,
		                                          server, object);
	}
}

private void chrome_custom(string? display,
                           GLib.HashTable<string, string>? attrs,
                           ref DisplayInfo info)
{
	var origin = get_attribute_string (attrs, "origin_url");

	/* A brain dead url, parse */
	if (display != null && display == origin) {
		try {
			var regex = new GLib.Regex("[a-z]+://([^/]+)/", GLib.RegexCompileFlags.CASELESS, 0);
			GLib.MatchInfo match;
			if (regex.match(display, GLib.RegexMatchFlags.ANCHORED, out match)) {
				if (match.matches())
					info.label = match.fetch(1);
			}
		} catch (GLib.RegexError err) {
			GLib.critical("%s", err.message);
			return;
		}

	}

	if (info.label == null)
		info.label = display;
	if (origin != null)
		info.details = GLib.Markup.escape_text(origin);
	else
		info.details = "";
}

private string? decode_telepathy_id(string account) {
	account.replace("_", "%");
	return GLib.Uri.unescape_string(account);
}

private void empathy_custom(string? display,
                            GLib.HashTable<string, string>? attrs,
                            ref DisplayInfo info)
{
	var account = get_attribute_string(attrs, "account-id");

	/* Translators: This should be the same as the string in empathy */
	var prefix = _("IM account password for ");

	if (display != null && display.has_prefix(prefix)) {
		var len = prefix.length;
		var pos = display.index_of_char ('(', (int)len);
		if (pos != -1)
			info.label = display.slice(len, pos);

		try {
			var regex = new GLib.Regex("^.+/.+/(.+)$", GLib.RegexCompileFlags.CASELESS, 0);
			GLib.MatchInfo match;
			if (regex.match(account, GLib.RegexMatchFlags.ANCHORED, out match)) {
				if (match.matches())
					info.details = decode_telepathy_id (match.fetch(1));
			}
		} catch (GLib.RegexError err) {
			GLib.critical("%s", err.message);
			return;
		}
	}

	if (info.label == null)
		info.label = display;
	if (info.details == null)
		info.details = GLib.Markup.escape_text (account);
}

private void telepathy_custom(string? display,
                              GLib.HashTable<string, string>? attrs,
                              ref DisplayInfo info)
{
	var account = get_attribute_string (attrs, "account");
	if (account != null && display != null && display.index_of(account) != -1) {
		try {
			var regex = new GLib.Regex("^.+/.+/(.+)$", GLib.RegexCompileFlags.CASELESS, 0);
			GLib.MatchInfo match;
			if (regex.match(account, GLib.RegexMatchFlags.ANCHORED, out match)) {
				if (match.matches()) {
					var identifier = match.fetch(1);
					info.label = decode_telepathy_id(identifier);
				}
			}
		} catch (GLib.RegexError err) {
			GLib.critical("%s", err.message);
			return;
		}
	}

	if(info.label == null)
		info.label = display;
	if(account != null)
		info.details = GLib.Markup.escape_text(account);
}


private const DisplayEntry[] DISPLAY_ENTRIES = {
	{ GENERIC_SECRET, N_("Password or secret"), null},
	{ NETWORK_PASSWORD, N_("Network password"), network_custom },
	{ KEYRING_NOTE, N_("Stored note"), null },
	{ CHAINED_KEYRING, N_("Keyring password"), null },
	{ ENCRYPTION_KEY, N_("Encryption key password"), null },
	{ PK_STORAGE, N_("Key storage password"), null },
	{ CHROME_PASSWORD, N_("Google Chrome password"), chrome_custom },
	{ GOA_PASSWORD, N_("Gnome Online Accounts password"), null },
	{ TELEPATHY_PASSWORD, N_("Telepathy password"), telepathy_custom },
	{ EMPATHY_PASSWORD, N_("Instant messaging password"), empathy_custom },
	{ NETWORK_MANAGER_SECRET, N_("Network Manager secret"), null },
};

private struct MappingEntry {
	string item_type;
	string mapped_type;
	string match_attribute;
	string? match_pattern;
}

private const MappingEntry[] MAPPING_ENTRIES = {
	{ GENERIC_SECRET, CHROME_PASSWORD, "application", "chrome*" },
	{ GENERIC_SECRET, GOA_PASSWORD, "goa-identity", null },
	{ GENERIC_SECRET, TELEPATHY_PASSWORD, "account", "*/*/*" },
	{ GENERIC_SECRET, EMPATHY_PASSWORD, "account-id", "*/*/*" },
	/* Network secret for Auto anna/802-11-wireless-security/psk */
	{ GENERIC_SECRET, NETWORK_MANAGER_SECRET, "connection-uuid", null },
};

private string map_item_type_to_specific(string? item_type,
                                         GLib.HashTable<string, string>? attrs)
{

	if (item_type == null)
		return GENERIC_SECRET;
	if (attrs == null)
		return item_type;

	foreach (var mapping in MAPPING_ENTRIES) {
		if (item_type == mapping.item_type) {
			var value = get_attribute_string (attrs, mapping.match_attribute);
			if (value != null && mapping.match_pattern != null) {
				if (GLib.PatternSpec.match_simple(mapping.match_pattern, value))
					return mapping.mapped_type;
			} else if (value != null) {
				return mapping.mapped_type;
			}
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
			return new DeleteDialog(parent, _("Are you sure you want to delete the password '%s'?"), label);
		} else {
			return new DeleteDialog(parent, GLib.ngettext ("Are you sure you want to delete %d password?",
			                                               "Are you sure you want to delete %d passwords?", num), num);
		}
	}

	public ItemDeleter(Item item) {
		if (!add_object(item))
			GLib.assert_not_reached();
	}

	public override unowned GLib.List<weak GLib.Object> get_objects() {
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
}
