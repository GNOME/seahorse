/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

public class KeyringAdd : Gtk.Dialog {
	construct {
		this.title = _("Add Password Keyring");
		this.modal = true;
		this.window_position = Gtk.WindowPosition.CENTER_ON_PARENT;
		this.border_width = 5;

		var builder = Util.load_built_contents(this, "add-keyring");
		this.add_buttons(Gtk.Stock.OK, Gtk.ResponseType.ACCEPT,
		                 Gtk.Stock.CANCEL, Gtk.ResponseType.CANCEL);

		var entry = (Gtk.Entry)builder.get_object("keyring-name");
		this.set_response_sensitive(Gtk.ResponseType.ACCEPT, false);
		entry.changed.connect((editable) => {
			var name = entry.get_text();
			this.set_response_sensitive(Gtk.ResponseType.ACCEPT, name != "");
		});

		this.response.connect((resp) => {
			if (resp == Gtk.ResponseType.ACCEPT) {
				var name = entry.get_text();
				var cancellable = Dialog.begin_request(this);
				var service = Backend.instance().service;
				Secret.Collection.create.begin(service, name, null,
				                               Secret.CollectionCreateFlags.COLLECTION_CREATE_NONE,
				                               cancellable, (obj, res) => {
					/* Clear the operation without cancelling it since it is complete */
					Dialog.complete_request(this, false);

					try {
						Secret.Collection.create.end(res);
					} catch (GLib.Error err) {
						Util.show_error(this, _("Couldn't add keyring"), err.message);
					}

					this.destroy();
				});
			} else {
				this.destroy();
			}
		});

	}

	public KeyringAdd(Gtk.Window? parent) {
		GLib.Object(transient_for: parent);
		this.show();
		this.present();
	}
}

}
}
