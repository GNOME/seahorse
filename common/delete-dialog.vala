/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

namespace Seahorse {

public class DeleteDialog : Gtk.MessageDialog {
	private Gtk.ToggleButton _check;
	private bool _check_require;

	[Notify]
	public string? check_label {
		get {
			if (_check.get_visible())
				return _check.get_label();
			return null;
		}
		set {
			if (value == null) {
				_check.hide();
				value = "";
			} else {
				_check.show();
			}
			_check.set_label(value);
		}
	}

	[Notify]
	public bool check_value {
		get {
			if (_check.get_visible())
				return _check.get_active();
			return false;
		}
		set {
			_check.set_active(value);
		}
	}

	[Notify]
	public bool check_require {
		get {
			return _check_require;
		}
		set {
			_check_require = value;
			update_response_buttons();
		}
	}

	[CCode (type = "GtkDialog*")]
	public DeleteDialog(Gtk.Window? parent,
	                    string format,
	                    ...) {
		GLib.Object(
			message_type: Gtk.MessageType.QUESTION,
			transient_for: parent,
			text: format.vprintf(va_list())
		);
	}

	construct {
		set_modal(true);
		set_destroy_with_parent(true);

		_check = new Gtk.CheckButton();
		((Gtk.Container)get_message_area()).add(_check);
		_check.toggled.connect((toggle) => {
			update_response_buttons();
		});

		var cancel = new Gtk.Button.from_stock(Gtk.Stock.CANCEL);
		add_action_widget(cancel, Gtk.ResponseType.CANCEL);
		cancel.show();

		var delet = new Gtk.Button.from_stock(Gtk.Stock.DELETE);
		add_action_widget(delet, Gtk.ResponseType.OK);
		delet.show();
	}

	private void update_response_buttons() {
		set_response_sensitive(Gtk.ResponseType.OK,
		                       !_check_require || _check.get_active());
	}

	public static bool prompt (Gtk.Window? parent, string text) {
		Gtk.Dialog? dialog = new DeleteDialog(parent, "%s", text);
		var response = dialog.run();
		dialog.destroy();
		return (response == Gtk.ResponseType.OK);
	}

}

}
