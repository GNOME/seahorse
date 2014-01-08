/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
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
	public const string ICON_PASSWORD = "gcr-password";
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
