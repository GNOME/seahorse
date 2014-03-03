/*
 * Seahorse
 *
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
 *
 * Author: Stef Walter <stefw@redhat.com>
 */

namespace Seahorse {
namespace Pkcs11 {

class KeyDeleter : Deleter {
	private Certificate? _cert;
	private PrivateKey? _key;
	private string? _label;

	public override Gtk.Dialog create_confirm(Gtk.Window? parent) {
		var dialog = new DeleteDialog(parent, _("Are you sure you want to permanently delete %s?"), this._label);
		dialog.check_label = _("I understand that this key will be permanently deleted.");
		dialog.check_require = true;
		return dialog;
	}

	public KeyDeleter(Gck.Object cert_or_key) {
		base(cert_or_key);
	}

	public override bool add_object (GLib.Object obj) {
		GLib.Object? partner;

		if (obj is PrivateKey) {
			if (this._key != null)
				return false;
			if (this._cert != null) {
				partner = this._cert.partner;
				if (partner != obj)
					return false;
			}
			this._key = (PrivateKey)obj;
			this.objects.prepend(this._key);

		} else if (obj is Certificate) {
			if (this._cert != null)
				return false;
			if (this._key != null) {
				partner = this._key.partner;
				if (partner != obj)
					return false;
			}
			this._cert = (Certificate)obj;
			this.objects.prepend(this._cert);
		} else {
			return false;
		}

		if (this._label == null)
			obj.get("label", out this._label);
		return true;
	}
}

}
}
