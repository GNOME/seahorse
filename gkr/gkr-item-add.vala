/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

namespace Seahorse {
namespace Gkr {

public class ItemAdd : Gtk.Dialog {
	construct {
		this.title = _("Add Password");
		this.modal = true;
		this.window_position = Gtk.WindowPosition.CENTER_ON_PARENT;
		this.border_width = 5;

		var builder = Util.load_built_contents(this, "gkr-add-item");
		this.add_buttons(Gtk.Stock.OK, Gtk.ResponseType.ACCEPT,
		                 Gtk.Stock.CANCEL, Gtk.ResponseType.CANCEL);

		/* Load up a list of all the keyrings, and select the default */
		var combo = (Gtk.ComboBox)builder.get_object("item-keyring");
		var store = new Gtk.ListStore(2, typeof(string), typeof(Secret.Collection));
		combo.set_model(store);

		var cell = new Gtk.CellRendererText();
		combo.pack_start(cell, true);
		combo.add_attribute(cell, "text", 0);

		foreach (var keyring in Backend.instance().get_keyrings()) {
			Gtk.TreeIter iter;
			store.append(out iter);
			store.set(iter,
			          0, keyring.label,
			          1, keyring,
			          -1);
			if (keyring.is_default)
				combo.set_active_iter(iter);
		}

		var label = (Gtk.Entry)builder.get_object("item-label");
		this.set_response_sensitive(Gtk.ResponseType.ACCEPT, false);
		label.changed.connect((editable) => {
			var value = label.get_text();
			this.set_response_sensitive(Gtk.ResponseType.ACCEPT, value != "");
		});

		var area = (Gtk.Container)builder.get_object("password-area");
		var buffer = new Gcr.SecureEntryBuffer();
		var entry = new Gtk.Entry.with_buffer(buffer);
		entry.visibility = false;
		area.add(entry);
		entry.show();

		var check = (Gtk.ToggleButton)builder.get_object("show-password");
		check.toggled.connect(() => {
			entry.visibility = check.active;
		});

		this.response.connect((resp) => {
			if (resp != Gtk.ResponseType.ACCEPT) {
				this.destroy();
				return;
			}

			Gtk.TreeIter iter;
			if (!combo.get_active_iter(out iter))
				return;

			Secret.Collection collection;
			combo.model.get(iter, 1, out collection, -1);

			var secret = new Secret.Value(entry.get_text(), -1, "text/plain");
			var cancellable = Dialog.begin_request(this);
			var attributes = new GLib.HashTable<string, string>(GLib.str_hash, GLib.str_equal);

			/* TODO: Workaround for https://bugzilla.gnome.org/show_bug.cgi?id=697681 */
			var schema = new Secret.Schema("org.gnome.keyring.Note", Secret.SchemaFlags.NONE);

			Secret.Item.create.begin(collection, schema, attributes,
			                         label.get_text(), secret, Secret.ItemCreateFlags.NONE,
			                         cancellable, (obj, res) => {
				try {
					/* Clear the operation without cancelling it since it is complete */
					Dialog.complete_request(this, false);

					Secret.Item.create.end(res);
				} catch (GLib.Error err) {
					Util.show_error(this, _("Couldn’t add item"), err.message);
				}

				this.destroy();
			});

		});
	}

	public ItemAdd(Gtk.Window? parent) {
		GLib.Object(transient_for: parent);
		this.show();
		this.present();
	}
}

}
}
