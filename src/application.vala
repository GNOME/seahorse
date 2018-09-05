/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2005, 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2012 Stefan Walter
 * Copyright (C) 2018 Niels De Graef
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

public class Seahorse.Application : Gtk.Application {
    private SearchProvider? search_provider;
    private uint search_provider_dbus_id = 0;

    private const int INACTIVITY_TIMEOUT = 120 * 1000; // Two minutes, in milliseconds

    const OptionEntry[] cmd_options = {
        { "version", 'v', 0, OptionArg.NONE, null, N_("Version of this application"), null },
        { "no-window", 0, 0, OptionArg.NONE, null, N_("Don't display a window"), null },
        { null }
    };

    public Application () {
        GLib.Object (
            application_id: "org.gnome.seahorse.Application",
            flags: ApplicationFlags.HANDLES_COMMAND_LINE
        );
        this.search_provider = new SearchProvider(this);
        add_main_option_entries(cmd_options);
    }

    public override void startup() {
        base.startup();

        // Insert Icons into Stock
        icons_init ();

        // Initialize the backends
        Gkr.Backend.initialize();
        Ssh.Backend.initialize();
#if WITH_PGP
        Pgp.Backend.initialize();
#endif
#if WITH_PKCS11
        Pkcs11.Backend.initialize();
#endif
    }

    public override void activate() {
        var key_mgr = get_active_window();

        if (key_mgr == null)
            key_mgr = new Seahorse.KeyManager(this);

        key_mgr.present();
    }

    public override int handle_local_options (VariantDict options) {
        if (options.contains("version")) {
            print ("%s\n", Config.PACKAGE_STRING);
#if WITH_PGP
            print ("GNUPG: %s (%d.%d.%d)\n", Config.GNUPG, Config.GPG_MAJOR, Config.GPG_MINOR, Config.GPG_MICRO);
#endif
            return 0;
        }

        return -1;
    }

    public override int command_line (ApplicationCommandLine command_line) {
        VariantDict options;

        options = command_line.get_options_dict();
        if (options.contains("no-window")) {
            hold();
            set_inactivity_timeout(INACTIVITY_TIMEOUT);
            release();
            return 0;
        }

        activate ();
        return 0;
    }

    public override bool dbus_register (DBusConnection connection, string object_path) throws Error {
        if (!base.dbus_register(connection, object_path))
            return false;

        this.search_provider_dbus_id = connection.register_object(object_path, this.search_provider);
        return true;
    }

    public override void dbus_unregister (DBusConnection connection, string object_path) {
        if (this.search_provider_dbus_id != 0) {
            connection.unregister_object(this.search_provider_dbus_id);
            this.search_provider_dbus_id = 0;
        }

        base.dbus_unregister(connection, object_path);
    }

    public void initialize_search () {
        this.search_provider.load.begin();
    }
}
