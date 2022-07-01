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
#include "seahorse-keyserver-results.h"
#include "seahorse-pgp-backend.h"

#include "seahorse-common.h"

/**
 * SECTION:seahorse-keyserver-search
 * @short_description: Contains the functions to start a search for keys on a
 * keyserver.
 */

struct _SeahorseKeyserverSearch {
    GtkApplicationWindow parent_instance;

    GPtrArray *selected_servers; /* (element-type SeahorseServerSource) */
    gboolean selected_servers_changed;

    GtkWidget *search_row;
    GtkWidget *key_server_list;
};

G_DEFINE_TYPE (SeahorseKeyserverSearch, seahorse_keyserver_search, GTK_TYPE_APPLICATION_WINDOW)

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

/* Extracts data, stores it in settings and starts a search using the entered
 * search data. */
static void
search_action (GtkWidget *widget, const char *action_name, GVariant *param)
{
    SeahorseKeyserverSearch *self = SEAHORSE_KEYSERVER_SEARCH (widget);
    SeahorseAppSettings *app_settings;
    const char *search_text = NULL;

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
    gtk_window_close (GTK_WINDOW (self));
    seahorse_keyserver_results_show (search_text,
                                     gtk_window_get_transient_for (GTK_WINDOW (self)));
}

static void
on_row_activated (GtkListBox    *box,
                  GtkListBoxRow *row,
                  gpointer       user_data)
{
    SeahorseKeyserverSearch *self = SEAHORSE_KEYSERVER_SEARCH (user_data);
    SeahorseServerSource *ssrc;
    GtkWidget *check;
    gboolean found;
    guint pos;

    ssrc = g_object_get_data (G_OBJECT (row), "keyserver-uri");

    g_return_if_fail (SEAHORSE_IS_SERVER_SOURCE (ssrc));

    self->selected_servers_changed = TRUE;
    found = g_ptr_array_find (self->selected_servers, ssrc, &pos);
    if (found) {
        g_ptr_array_remove_index (self->selected_servers, pos);
    } else {
        g_ptr_array_add (self->selected_servers, ssrc);
    }

    check = g_object_get_data (G_OBJECT (row), "check");
    gtk_widget_set_visible (check, !found);

    on_keyserver_search_control_changed (NULL, self);
}

GtkWidget *
create_row_for_server_source (gpointer item,
                              gpointer user_data)
{
    SeahorseKeyserverSearch *self = SEAHORSE_KEYSERVER_SEARCH (user_data);
    SeahorseServerSource *ssrc = SEAHORSE_SERVER_SOURCE (item);
    g_autofree char *uri = NULL;
    GtkWidget *row;
    GtkWidget *check;
    gboolean is_selected;

    row = adw_action_row_new ();
    gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (row), FALSE);
    g_object_set_data (G_OBJECT (row), "keyserver-uri", ssrc);

    uri = seahorse_place_get_uri (SEAHORSE_PLACE (ssrc));
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), uri);

    check = gtk_image_new_from_icon_name ("emblem-ok-symbolic");
    is_selected = g_ptr_array_find (self->selected_servers, ssrc, NULL);
    gtk_widget_set_visible (check, is_selected);
    adw_action_row_add_suffix (ADW_ACTION_ROW (row), check);
    g_object_set_data (G_OBJECT (row), "check", check);

    return row;
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
    for (guint i = 0; i < g_list_model_get_n_items (remotes); i++) {
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

        for (guint j = 0; last_servers[j]; j++) {
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
    g_signal_connect (self->key_server_list,
                      "row-activated",
                      G_CALLBACK (on_row_activated),
                      self);
    gtk_list_box_bind_model (GTK_LIST_BOX (self->key_server_list),
                             remotes,
                             create_row_for_server_source,
                             self,
                             NULL);

    on_keyserver_search_control_changed (NULL, self);
}

static void
seahorse_keyserver_search_finalize (GObject *object)
{
    SeahorseKeyserverSearch *self = SEAHORSE_KEYSERVER_SEARCH (object);

    g_ptr_array_unref (self->selected_servers);

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

    gtk_widget_class_bind_template_child (widget_class, SeahorseKeyserverSearch, search_row);
    gtk_widget_class_bind_template_child (widget_class, SeahorseKeyserverSearch, key_server_list);

    gtk_widget_class_bind_template_callback (widget_class, on_keyserver_search_control_changed);
}

/**
 * seahorse_keyserver_search_show:
 * @parent: the parent window to connect this window to
 *
 * Shows a remote search window.
 *
 * Returns: the new window.
 */
SeahorseKeyserverSearch *
seahorse_keyserver_search_new (GtkWindow *parent)
{
    g_autoptr(SeahorseKeyserverSearch) self = NULL;

    self = g_object_new (SEAHORSE_TYPE_KEYSERVER_SEARCH, NULL);

    return g_steal_pointer (&self);
}
