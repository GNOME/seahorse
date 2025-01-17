/*
 * Seahorse
 *
 * Copyright (C) 2004-2005 Stefan Walter
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

#include "seahorse-keyserver-search.h"
#include "seahorse-pgp-backend.h"
#include "seahorse-gpgme-keyring.h"

#include <glib/gi18n.h>

#include "seahorse-common.h"


/**
 * SeahorseKeyserverRow:
 */

#define SEAHORSE_TYPE_KEYSERVER_ROW (seahorse_keyserver_row_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseKeyserverRow, seahorse_keyserver_row,
                      SEAHORSE, KEYSERVER_ROW,
                      AdwActionRow)

struct _SeahorseKeyserverRow {
    AdwActionRow parent;

    SeahorseServerSource *keyserver;
    GtkWidget *check_image;
};

G_DEFINE_TYPE (SeahorseKeyserverRow, seahorse_keyserver_row, ADW_TYPE_ACTION_ROW);

static void
seahorse_keyserver_row_init (SeahorseKeyserverRow *row)
{
}

static void
seahorse_keyserver_row_class_init (SeahorseKeyserverRowClass *klass)
{
}

static GtkWidget *
seahorse_keyserver_row_new (SeahorseServerSource *ssrc,
                            GPtrArray *selected_servers)
{
    g_autoptr(SeahorseKeyserverRow) row = NULL;
    g_autofree char *uri = NULL;
    gboolean is_selected;

    row = g_object_new (SEAHORSE_TYPE_KEYSERVER_ROW, NULL);
    gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);

    uri = seahorse_place_get_uri (SEAHORSE_PLACE (ssrc));
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), uri);

    row->check_image = gtk_image_new_from_icon_name ("emblem-ok-symbolic");
    is_selected = g_ptr_array_find (selected_servers, ssrc, NULL);
    gtk_widget_set_visible (row->check_image, is_selected);
    adw_action_row_add_suffix (ADW_ACTION_ROW (row), row->check_image);

    return GTK_WIDGET (g_steal_pointer (&row));
}


/**
 * SeahorseFoundKeyRow:
 */

#define SEAHORSE_TYPE_FOUND_KEY_ROW (seahorse_found_key_row_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseFoundKeyRow, seahorse_found_key_row,
                      SEAHORSE, FOUND_KEY_ROW,
                      AdwActionRow)

struct _SeahorseFoundKeyRow {
    AdwActionRow parent;

    SeahorsePgpKey *key;
    GtkButton *import_button;
};

G_DEFINE_TYPE (SeahorseFoundKeyRow, seahorse_found_key_row, ADW_TYPE_ACTION_ROW);

static void
on_import_complete (GObject *source, GAsyncResult *result, void *user_data)
{
    SeahorsePgpBackend *backend = SEAHORSE_PGP_BACKEND (source);
    g_autoptr(SeahorseFoundKeyRow) row =
        SEAHORSE_FOUND_KEY_ROW (user_data);
    const char *result_icon_name;
    g_autofree char *result_tooltip = NULL;
    g_autoptr(GError) err = NULL;

    if (!seahorse_pgp_backend_transfer_finish (backend, result, &err)) {
        result_icon_name = "dialog-warning-symbolic";
        result_tooltip = g_strdup_printf (_("Couldn’t import key: %s"),
                                          err->message);
    } else {
        result_icon_name = "emblem-ok-symbolic";
        result_tooltip = g_strdup (_("Key import succeeded"));
    }

    gtk_button_set_icon_name (row->import_button, result_icon_name);
    gtk_widget_set_tooltip_text (GTK_WIDGET (row->import_button),
                                 result_tooltip);
}

