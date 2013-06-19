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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

namespace Seahorse {
	public const string ICON_KEY = "seahorse-key";
	public const string ICON_SECRET = "seahorse-key-personal";
	public const string ICON_KEY_SSH = "seahorse-key-ssh";
	public const string ICON_PERSON = "seahorse-person";
	public const string ICON_SIGN = "seahorse-sign";
	public const string ICON_SIGN_OK = "seahorse-sign-ok";
	public const string ICON_SIGN_BAD = "seahorse-sign-bad";
	public const string ICON_SIGN_UNKNOWN = "seahorse-sign-unknown";
	public const string ICON_WEBBROWSER = "web-browser";
	public const string ICON_FOLDER = "folder";
	public bool _icons_inited = false;

	public static void icons_init() {
		if (_icons_inited)
			return;
		_icons_inited = true;

		var path = GLib.Path.build_filename(Config.PKGDATADIR, "icons");
		Gtk.IconTheme.get_default().append_search_path (path);
		Gtk.Window.set_default_icon_name ("seahorse");
	}
}
