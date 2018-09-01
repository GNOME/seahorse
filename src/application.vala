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

    public Application () {
        GLib.Object (
            application_id: "org.gnome.seahorse.Application",
            flags: ApplicationFlags.HANDLES_COMMAND_LINE
        );
        this.search_provider = new SearchProvider(this);
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

    static bool show_version = false;
    const OptionEntry[] local_options = {
        { "version", 'v', 0, OptionArg.NONE, out show_version, N_("Version of this application"), null },
        { null }
    };

    public override bool local_command_line (ref weak string[] arguments, out int exit_status) {
        OptionContext context = new OptionContext(N_("- System Settings"));
        context.set_ignore_unknown_options(true);
        context.add_main_entries(local_options, Config.GETTEXT_PACKAGE);
        context.set_translation_domain(Config.GETTEXT_PACKAGE);
        context.add_group(Gtk.get_option_group (true));

        try {
            unowned string[] tmp = arguments;
            context.parse (ref tmp);
        } catch (Error e) {
            printerr ("seahorse: %s\n", e.message);
            exit_status = 1;
            return true;
        }

        if (show_version) {
            print ("%s\n", Config.PACKAGE_STRING);
#if WITH_PGP
            print ("GNUPG: %s (%d.%d.%d)\n", Config.GNUPG, Config.GPG_MAJOR, Config.GPG_MINOR, Config.GPG_MICRO);
#endif
            exit_status = 0;
            return true;
        }

        return base.local_command_line(ref arguments, out exit_status);
    }

    static bool no_window = false;
    const OptionEntry[] options = {
        { "no-window", 0, 0, OptionArg.NONE, out no_window, N_("Don't display a window"), null },
        { null }
    };

    public override int command_line (ApplicationCommandLine command_line) {
        OptionContext context = new OptionContext(N_("- System Settings"));
        context.set_ignore_unknown_options (true);
        context.add_main_entries (options, Config.GETTEXT_PACKAGE);
        context.set_translation_domain(Config.GETTEXT_PACKAGE);

        string[] arguments = command_line.get_arguments();
        try {
            unowned string[] tmp = arguments;
            context.parse (ref tmp);
        } catch (Error e) {
            printerr ("seahorse: %s\n", e.message);
            return 1;
        }

        if (no_window) {
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
