/*
 * Seahorse
 *
 * Copyright (C) 2018 Niels De Graef
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
 */

/* This is installed by gnome-keyring */
public class Seahorse.PgpSettings : GLib.Settings {
    public bool ascii_armor {
        get { return get_boolean("ascii-armor"); }
        set { set_boolean("ascii-armor", value); }
    }

    public string default_key {
        owned get { return get_string("default-key"); }
        set { set_string("default-key", value); }
    }

    public bool encrypt_to_self {
        get { return get_boolean("encrypt-to-self"); }
        set { set_boolean("encrypt-to-self", value); }
    }

    public string[] keyservers {
        owned get { return get_strv("keyservers"); }
        set { set_strv("keyservers", value); }
    }

    public string last_signer {
        owned get { return get_string("last-signer"); }
        set { set_string("last-signer", value); }
    }

    public string sort_recipients_by {
        owned get { return get_string("sort-recipients-by"); }
        set { set_string("sort-recipients-by", value); }
    }

	public PgpSettings () {
        GLib.Object (schema_id: "org.gnome.crypto.pgp");
	}

	private static PgpSettings? _instance = null;
	public static PgpSettings instance() {
		if (_instance == null)
			_instance = new PgpSettings();
		return _instance;
	}
}
