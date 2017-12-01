/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2017 Niels De Graef
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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

public class Seahorse.Pgp.BackendActions : Seahorse.Actions {

    private const Gtk.ActionEntry FIND_ACTIONS[] = {
        { "remote-find", Gtk.Stock.FIND, _("_Find Remote Keys…"), "",
              _("Search for keys on a key server"), on_remote_find },
    };

    private const Gtk.ActionEntry SYNC_ACTIONS[] = {
        { "remote-sync", Gtk.Stock.REFRESH, _("_Sync and Publish Keys…"), "",
              _("Publish and/or synchronize our keys with those online."), on_remote_sync }
    };

#if WITH_KEYSERVER

    public const string BACKEND_DEFINITION = """
      <ui>
        <menubar>
          <placeholder name="RemoteMenu">
            <menu name="Remote" action="remote-menu">
              <menuitem action="remote-find"/>
              <menuitem action="remote-sync"/>
            </menu>
          </placeholder>
        </menubar>
      </ui>""";

    private void on_remote_find(Gtk.Action action) {
        seahorse_keyserver_search_show (seahorse_action_get_window (action));
    }

    private void on_remote_sync (Gtk.Action action) {
        SeahorseGpgmeKeyring *keyring;
        SeahorseCatalog *catalog;
        GList *objects = null;
        GList *keys = null;
        GList *l;

        catalog = seahorse_actions_get_catalog (actions);
        if (catalog != null) {
            objects = seahorse_catalog_get_selected_objects (catalog);
            for (l = objects; l != null; l = g_list_next (l)) {
                if (SEAHORSE_IS_PGP_KEY (l->data))
                    keys = g_list_prepend (keys, l->data);
            }
            g_list_free (objects);
        }
        g_object_unref (catalog);

        if (keys == null) {
            keyring = seahorse_pgp_backend_get_default_keyring (null);
            keys = gcr_collection_get_objects (GCR_COLLECTION (keyring));
        }

        seahorse_keyserver_sync_show (keys, seahorse_action_get_window (action));
        g_list_free (keys);
    }


#endif

    construct {
        set_translation_domain(Config.GETTEXT_PACKAGE);
        add_actions(FIND_ACTIONS, this);
        add_actions(SYNC_ACTIONS, this);
        register_definition(BACKEND_DEFINITION);
    }

    private static GtkActionGroup? actions = null;
    public static Gtk.ActionGroup instance() {
        if (actions == null) {
            actions = g_object_new (SEAHORSE_TYPE_PGP_BACKEND_ACTIONS,
                                    "name", "pgp-backend",
                                    null);
            g_object_add_weak_pointer (G_OBJECT (actions),
                                       (gpointer *)&actions);
        }

        return actions;
    }
}


public class Seahorse.Gpgme.KeyActions : Seahorse.Actions {

    private void
    seahorse_gpgme_key_actions_init (SeahorseGpgmeKeyActions *self)
    {
#if WITH_KEYSERVER
        GtkActionGroup *actions = GTK_ACTION_GROUP (self);
        gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
        gtk_action_group_add_actions (actions, SYNC_ACTIONS,
                                      G_N_ELEMENTS (SYNC_ACTIONS), null);
#endif
    }

    private static GtkActionGroup? actions = null;
    public static GtkActionGroup instance() {
        if (actions == null) {
            actions = g_object_new (SEAHORSE_TYPE_GPGME_KEY_ACTIONS,
                                    "name", "gpgme-key",
                                    null);
            g_object_add_weak_pointer (G_OBJECT (actions),
                                       (gpointer *)&actions);
        }

        return actions;
    }
}