static void
on_import_button_clicked (GtkButton *import_button, void *user_data)
{
    SeahorseFoundKeyRow *row = user_data;
    g_autoptr(GtkWidget) spinner = NULL;
    g_autoptr(GListStore) keys = NULL;
    g_autoptr(GCancellable) cancellable = NULL;
    SeahorsePgpBackend *backend;
    SeahorseGpgmeKeyring *keyring;

    /* Let the button show a spinner while importing */
    gtk_widget_set_sensitive (GTK_WIDGET (import_button), FALSE);
    spinner = gtk_spinner_new ();
    gtk_spinner_start (GTK_SPINNER (spinner));
    gtk_button_set_child (import_button, g_steal_pointer (&spinner));

    /* Now import the key */
    keys = g_list_store_new (SEAHORSE_PGP_TYPE_KEY);
    g_list_store_append (keys, row->key);
    cancellable = g_cancellable_new ();
    backend = seahorse_pgp_backend_get ();
    keyring = seahorse_pgp_backend_get_default_keyring (backend);
    seahorse_pgp_backend_transfer_async (backend,
                                         G_LIST_MODEL (keys),
                                         SEAHORSE_PLACE (keyring),
                                         cancellable, on_import_complete,
                                         g_object_ref (row));
}

static void
seahorse_found_key_row_init (SeahorseFoundKeyRow *row)
{
}

static void
seahorse_found_key_row_class_init (SeahorseFoundKeyRowClass *klass)
{
}

static GtkWidget *
seahorse_found_key_row_new (SeahorsePgpKey *key)
{
    g_autoptr(SeahorseFoundKeyRow) row = NULL;
    const char *title = NULL;
    GtkWidget *import_button = NULL;

    row = g_object_new (SEAHORSE_TYPE_FOUND_KEY_ROW, NULL);
    row->key = key;

    title = seahorse_item_get_title (SEAHORSE_ITEM (key));

    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
    gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);

    import_button = gtk_button_new_from_icon_name ("document-save-symbolic");
    row->import_button = GTK_BUTTON (import_button);
    g_signal_connect_object (import_button, "clicked",
                             G_CALLBACK (on_import_button_clicked), row, 0);
    gtk_widget_set_valign (import_button, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class (import_button, "flat");
    gtk_widget_set_tooltip_text (import_button, _("Import"));
    adw_action_row_add_suffix (ADW_ACTION_ROW (row), import_button);

    return GTK_WIDGET (g_steal_pointer (&row));
}


/**
 * SeahorseKeyserverSearch:
 *
 * Contains the functions to start a search for keys on a keyserver.
 */

struct _SeahorseKeyserverSearch {
    AdwDialog parent_instance;

    GPtrArray *selected_servers; /* (element-type SeahorseServerSource) */
    gboolean selected_servers_changed;
    GListStore *keys;

    GtkWidget *navigation_view;

    GtkWidget *search_row;
    GtkWidget *key_server_list;

    GtkWidget *results_page;
    GtkWidget *results_view;
    GtkWidget *key_list;
};

G_DEFINE_TYPE (SeahorseKeyserverSearch, seahorse_keyserver_search, ADW_TYPE_DIALOG)

/* Enables the "search" button if the edit-field contains text and at least a
 * server is selected */
static void
on_keyserver_search_control_changed (GtkWidget *entry, SeahorseKeyserverSearch *self)
{
    gboolean enabled = TRUE;

    /* Need to have at least one key server selected ... */
    if (self->selected_servers->len == 0) {
        enabled = FALSE;

    /* ... and some search text */
    } else {
        g_autofree char *text = NULL;

        text = gtk_editable_get_chars (GTK_EDITABLE (self->search_row), 0, -1);
        if (!text || !text[0])
            enabled = FALSE;
    }

    gtk_widget_action_set_enabled (GTK_WIDGET (self), "search", enabled);
}

static void
on_search_completed (GObject *source, GAsyncResult *result, gpointer user_data)
{
    SeahorseKeyserverSearch *self = SEAHORSE_KEYSERVER_SEARCH (user_data);
    SeahorsePgpBackend *backend = seahorse_pgp_backend_get ();
    g_autoptr(GError) error = NULL;

    seahorse_pgp_backend_search_remote_finish (backend, result, &error);
    if (error != NULL) {
        g_dbus_error_strip_remote_error (error);
        seahorse_util_show_error (GTK_WIDGET (self),
                                  _("The search for keys failed."),
                                  error->message);
    }

    g_object_unref (self);
}

/* Extracts data, stores it in settings and starts a search using the entered
 * search data. */
