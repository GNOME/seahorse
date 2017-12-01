/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2017 Niels De Graef
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

namespace Seahorse {

public class KeyserverSync : Gtk.Dialog {

    static void on_transfer_upload_complete(GObject object, AsyncResult *result, Seahorse.Place place) {
        GError *error = null;

        if (!seahorse_pgp_backend_transfer_finish (SEAHORSE_PGP_BACKEND (object), result, &error)) {
            string publish_to = Seahorse.Application.settings().get_string("server-publish-to");
            seahorse_util_handle_error (&error, null,
                                        _("Couldn’t publish keys to server"), publish_to);
        }
    }

    static void on_transfer_download_complete (GObject *object, AsyncResult *result, Seahorse.Place place) {
        GError *error = null;
        if (!seahorse_transfer_finish (result, &error)) {
            seahorse_util_handle_error (&error, null,
                                        _("Couldn’t retrieve keys from server: %s"), place.key_server);
        }
    }

    G_MODULE_EXPORT void on_sync_ok_clicked (GtkButton *button) {
        GList *keys;
        keys = (GList*)g_object_get_data (G_OBJECT (swidget), "publish-keys");
        keys = g_list_copy (keys);

        destroy();

        seahorse_keyserver_sync (keys);
    }

    G_MODULE_EXPORT void on_sync_configure_clicked (GtkButton *button) {
        seahorse_prefs_show (GTK_WINDOW (seahorse_widget_get_widget (swidget, swidget->name)), "keyserver-tab");
    }

    static void update_message() {
        Gtk.Widget widget = seahorse_widget_get_widget (swidget, "publish-message");
        Gtk.Widget widget2 = seahorse_widget_get_widget (swidget, "sync-message");
        Gtk.Widget sync_button = seahorse_widget_get_widget (swidget, "sync-button");

        string text = Seahorse.Application.settings().get_string("server-publish-to");
        bool filled_in = (text != null && text != "");
        widget.sensitive = filled_in;
        widget2.sensitive = !filled_in;
        sync_button.sensitive = filled_in;
    }

    static void on_settings_publish_to_changed(GSettings *settings, string key) {
        update_message();
    }

    /**
     * Shows a synchronize window.
     *
     * @keys: The keys to synchronize
     **/
    public void show(List<Pgp.Key> keys, GtkWindow *parent) {
        SeahorseWidget *swidget;
        GtkWindow *win;
        GtkWidget *w;
        guint n;

        swidget = seahorse_widget_new_allow_multiple ("keyserver-sync", parent);
        g_return_val_if_fail (swidget != null, null);

        /* The details message */
        string t = ngettext("<b>%d key is selected for synchronizing</b>",
                            "<b>%d keys are selected for synchronizing</b>", keys.length), keys.length);

        Gtk.Label detail_message = GTK_WIDGET (seahorse_widget_get_widget (swidget, "detail-message"));
        detail_message.set_markup(t);

        // The right help message
        update_message();
        g_signal_connect_object (seahorse_application_settings (null), "changed::server-publish-to",
                                 G_CALLBACK (on_settings_publish_to_changed), swidget, 0);

        keys = g_list_copy (keys);
        g_return_val_if_fail (!keys || SEAHORSE_IS_OBJECT (keys->data), win);
        g_object_set_data_full (G_OBJECT (swidget), "publish-keys", keys,
                                (GDestroyNotify)g_list_free);
    }

    void seahorse_keyserver_sync(List<Pgp.Key> keys) {
        if (keys == null)
            return;

        Cancellable cancellable = new Cancellable();

        string[] keyids = new string[keyids.length];
        foreach (Pgp.Key key in keys)
            keyids += key.keyid;

        // And now synchronizing keys from the servers
        string[] keyservers = seahorse_servers_get_uris ();
        foreach (string keyserver in keyservers) {
            ServerSource? source = Pgp.Backend.get().lookup_remote(keyserver);

            if (source == null) // This can happen if the URI scheme is not supported
                continue;

            GpgME.Keyring keyring = Pgp.Backend.get_default_keyring (null);
            Seahorse.Transfer.transfer_keyids_async(source, keyring, keyids, cancellable,
                                                    on_transfer_download_complete, source);
        }

        /* Publishing keys online */
        string? keyserver = Seahorse.Application.settings().get_string("server-publish-to");
        if (keyserver != null && keyserver != "") {
            ServerSource? source = Pgp.Backend.get().lookup_remote(keyserver);

            if (source == null) // This can happen if the URI scheme is not supported
                continue;

            Pgp.Backend.transfer_async(null, keys, source, cancellable,
                                       on_transfer_upload_complete, source);
        }

        Seahorse.Progress.show(cancellable, _("Synchronizing keys…"), false);
    }
}

}
