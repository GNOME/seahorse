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

public class Seahorse.Application : Adw.Application {

    private SearchProvider? search_provider;
    private uint search_provider_dbus_id = 0;

    private KeyManager? key_mgr = null;

    private const string[] DEVELOPERS = {
        "Niels De Graef <nielsdegraef@gmail.com>",
        "Jacob Perkins <jap1@users.sourceforge.net>",
        "Jose Carlos Garcia Sogo <jsogo@users.sourceforge.net>",
        "Jean Schurger <yshark@schurger.org>",
        "Stef Walter <stef@memberwebs.com>",
        "Adam Schreiber <sadam@clemson.edu>",
        "Albrecht Dreß <albrecht.dress@arcor.de>",
        "Jim Pharis <binbrain@gmail.com>",
        null
    };

    private const string[] DESIGNERS = {
        "Jacob Perkins <jap1@users.sourceforge.net>",
        "Stef Walter <stef@memberwebs.com>",
        null
    };

    private const GLib.ActionEntry[] action_entries = {
         { "quit",            quit                  },
         { "help",            on_app_help           },
         { "about",           on_app_about          },
         { "preferences",     on_app_preferences    },
    };

    const OptionEntry[] cmd_options = {
        { "version", 'v', 0, OptionArg.NONE, null, N_("Version of this application"), null },
        { null }
    };

    public Application() {
        GLib.Object (
            application_id: Config.APPLICATION_ID,
            resource_base_path: "/org/gnome/Seahorse",
            flags: ApplicationFlags.HANDLES_OPEN
        );
        this.search_provider = new SearchProvider(this);

        add_action_entries(action_entries, this);
        var pref_action = lookup_action("preferences") as SimpleAction;
        pref_action.set_enabled(PrefsDialog.available());
        add_action_accelerators();

        add_main_option_entries(cmd_options);
    }

    private void add_action_accelerators() {
        set_accels_for_action ("app.help",        {"F1"});
        set_accels_for_action ("app.preferences", {"<control>comma"});
        set_accels_for_action ("app.quit",        {"<control>q"});

        set_accels_for_action ("win.show-search", { "<control>f" });
        set_accels_for_action ("win.new-item",    { "<control>n" });
        set_accels_for_action ("win.import-file", { "<control>i" });
        set_accels_for_action ("win.copy",        { "<control>c" });
        set_accels_for_action ("win.paste",       { "<control>v" });
    }

    public override void startup() {
        base.startup();

        Environment.set_prgname(Config.APPLICATION_ID);
        Environment.set_application_name(_("Passwords and Keys"));

        // Initialize the backends
        Gkr.Backend.initialize();
        Ssh.Backend.initialize();
#if WITH_PGP
        Pgp.Backend.initialize(null);
#endif
#if WITH_PKCS11
        Pkcs11.Backend.initialize();
#endif
    }

    public override void activate() {
        if (get_active_window() == null)
            this.key_mgr = new Seahorse.KeyManager(this);

        this.key_mgr.present();
    }

    public override void open(File[] files, string hint) {
        activate();
        return_if_fail(this.key_mgr != null);

        // We only support opening 1 file
        this.key_mgr.import_file.begin(files[0]);
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

    private void on_app_about(SimpleAction action, Variant? param) {
        var dialog = new Adw.AboutDialog() {
            application_name = _("Passwords and Keys"),
            developers = DEVELOPERS,
            designers = DESIGNERS,
            developer_name = _("The GNOME Project"),
            website = "https://wiki.gnome.org/Apps/Seahorse",
            issue_url = "https://gitlab.gnome.org/GNOME/seahorse/-/issues/new",
            version = Config.VERSION,
            application_icon = Config.APPLICATION_ID,
            copyright = "© Seahorse contributors",
            license_type = Gtk.License.GPL_2_0,
        };
        dialog.present(this.key_mgr);
    }

    private void on_app_help(SimpleAction action, Variant? param) {
        var help_launcher = new Gtk.UriLauncher("help:seahorse");
        help_launcher.launch.begin(this.key_mgr, null, (obj, res) => {
            try {
                help_launcher.launch.end(res);
            } catch (GLib.Error err) {
                warning("Error showing help: %s", err.message);
            }
        });
    }

    private void on_app_preferences(SimpleAction action, Variant? param) {
        var prefs_dialog = new PrefsDialog();
        prefs_dialog.present(this.key_mgr);
    }
}
