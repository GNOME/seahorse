/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

#include "config.h"

#include "seahorse-keyserver-sync.h"

#include "seahorse-pgp-backend.h"
#include "seahorse-transfer.h"

#include "seahorse-common.h"

#include "libseahorse/seahorse-progress.h"
#include "libseahorse/seahorse-util.h"

#include <glib/gi18n.h>

struct _SeahorseKeyserverSync {
    GtkDialog parent_instance;

    GList *keys;

    GtkWidget *detail_message;
    GtkWidget *publish_message;
    GtkWidget *sync_message;
    GtkWidget *sync_button;
};

enum {
    PROP_0,
    PROP_KEYS,
    N_PROPS
};

G_DEFINE_TYPE (SeahorseKeyserverSync, seahorse_keyserver_sync, GTK_TYPE_DIALOG)

static void
on_transfer_upload_complete (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
    SeahorsePgpBackend *backend = SEAHORSE_PGP_BACKEND (object);
    SeahorseAppSettings *app_settings;
    GError *error = NULL;
    g_autofree char *publish_to = NULL;

    if (seahorse_pgp_backend_transfer_finish (backend, result, &error))
        return;

    app_settings = seahorse_app_settings_instance ();
    publish_to = seahorse_app_settings_get_server_publish_to (app_settings);
    seahorse_util_handle_error (&error, NULL,
                                _("Couldn’t publish keys to server"), publish_to);
}

static void
on_transfer_download_complete (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
    g_autoptr(SeahorseServerSource) ssrc = SEAHORSE_SERVER_SOURCE (user_data);
    GError *error = NULL;

    if (!seahorse_transfer_finish (result, &error)) {
        g_autofree char *uri = NULL;

        uri = seahorse_place_get_uri (SEAHORSE_PLACE (ssrc));
        seahorse_util_handle_error (&error, NULL,
                                    _("Couldn’t retrieve keys from server: %s"),
                                    uri);
    }
}

static void
update_message (SeahorseKeyserverSync *self)
{
    SeahorseAppSettings *app_settings;
    g_autofree char *text = NULL;
    gboolean should_publish;

    app_settings = seahorse_app_settings_instance ();
    text = seahorse_app_settings_get_server_publish_to (app_settings);

    should_publish = (text && *text);
    gtk_widget_set_visible (self->publish_message, should_publish);
    gtk_widget_set_visible (self->sync_message, !should_publish);
    gtk_widget_set_sensitive (self->sync_button, should_publish);
}

static void
on_settings_publish_to_changed (GSettings  *settings,
                                const char *key,
                                gpointer    user_data)
{
    SeahorseKeyserverSync *self = SEAHORSE_KEYSERVER_SYNC (user_data);

    update_message (self);
}

static void
on_sync_ok_clicked (GtkButton *button, gpointer user_data)
{
    SeahorseKeyserverSync *self = SEAHORSE_KEYSERVER_SYNC (user_data);
    g_autoptr(GList) keys = NULL;

    keys = g_list_copy (self->keys);
    seahorse_keyserver_sync_do_sync (keys);
}

static void
on_sync_configure_clicked (GtkButton *button, gpointer user_data)
{
    SeahorseKeyserverSync *self = SEAHORSE_KEYSERVER_SYNC (user_data);
    SeahorsePrefs *prefs;

    prefs = seahorse_prefs_new (GTK_WINDOW (self));
    gtk_window_present (GTK_WINDOW (prefs));
}