static void
search_action (GtkWidget *widget, const char *action_name, GVariant *param)
{
    SeahorseKeyserverSearch *self = SEAHORSE_KEYSERVER_SEARCH (widget);
    SeahorseAppSettings *app_settings;
    const char *search_text = NULL;
    GCancellable *cancellable;

    app_settings = seahorse_app_settings_instance ();

    /* The keyservers to search, and save for next time */
    if (!self->selected_servers_changed) {
        g_autoptr(GPtrArray) new_last_servers = NULL;
        SeahorsePgpBackend *pgp_backend;
        GListModel *remotes;

        new_last_servers = g_ptr_array_new_full (self->selected_servers->len + 1,
                                                 g_free);

        pgp_backend = seahorse_pgp_backend_get ();
        remotes = seahorse_pgp_backend_get_remotes (pgp_backend);

        /* Save an empty array if all are selected */
        if (g_list_model_get_n_items (remotes) != self->selected_servers->len) {
            for (guint i = 0; i < self->selected_servers->len; i++) {
                SeahorseServerSource *ssrc = g_ptr_array_index (self->selected_servers, i);
                g_ptr_array_add (new_last_servers,
                                 seahorse_place_get_uri (SEAHORSE_PLACE (ssrc)));
            }
        }
        g_ptr_array_add (new_last_servers, NULL);

        seahorse_app_settings_set_last_search_servers (app_settings,
                                                       (char **) new_last_servers->pdata);
    }

    search_text = gtk_editable_get_text (GTK_EDITABLE (self->search_row));
    seahorse_app_settings_set_last_search_text (app_settings, search_text);

    /* Clear any keys from the last time */
    g_list_store_remove_all (self->keys);

    cancellable = g_cancellable_new ();
    seahorse_pgp_backend_search_remote_async (seahorse_pgp_backend_get (),
                                              search_text,
                                              self->keys,
                                              cancellable, on_search_completed,
                                              g_object_ref (self));

    adw_navigation_view_push (ADW_NAVIGATION_VIEW (self->navigation_view),
                              ADW_NAVIGATION_PAGE (self->results_page));
}

static void
on_keyserver_row_activated (GtkListBox    *box,
                            GtkListBoxRow *_row,
                            void          *user_data)
{
    SeahorseKeyserverSearch *self = SEAHORSE_KEYSERVER_SEARCH (user_data);
    SeahorseKeyserverRow *row = SEAHORSE_KEYSERVER_ROW (_row);
    gboolean found;
    unsigned int pos;

    self->selected_servers_changed = TRUE;
    found = g_ptr_array_find (self->selected_servers, row->keyserver, &pos);
    if (found) {
        g_ptr_array_remove_index (self->selected_servers, pos);
    } else {
        g_ptr_array_add (self->selected_servers, row->keyserver);
    }

    gtk_widget_set_visible (row->check_image, !found);

    on_keyserver_search_control_changed (NULL, self);
}

GtkWidget *
create_row_for_server_source (void *item,
                              void *user_data)
{
    SeahorseKeyserverSearch *self = SEAHORSE_KEYSERVER_SEARCH (user_data);

    return seahorse_keyserver_row_new (SEAHORSE_SERVER_SOURCE (item),
                                       self->selected_servers);
}

static void
on_result_row_activated (GtkListBox *key_list, GtkListBoxRow *row, void *user_data)
{
    SeahorseKeyserverSearch *self = SEAHORSE_KEYSERVER_SEARCH (user_data);
    SeahorseFoundKeyRow *_row = SEAHORSE_FOUND_KEY_ROW (row);

    g_return_if_fail (SEAHORSE_IS_VIEWABLE (_row->key));

    seahorse_viewable_view (SEAHORSE_VIEWABLE (_row->key), GTK_WINDOW (self));
}

static GtkWidget *
create_row_for_key (void *item, void *user_data)
{
    return seahorse_found_key_row_new (SEAHORSE_PGP_KEY (item));
}

