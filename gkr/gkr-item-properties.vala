/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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
namespace Gkr {

public class ItemProperties : Gtk.Dialog {
	public Item item { construct; get; }

	private Gtk.Builder _builder;
	private Gtk.Entry _password_entry;
	private Gtk.Expander _password_expander;
	private bool _password_changed;
	private bool _updating_password;
	private bool _updating_description;

	construct {
		this._builder = new Gtk.Builder();
		try {
			string path = GLib.Path.build_filename(Config.UIDIR, "seahorse-gkr-item-properties.xml");
			this._builder.add_from_file(path);
		} catch (GLib.Error err) {
			GLib.critical ("%s", err.message);
		}

		this.add_button(Gtk.Stock.CLOSE, Gtk.ResponseType.CLOSE);
		var content = (Gtk.Widget)this._builder.get_object("gkr-item-properties");
		((Gtk.Container)this.get_content_area()).add(content);
		content.show();

		this.response.connect((response) => {
			this.destroy();
		});

		/* Setup the image properly */
		this.item.bind_property ("icon", this._builder.get_object("key-image"), "gicon",
		                         GLib.BindingFlags.SYNC_CREATE);

		/* Setup the label properly */
		Gtk.Entry description = (Gtk.Entry)this._builder.get_object("description-field");
		this.item.bind_property("label", description, "text", GLib.BindingFlags.SYNC_CREATE);
		description.activate.connect(() => {
			description_activate(description);
		});
		description.focus_out_event.connect_after(() => {
			description_activate(description);
			return false;
		});

		/* The expander for showing password */
		this._password_expander = (Gtk.Expander)this._builder.get_object("password-expander");
		this._password_expander.activate.connect_after(expander_activate);

		/* The check button for password visibility */
		Gtk.CheckButton check = (Gtk.CheckButton)this._builder.get_object("show-password-check");
		check.toggled.connect(() => {
			this._password_entry.visibility = check.active;
		});

		/* Window title */
		this.item.bind_property ("label", this, "title", GLib.BindingFlags.SYNC_CREATE);

		/* Update as appropriate */
		this.item.notify.connect((pspec) => {
			switch(pspec.name) {
			case "use":
				update_use();
				update_type();
				update_visibility();
				break;
			case "attributes":
				update_details();
				update_server();
				update_user();
				break;
			case "has-secret":
				password_display();
				break;
			}
		});

		/* Create the password entry */
		var buffer = new Gcr.SecureEntryBuffer();
		this._password_entry = new Gtk.Entry.with_buffer(buffer);
		Gtk.Box box = (Gtk.Box)this._builder.get_object("password-box-area");
		box.add(this._password_entry);
		this._password_entry.visibility = false;
		this._password_entry.show();
		this._password_changed = false;
		this._updating_password = false;

		/* Now watch for changes in the password */
		this._password_entry.activate.connect(password_activate);
		this._password_entry.changed.connect(() => {
			this._password_changed = true;
		});
		this._password_entry.focus_out_event.connect_after(() => {
			password_activate ();
			return false;
		});

		/* Sensitivity of the password entry */
		this.item.bind_property("has-secret", this._password_entry, "sensitive");
	}

	public ItemProperties(Item item,
	                      Gtk.Window? parent) {
		GLib.Object (
			item: item,
			transient_for: parent
		);

		item.refresh();
	}

	private void update_use() {
		Gtk.Label use = (Gtk.Label)this._builder.get_object("use-field");
		switch (this.item.use) {
		case Use.NETWORK:
			use.label = _("Access a network share or resource");
			break;
		case Use.WEB:
			use.label = _("Access a website");
			break;
		case Use.PGP:
			use.label = _("Unlocks a PGP key");
			break;
		case Use.SSH:
			use.label = _("Unlocks a Secure Shell key");
			break;
		case Use.OTHER:
			use.label = _("Saved password or login");
			break;
		default:
			use.label = "";
			break;
		};
	}

	private void update_type() {
		Gtk.Label type = (Gtk.Label)this._builder.get_object("type-field");
		switch (item.use) {
		case Use.NETWORK:
		case Use.WEB:
			type.label = _("Network Credentials");
			break;
		case Use.PGP:
		case Use.SSH:
		case Use.OTHER:
			type.label = _("Password");
			break;
		default:
			type.label = "";
			break;
		};
	}

	private void update_visibility() {
		var use = this.item.use;
		bool visible = use == Use.NETWORK || use == Use.WEB;
		this._builder.get_object("server-label").set("visible", visible);
		this._builder.get_object("server-field").set("visible", visible);
		this._builder.get_object("login-label").set("visible", visible);
		this._builder.get_object("login-field").set("visible", visible);
	}

	private void update_server() {
		Gtk.Label server = (Gtk.Label)this._builder.get_object("server-label");
		var value = this.item.get_attribute("server");
		if (value == null)
			value = "";
		server.label = value;
	}

	private void update_user() {
		Gtk.Label login = (Gtk.Label)this._builder.get_object("login-label");
		var value = this.item.get_attribute("user");
		if (value == null)
			value = "";
		login.label = value;
	}

	private void update_details() {
		var contents = new GLib.StringBuilder();
		var attrs = this.item.attributes;
		var iter = GLib.HashTableIter<string, string>(attrs);
		string key, value;
		while (iter.next(out key, out value)) {
			if (key.has_prefix("gkr:") || key.has_prefix("xdg:"))
				continue;
			contents.append_printf("<b>%s</b>: %s\n",
			                       GLib.Markup.escape_text(key),
			                       GLib.Markup.escape_text(value));
		}
		Gtk.Label details = (Gtk.Label)this._builder.get_object("details-box");
		details.use_markup = true;
		details.label = contents.str;
	}

	private void password_activate()
	{
		if (!this._password_expander.expanded)
			return;
		if (!this._password_changed)
			return;
		if (this._updating_password)
			return;

		this._updating_password = true;
		this._password_expander.sensitive = false;

		var value = new Secret.Value(this._password_entry.text, -1, "text/plain");
		this.item.set_secret.begin(value, null, (obj, res) => {
			try {
				this.item.set_secret.end(res);
				password_display();
			} catch (GLib.Error err) {
				DBusError.strip_remote_error(err);
				Util.show_error (this, _("Couldn't change password."), err.message);
			}

			this._password_expander.sensitive = true;
			this._updating_password = false;
		});
	}

	private void password_display() {
		if (this._password_expander.expanded) {
			var secret = this.item.get_secret();
			if (secret != null) {
				unowned string? password = secret.get_text();
				if (password != null) {
					this._password_entry.set_text(password);
					this._password_changed = false;
					return;
				}
			}
		}
		this._password_entry.set_text("");
		this._password_changed = false;
	}

	private void description_activate(Gtk.Entry description)
	{
		if (this._updating_description)
			return;

		this._updating_description = true;
		description.sensitive = false;

		this.item.set_label.begin(description.text, null, (obj, res) => {
			try {
				this.item.set_label.end(res);
			} catch (GLib.Error err) {
				description.text = this.item.label;
				DBusError.strip_remote_error(err);
				Util.show_error (this, _("Couldn't set description."), err.message);
			}

			description.sensitive = true;
			this._updating_description = false;
		});
	}

	private void expander_activate (Gtk.Expander expander)
	{
		if (!expander.expanded)
			return;

		/* Always have a hidden password when opening box */
		Gtk.CheckButton check = (Gtk.CheckButton)this._builder.get_object("show-password-check");
		check.set_active (false);

		/* Make sure to trigger retrieving the secret */
		password_display ();
	}
}

}
}