static void
seahorse_keyserver_sync_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
    SeahorseKeyserverSync *self = SEAHORSE_KEYSERVER_SYNC (object);

    switch (prop_id) {
    case PROP_KEYS:
        g_value_set_pointer (value, self->keys);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_keyserver_sync_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
    SeahorseKeyserverSync *self = SEAHORSE_KEYSERVER_SYNC (object);

    switch (prop_id) {
    case PROP_KEYS:
        g_list_free (self->keys);
        self->keys = g_list_copy (g_value_get_pointer (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_keyserver_sync_finalize (GObject *obj)
{
    SeahorseKeyserverSync *self = SEAHORSE_KEYSERVER_SYNC (obj);

    g_clear_pointer (&self->keys, g_list_free);

    G_OBJECT_CLASS (seahorse_keyserver_sync_parent_class)->finalize (obj);
}

static void
seahorse_keyserver_sync_constructed (GObject *obj)
{
    SeahorseKeyserverSync *self = SEAHORSE_KEYSERVER_SYNC (obj);
    SeahorseAppSettings *app_settings;
    unsigned int n_keys;
    g_autofree char *t = NULL;

    G_OBJECT_CLASS (seahorse_keyserver_sync_parent_class)->constructed (obj);

    /* The details message */
    n_keys = g_list_length (self->keys);
    t = g_strdup_printf (ngettext ("<b>%d key is selected for synchronizing</b>",
                                   "<b>%d keys are selected for synchronizing</b>",
                                   n_keys),
                         n_keys);
    gtk_label_set_markup (GTK_LABEL (self->detail_message), t);

    /* The right help message */
    app_settings = seahorse_app_settings_instance ();
    g_signal_connect_object (app_settings, "changed::server-publish-to",
                             G_CALLBACK (on_settings_publish_to_changed),
                             self, 0);
    update_message (self);
}

static void
seahorse_keyserver_sync_init (SeahorseKeyserverSync *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}

static void
seahorse_keyserver_sync_class_init (SeahorseKeyserverSyncClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->constructed = seahorse_keyserver_sync_constructed;
    gobject_class->get_property = seahorse_keyserver_sync_get_property;
    gobject_class->set_property = seahorse_keyserver_sync_set_property;
    gobject_class->finalize = seahorse_keyserver_sync_finalize;

    g_object_class_install_property (gobject_class, PROP_KEYS,
        g_param_spec_pointer ("keys", "Keys",
                              "A GList of keys which should be synced",
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Seahorse/seahorse-keyserver-sync.ui");

    gtk_widget_class_bind_template_child (widget_class, SeahorseKeyserverSync, detail_message);
    gtk_widget_class_bind_template_child (widget_class, SeahorseKeyserverSync, publish_message);
    gtk_widget_class_bind_template_child (widget_class, SeahorseKeyserverSync, sync_message);
    gtk_widget_class_bind_template_child (widget_class, SeahorseKeyserverSync, sync_button);

    gtk_widget_class_bind_template_callback (widget_class, on_sync_ok_clicked);
    gtk_widget_class_bind_template_callback (widget_class, on_sync_configure_clicked);
}

/**
 * seahorse_keyserver_sync_do_sync:
 * @keys: (element-type SeahorsePgpKey): List of keys that need to be synced
 *
 * Non-interactively synchronizes the given @keys with the chosen keyserver.
 */
void
seahorse_keyserver_sync_do_sync (GList *keys)
{
    SeahorseServerSource *source;
    SeahorseGpgmeKeyring *keyring;
    SeahorseAppSettings *app_settings;
    SeahorsePgpSettings *pgp_settings;
    g_autofree char *keyserver = NULL;
    g_autoptr(GCancellable) cancellable = NULL;
    g_auto(GStrv) keyservers = NULL;
    g_autoptr(GPtrArray) keyids = NULL;

    if (!keys)
        return;

    keyring = seahorse_pgp_backend_get_default_keyring (NULL);
    pgp_settings = seahorse_pgp_settings_instance ();
    cancellable = g_cancellable_new ();

    keyids = g_ptr_array_new ();
    for (GList *l = keys; l != NULL; l = g_list_next (l))
        g_ptr_array_add (keyids, (char *) seahorse_pgp_key_get_keyid (l->data));
    g_ptr_array_add (keyids, NULL);

    /* And now synchronizing keys from the servers */
    keyservers = seahorse_pgp_settings_get_uris (pgp_settings);
    for (guint i = 0; keyservers[i] != NULL; i++) {
        source = seahorse_pgp_backend_lookup_remote (NULL, keyservers[i]);

        /* This can happen if the URI scheme is not supported */
        if (source == NULL)
            continue;

        seahorse_transfer_keyids_async (SEAHORSE_SERVER_SOURCE (source),
                                        SEAHORSE_PLACE (keyring),
                                        (const char **) keyids->pdata,
                                        cancellable,
                                        on_transfer_download_complete,
                                        g_object_ref (source));
    }

    /* Publishing keys online */
    app_settings = seahorse_app_settings_instance ();
    keyserver = seahorse_app_settings_get_server_publish_to (app_settings);
    if (keyserver && keyserver[0]) {
        source = seahorse_pgp_backend_lookup_remote (NULL, keyserver);

        /* This can happen if the URI scheme is not supported */
        if (source != NULL) {
            seahorse_pgp_backend_transfer_async (NULL, keys, SEAHORSE_PLACE (source),
                                                 cancellable, on_transfer_upload_complete,
                                                 g_object_ref (source));
        }
    }

    seahorse_progress_show (cancellable, _("Synchronizing keys…"), FALSE);
}

SeahorseKeyserverSync *
seahorse_keyserver_sync_new (GList     *keys,
                             GtkWindow *parent)
{
    g_return_val_if_fail (keys, NULL);
    g_return_val_if_fail (keys->data, NULL);
    g_return_val_if_fail (!parent || GTK_IS_WINDOW (parent), NULL);

    return g_object_new (SEAHORSE_TYPE_KEYSERVER_SYNC,
                         "keys", keys,
                         "transient-for", parent,
                         "use-header-bar", 1,
                         NULL);
}
