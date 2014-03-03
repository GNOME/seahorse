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

public class Deleter : Seahorse.Deleter {
	protected GLib.List<Gck.Object> objects;

	public override Gtk.Dialog create_confirm(Gtk.Window? parent) {
		var num = this.objects.length();
		if (num == 1) {
			string label;
			this.objects.data.get("label", out label);
			return new DeleteDialog(parent, _("Are you sure you want to permanently delete %s?"), label);
		} else {
			return new DeleteDialog(parent, GLib.ngettext ("Are you sure you want to permanently delete %d certificate?",
			                                               "Are you sure you want to permanently delete %d certificates?", num), num);
		}
	}

	public Deleter(Gck.Object object) {
		if (!add_object(object))
			GLib.assert_not_reached();
	}

	public override unowned GLib.List<weak GLib.Object> get_objects() {
		return this.objects;
	}

	public override bool add_object (GLib.Object obj) {
		if (obj is Certificate) {
			this.objects.append((Gck.Object)obj);
			return true;
		}
		return false;
	}

	public override async bool delete(GLib.Cancellable? cancellable) throws GLib.Error {
		var objects = this.objects.copy();
		foreach (var object in objects) {
			try {
				yield object.destroy_async(cancellable);

				Token? token;
				object.get("place", out token);
				if (token != null)
					token.remove_object(object);

			} catch (GLib.Error e) {
				/* Ignore objects that have gone away */
				if (e.domain != Gck.Error.get_quark() ||
				    e.code != CKR.OBJECT_HANDLE_INVALID)
					throw e;
			}
		}
		return true;
	}
}


}
}
