/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2013 Red Hat Inc.
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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

namespace Seahorse {
namespace Pkcs11 {

public class PrivateKey : Gck.Object, Gck.ObjectCache,
                          Deletable, Exportable, Viewable {
	public Token? place {
		owned get { return (Token?)this._token.get(); }
		set { this._token.set(value); }
	}

	public Flags object_flags {
		get { return Flags.PERSONAL; }
	}

	public Gtk.ActionGroup? actions {
		get { return null; }
	}

	public Certificate? partner {
		owned get { return (Certificate?)this._certificate.get(); }
		set {
			this._certificate.set(value);
			notify_property("partner");
			notify_property("description");
		}
	}

	public string? label {
		owned get {
			if (this._attributes != null) {
				string label;
				if (this._attributes.find_string(CKA.LABEL, out label))
					return label;
			}
			Certificate? cert = this.partner;
			if (cert != null)
				return cert.label;
			return _("Unnamed private key");
		}
	}

	public string? markup {
		owned get { return GLib.Markup.escape_text(this.label, -1); }
	}

	public string? description {
		get { return _("Private key"); }
	}

	public GLib.Icon? icon {
		get {
			if (this._icon == null)
				this._icon = new GLib.ThemedIcon(Gcr.ICON_KEY);
			return this._icon;
		}
	}

	public Gck.Attributes attributes {
		owned get { return this._attributes; }
		set {
			this._attributes = value;
			notify_property("attributes");
		}
	}

	public bool deletable {
		get {
			Token ?token = this.place;
			return token == null ? false : token.is_deletable(this);
		}
	}

	public bool exportable {
		get { return false; }
	}

	private GLib.WeakRef _token;
	private Gck.Attributes? _attributes;
	private GLib.WeakRef _certificate;
	private GLib.Icon? _icon;

	public void fill(Gck.Attributes attributes) {
		Gck.Builder builder = new Gck.Builder(Gck.BuilderFlags.NONE);
		if (this._attributes != null)
			builder.add_all(this._attributes);
		builder.set_all(attributes);
		this._attributes = builder.steal();
		notify_property("attributes");
	}

	public Seahorse.Deleter create_deleter() {
		return new KeyDeleter(this);
	}

	public GLib.List<Exporter> create_exporters(ExporterType type) {
		/* In the future we may exporters here, but for now no exporting */
		var exporters = new GLib.List<Exporter>();
		return exporters;
	}

	public Gtk.Window? create_viewer(Gtk.Window? parent) {
		var viewer = new Pkcs11.Properties(this, parent);
		viewer.show();
		return viewer;
	}
}

}
}
