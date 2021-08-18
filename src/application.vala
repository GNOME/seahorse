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

    private KeyManager? key_mgr = null;

    private const string[] AUTHORS = {
        "Jacob Perkins <jap1@users.sourceforge.net>",
        "Jose Carlos Garcia Sogo <jsogo@users.sourceforge.net>",
        "Jean Schurger <yshark@schurger.org>",
        "Stef Walter <stef@memberwebs.com>",
        "Adam Schreiber <sadam@clemson.edu>",
        "Niels De Graef <nielsdegraef@gmail.com>",
        "",
        N_("Contributions:"),
        "Albrecht Dreß <albrecht.dress@arcor.de>",
        "Jim Pharis <binbrain@gmail.com>",
        null
    };

    private const string[] DOCUMENTERS = {
        "Jacob Perkins <jap1@users.sourceforge.net>",
        "Adam Schreiber <sadam@clemson.edu>",
        "Milo Casagrande <milo_casagrande@yahoo.it>",
        null
    };

    private const string[] ARTISTS = {
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
        pref_action.set_enabled(Prefs.available());
        add_action_accelerators();

        add_main_option_entries(cmd_options);
    }

    private void add_action_accelerators() {
        set_accels_for_action ("app.help",        {"F1"});
        set_accels_for_action ("app.quit",        {"<control>q"});

        set_accels_for_action ("win.show-search", { "<control>f" });
        set_accels_for_action ("win.new-item",    { "<control>n" });
        set_accels_for_action ("win.import-file", { "<control>i" });
        set_accels_for_action ("win.copy",        { "<control>c" });
        set_accels_for_action ("win.paste",       { "<control>v" });
    }

    public override void startup() {
        base.startup();

        Hdy.init();

        // Insert Icons into Stock
        icons_init();

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

        string[] uris = {};
        foreach (File file in files)
            uris += file.get_uri();
        this.key_mgr.import_files(uris);
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
        var about = new Gtk.AboutDialog();
        about.set_artists(ARTISTS);
        about.set_authors(AUTHORS);
        about.set_documenters(DOCUMENTERS);
        about.set_version(Config.VERSION);
        about.set_comments(_("Passwords and Keys"));
        about.set_copyright("© 2002 - 2018 Seahorse Contributors");
        about.set_translator_credits(_("translator-credits"));
        about.set_logo_icon_name(Config.APPLICATION_ID);
        about.set_website("https://wiki.gnome.org/Apps/Seahorse");
        about.set_website_label(_("Seahorse Project Homepage"));

        about.response.connect((response) => {
            about.hide();
        });

        about.set_transient_for(this.key_mgr);
        about.run();
        about.destroy();
    }

    private void on_app_help(SimpleAction action, Variant? param) {
        try {
          Gtk.show_uri_on_window(this.key_mgr, "help:seahorse", Gtk.get_current_event_time ());
        } catch (GLib.Error err) {
          warning("Error showing help: %s", err.message);
        }
    }

    private void on_app_preferences(SimpleAction action, Variant? param) {
        Prefs prefs_dialog = new Prefs(this.key_mgr);
        prefs_dialog.present();
    }
}
