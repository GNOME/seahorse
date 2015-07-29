/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2013 Red Hat, Inc.
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

public class Certificate : Gck.Object, Gcr.Comparable, Gcr.Certificate,
                           Gck.ObjectCache, Deletable, Exportable, Viewable {
	public Token? place {
		owned get { return (Token?)this._token.get(); }
		set { this._token.set(value); }
	}

	public Flags object_flags {
		get { ensure_flags(); return this._flags; }
	}

	public Gtk.ActionGroup? actions {
		get { return null; }
	}

	public PrivateKey? partner {
		owned get { return (PrivateKey?)this._private_key.get(); }
		set {
			this._private_key.set(value);
			this._icon = null;
			this.notify_property("partner");
			this.notify_property("icon");
			this.notify_property("description");
		}
	}

	public Gck.Attributes attributes {
		owned get { return this._attributes; }
		set {
			this._attributes = value;
			this.notify_property("attributes");
		}
	}

	public bool deletable {
		get {
			var token = this.place;
			if (token == null)
				return false;
			return token.is_deletable(this);
		}
	}

	public bool exportable {
		get { return this._der != null; }
	}

	public GLib.Icon icon {
		owned get {
			if (this._icon != null)
				return this._icon;
			var icon = new GLib.ThemedIcon(Gcr.ICON_CERTIFICATE);
			if (this._private_key.get() != null) {
				var eicon = new GLib.ThemedIcon (Gcr.ICON_KEY);
				var emblem = new GLib.Emblem (eicon);
				this._icon = new GLib.EmblemedIcon (icon, emblem);
			} else {
				this._icon = icon;
			}
			return this._icon;
		}
	}

	public string description {
		owned get {
			ensure_flags ();
			if (this._private_key.get() != null)
				return _("Personal certificate and key");
			if ((this._flags & Flags.PERSONAL) == Flags.PERSONAL)
				return _("Personal certificate");
			else
				return _("Certificate");
		}
	}

	public string? label {
		owned get { return get_subject_name(); }
	}

	public string? subject {
		owned get { return get_subject_name(); }
	}

	public string? markup {
		owned get { return get_markup_text(); }
	}

	public string? issuer {
		owned get { return get_issuer_name(); }
	}

	public GLib.Date expiry {
		owned get { return get_expiry_date(); }
	}

	private GLib.WeakRef _token;
	private Gck.Attributes? _attributes;
	private unowned Gck.Attribute? _der;
	private GLib.WeakRef _private_key;
	private GLib.Icon? _icon;
	private Flags _flags;

	private static uint8[] EMPTY = { };

	construct {
		this._flags = (Flags)uint.MAX;
		this._der = null;
		this._private_key = GLib.WeakRef(null);
		this._token = GLib.WeakRef(null);

		this.notify.connect((pspec) => {
			if (pspec.name != "attributes")
				return;
			if (this._attributes != null)
				this._der = this._attributes.find(CKA.VALUE);
			notify_property ("label");
			notify_property ("markup");
			notify_property ("subject");
			notify_property ("issuer");
			notify_property ("expiry");
		});

		if (this._attributes != null)
			this._der = this._attributes.find(CKA.VALUE);
	}

	public override void dispose() {
		this.partner = null;
		base.dispose();
	}

	public Gtk.Window? create_viewer(Gtk.Window? parent) {
		var viewer = new Pkcs11.Properties(this, parent);
		viewer.show();
		return viewer;
	}

	public Seahorse.Deleter create_deleter() {
		Seahorse.Deleter deleter;

		PrivateKey? key = this.partner;
		if (key == null) {
			deleter = new Pkcs11.Deleter(this);
		} else {
			deleter = key.create_deleter();
			if (!deleter.add_object(this))
				GLib.return_val_if_reached(null);
		}

		return deleter;
	}

	public GLib.List<Exporter> create_exporters(ExporterType type) {
		var exporters = new GLib.List<Exporter>();

		if (this.exportable) {
			var exporter = new CertificateDerExporter(this);
			exporters.append(exporter);
		}

		return exporters;
	}

	public void fill(Gck.Attributes attributes) {
		Gck.Builder builder = new Gck.Builder(Gck.BuilderFlags.NONE);

		if (this._attributes != null)
			builder.add_all(this._attributes);
		builder.set_all(attributes);
		this._attributes = builder.steal();
		this.notify_property("attributes");
	}

	[CCode (array_length_type = "gsize")]
	public unowned uint8[] get_der_data() {
		if (this._der == null)
			return EMPTY;
		return this._der.get_data();
	}

	public int compare (Gcr.Comparable? other) {
		if (other == null)
			return -1;
		unowned uint8[] data1 = this.get_der_data();
		unowned uint8[] data2 = ((Gcr.Certificate)other).get_der_data();
		return Gcr.Comparable.memcmp(data1, data2);
	}

	private Flags calc_is_personal_and_trusted() {
		ulong category = 0;
		bool is_ca;

		/* If a matching private key, then this is personal*/
		if (this._private_key.get() != null)
			return Flags.PERSONAL | Flags.TRUSTED;

		if (this._attributes != null &&
		    this._attributes.find_ulong (CKA.CERTIFICATE_CATEGORY, out category)) {
			if (category == 2)
				return 0;
			else if (category == 1)
				return Flags.PERSONAL;
		}

		if (get_basic_constraints (out is_ca, null))
			return is_ca ? 0 : Flags.PERSONAL;

		return Flags.PERSONAL;
	}

	private void ensure_flags() {
		if (this._flags == uint.MAX)
			this._flags = Flags.EXPORTABLE | calc_is_personal_and_trusted ();
	}
}

}
}
