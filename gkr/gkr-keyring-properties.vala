/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

namespace Seahorse {
namespace Gkr {

public class KeyringProperties : Gtk.Dialog {
	public Keyring keyring { construct; get; }

	private Gtk.Builder _builder;

	construct {
		this._builder = new Gtk.Builder();
		try {
			string path = GLib.Path.build_filename(Config.UIDIR, "seahorse-gkr-keyring.xml");
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
		this.keyring.bind_property ("icon", this._builder.get_object("keyring-image"), "gicon",
		                            GLib.BindingFlags.SYNC_CREATE);

		/* The window title */
		this.keyring.bind_property ("label", this, "title", GLib.BindingFlags.SYNC_CREATE);

		/* Setup the label properly */
		var name = (Gtk.Label)this._builder.get_object("name-field");
		this.keyring.bind_property ("label", name, "label", GLib.BindingFlags.SYNC_CREATE);

		/* The date field */
		this.keyring.notify.connect((pspec) => {
			switch(pspec.name) {
			case "created":
				var created = (Gtk.Label)this._builder.get_object("created-field");
				created.label = Util.get_display_date_string((long)this.keyring.created);
				break;
			}
		});
	}

	public KeyringProperties(Keyring keyring,
	                         Gtk.Window? parent) {
		GLib.Object (
			keyring: keyring,
			transient_for: parent
		);
	}

}

}
}