static void
seahorse_keyserver_search_init (SeahorseKeyserverSearch *self)
{
    SeahorsePgpBackend *pgp_backend;
    GListModel *remotes;
    SeahorseAppSettings *app_settings;
    g_auto(GStrv) last_servers = NULL;
    g_autofree char *search_text = NULL;

    gtk_widget_init_template (GTK_WIDGET (self));

    /* Get the remotes */
    pgp_backend = seahorse_pgp_backend_get ();
    remotes = seahorse_pgp_backend_get_remotes (pgp_backend);

    /* Find what the user last searched for and use it to fill in which
     * servers are selected initially */
    app_settings = seahorse_app_settings_instance ();
    self->selected_servers = g_ptr_array_new ();

    last_servers = seahorse_app_settings_get_last_search_servers (app_settings);
    for (unsigned int i = 0; i < g_list_model_get_n_items (remotes); i++) {
        g_autoptr(SeahorseServerSource) ssrc = g_list_model_get_item (remotes, i);
        g_autofree char *ssrc_name = NULL;
        g_autofree char *ssrc_name_fold = NULL;

        /* If no search saved: select all */
        if (!last_servers || !last_servers[0]) {
            g_ptr_array_add (self->selected_servers, ssrc);
            continue;
        }

        ssrc_name = seahorse_place_get_uri (SEAHORSE_PLACE (ssrc));
        ssrc_name_fold = g_utf8_casefold (ssrc_name, -1);

        for (unsigned int j = 0; last_servers[j]; j++) {
            const char *name = last_servers[j];

            if (g_utf8_collate (name, ssrc_name_fold) == 0)
                g_ptr_array_add (self->selected_servers, ssrc);
        }
    }

    search_text = seahorse_app_settings_get_last_search_text (app_settings);
    if (search_text != NULL) {
        gtk_editable_set_text (GTK_EDITABLE (self->search_row), search_text);
        gtk_editable_select_region (GTK_EDITABLE (self->search_row), 0, -1);
    }

    /* The key servers to list */
    gtk_list_box_bind_model (GTK_LIST_BOX (self->key_server_list),
                             remotes,
                             create_row_for_server_source,
                             self,
                             NULL);

    on_keyserver_search_control_changed (NULL, self);

    /* init key list */
    self->keys = g_list_store_new (SEAHORSE_PGP_TYPE_KEY);
    gtk_list_box_bind_model (GTK_LIST_BOX (self->key_list),
                             G_LIST_MODEL (self->keys),
                             create_row_for_key, self, NULL);
}

static void
seahorse_keyserver_search_finalize (GObject *object)
{
    SeahorseKeyserverSearch *self = SEAHORSE_KEYSERVER_SEARCH (object);

    g_ptr_array_unref (self->selected_servers);
    g_clear_object (&self->keys);

    G_OBJECT_CLASS (seahorse_keyserver_search_parent_class)->finalize (object);
}

static void
seahorse_keyserver_search_class_init (SeahorseKeyserverSearchClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->finalize = seahorse_keyserver_search_finalize;

    gtk_widget_class_install_action (widget_class, "search", NULL, search_action);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Seahorse/seahorse-keyserver-search.ui");

    gtk_widget_class_bind_template_child (widget_class, SeahorseKeyserverSearch, navigation_view);
    gtk_widget_class_bind_template_child (widget_class, SeahorseKeyserverSearch, search_row);
    gtk_widget_class_bind_template_child (widget_class, SeahorseKeyserverSearch, key_server_list);
    gtk_widget_class_bind_template_child (widget_class, SeahorseKeyserverSearch, results_page);
    gtk_widget_class_bind_template_child (widget_class, SeahorseKeyserverSearch, results_view);
    gtk_widget_class_bind_template_child (widget_class, SeahorseKeyserverSearch, key_list);

    gtk_widget_class_bind_template_callback (widget_class, on_keyserver_search_control_changed);
    gtk_widget_class_bind_template_callback (widget_class, on_keyserver_row_activated);
    gtk_widget_class_bind_template_callback (widget_class, on_result_row_activated);
}

SeahorseKeyserverSearch *
seahorse_keyserver_search_new (void)
{
    return g_object_new (SEAHORSE_TYPE_KEYSERVER_SEARCH, NULL);
}
