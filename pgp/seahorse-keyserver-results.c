/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

#include "config.h"

#include "seahorse-keyserver-results.h"

#include "seahorse-pgp-backend.h"
#include "seahorse-gpgme-keyring.h"
#include "seahorse-keyserver-search.h"

#include "libseahorse/seahorse-progress.h"
#include "libseahorse/seahorse-util.h"

#include <glib/gi18n.h>

#include <string.h>

#define SEAHORSE_TYPE_KEYSERVER_RESULTS_ROW (seahorse_keyserver_results_row_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseKeyserverResultsRow, seahorse_keyserver_results_row,
                      SEAHORSE, KEYSERVER_RESULTS_ROW,
                      AdwActionRow)

struct _SeahorseKeyserverResultsRow {
    AdwActionRow parent;

    GObject *key;
    GtkButton *import_button;
};

G_DEFINE_TYPE (SeahorseKeyserverResultsRow, seahorse_keyserver_results_row, ADW_TYPE_ACTION_ROW);

static void
on_import_complete (GObject *source, GAsyncResult *result, gpointer user_data)
{
    SeahorsePgpBackend *backend = SEAHORSE_PGP_BACKEND (source);
    g_autoptr(SeahorseKeyserverResultsRow) row =
        SEAHORSE_KEYSERVER_RESULTS_ROW (user_data);
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
on_import_button_clicked (GtkButton *import_button, gpointer user_data)
{
    SeahorseKeyserverResultsRow *row = user_data;
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
    keys = g_list_store_new (SEAHORSE_TYPE_OBJECT);
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
seahorse_keyserver_results_row_class_init (SeahorseKeyserverResultsRowClass *klass)
{
}

static void
seahorse_keyserver_results_row_init (SeahorseKeyserverResultsRow *row)
{
}

static GtkWidget *
seahorse_keyserver_results_row_new (GObject *item)
{
    g_autoptr(SeahorseKeyserverResultsRow) row = NULL;
    GtkWidget *import_button = NULL;
    g_autofree char *item_label = NULL;
    gboolean item_exportable;

    g_object_get (item,
                  "markup", &item_label,
                  "exportable", &item_exportable,
                  NULL);

    row = g_object_new (SEAHORSE_TYPE_KEYSERVER_RESULTS_ROW, NULL);
    gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (row), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (row), item_exportable);
    row->key = item;

    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), item_label);

    import_button = gtk_button_new_from_icon_name ("document-save-symbolic");
    row->import_button = GTK_BUTTON (import_button);
    g_signal_connect_object (import_button, "clicked",
                             G_CALLBACK (on_import_button_clicked), row, 0);
    gtk_widget_set_valign (import_button, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class (import_button, "flat");
    if (item_exportable) {
        gtk_widget_set_tooltip_text (import_button, _("Import"));
    } else {
        gtk_widget_set_tooltip_text (import_button, _("Can’t import key"));
        gtk_widget_set_sensitive (import_button, FALSE);
    }
    adw_action_row_add_suffix (ADW_ACTION_ROW (row), import_button);

    return GTK_WIDGET (g_steal_pointer (&row));
}


enum {
    PROP_0,
    PROP_SEARCH
};

struct _SeahorseKeyserverResults {
    GtkDialog parent;

    GtkBuilder *builder;

    char *search_string;
    GListStore *collection;
    GtkListBox *key_list;
};

G_DEFINE_TYPE (SeahorseKeyserverResults, seahorse_keyserver_results, GTK_TYPE_DIALOG);

static void
on_row_activated (GtkListBox *key_list, GtkListBoxRow *row, gpointer user_data)
{
    SeahorseKeyserverResults *self = SEAHORSE_KEYSERVER_RESULTS (user_data);
    SeahorseKeyserverResultsRow *_row = SEAHORSE_KEYSERVER_RESULTS_ROW (row);

    g_return_if_fail (SEAHORSE_IS_VIEWABLE (_row->key));

    seahorse_viewable_view (_row->key, GTK_WINDOW (self));
}

static GtkWidget *
create_row_for_key (void *item, gpointer user_data)
{
    return seahorse_keyserver_results_row_new ((GObject *) item);
}

static void
seahorse_keyserver_results_constructed (GObject *obj)
{
    SeahorseKeyserverResults *self = SEAHORSE_KEYSERVER_RESULTS (obj);
    GtkWindow *window = GTK_WINDOW (self);
    g_autofree char* title = NULL;

    G_OBJECT_CLASS (seahorse_keyserver_results_parent_class)->constructed (obj);

    if (g_utf8_strlen (self->search_string, -1) == 0) {
        title = g_strdup (_("Remote keys"));
    } else {
        title = g_strdup_printf (_("Remote keys containing “%s”"), self->search_string);
    }

    gtk_window_set_default_size (window, 640, 476);
    gtk_window_set_title (window, title);
    gtk_widget_set_visible (GTK_WIDGET (window), TRUE);

    self->builder = gtk_builder_new_from_resource ("/org/gnome/Seahorse/seahorse-keyserver-results.ui");
    gtk_window_set_child (GTK_WINDOW (self),
                          GTK_WIDGET (gtk_builder_get_object (self->builder, "keyserver-results")));

    /* init key list */
    self->key_list = GTK_LIST_BOX (gtk_builder_get_object (self->builder, "key_list"));
    g_signal_connect_object (self->key_list, "row-activated",
                             G_CALLBACK (on_row_activated), self, 0);
    gtk_list_box_bind_model (self->key_list, G_LIST_MODEL (self->collection), create_row_for_key, self, NULL);

    /* Set focus to the current key list */
    gtk_widget_grab_focus (GTK_WIDGET (self->key_list));
}

static void
seahorse_keyserver_results_init (SeahorseKeyserverResults *self)
{
    self->collection = g_list_store_new (SEAHORSE_PGP_TYPE_KEY);
}

static void
seahorse_keyserver_results_finalize (GObject *obj)
{
    SeahorseKeyserverResults *self = SEAHORSE_KEYSERVER_RESULTS (obj);

    g_clear_pointer (&self->search_string, g_free);
    g_clear_object (&self->collection);
    g_clear_object (&self->builder);

    G_OBJECT_CLASS (seahorse_keyserver_results_parent_class)->finalize (obj);
}

static void
seahorse_keyserver_results_set_property (GObject *obj,
                                         guint prop_id,
                                         const GValue *value,
                                         GParamSpec *pspec)
{
    SeahorseKeyserverResults *self = SEAHORSE_KEYSERVER_RESULTS (obj);
    const gchar* str;

    switch (prop_id) {
    case PROP_SEARCH:
        /* Many key servers ignore spaces at the beginning and end, so we do too */
        str = g_value_get_string (value);
        if (!str)
            str = "";
        self->search_string = g_strstrip (g_utf8_casefold (str, -1));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
        break;
    }
}

static void
seahorse_keyserver_results_get_property (GObject *obj,
                                         guint prop_id,
                                         GValue *value,
                                         GParamSpec *pspec)
{
    SeahorseKeyserverResults *self = SEAHORSE_KEYSERVER_RESULTS (obj);

    switch (prop_id) {
    case PROP_SEARCH:
        g_value_set_string (value, seahorse_keyserver_results_get_search (self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
        break;
    }
}

static void
seahorse_keyserver_results_class_init (SeahorseKeyserverResultsClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->constructed = seahorse_keyserver_results_constructed;
    gobject_class->finalize = seahorse_keyserver_results_finalize;
    gobject_class->set_property = seahorse_keyserver_results_set_property;
    gobject_class->get_property = seahorse_keyserver_results_get_property;

    g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SEARCH,
             g_param_spec_string ("search", "search", "search", NULL,
                                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
on_search_completed (GObject *source, GAsyncResult *result, gpointer user_data)
{
    SeahorseKeyserverResults *self = SEAHORSE_KEYSERVER_RESULTS (user_data);
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

/**
 * seahorse_keyserver_results_show:
 * @search_text: The text to search for
 * @parent: A GTK window as parent (or NULL)
 *
 * Creates a search results window and adds the operation to it's progress status.
 */
void
seahorse_keyserver_results_show (const gchar* search_text, GtkWindow *parent)
{
    SeahorseKeyserverResults* self;
    g_autoptr(GCancellable) cancellable = NULL;

    g_return_if_fail (search_text != NULL);

    self = g_object_new (SEAHORSE_TYPE_KEYSERVER_RESULTS,
                         "search", search_text,
                         "transient-for", parent,
                         NULL);

    /* Destorys itself with destroy */
    g_object_ref_sink (self);

    cancellable = g_cancellable_new ();

    seahorse_pgp_backend_search_remote_async (seahorse_pgp_backend_get (),
                                              search_text,
                                              self->collection,
                                              cancellable, on_search_completed,
                                              g_object_ref (self));

    seahorse_progress_attach (cancellable, self->builder);
}

/**
 * seahorse_keyserver_results_get_search:
 * @self:  SeahorseKeyserverResults object to get the search string from
 *
 * Returns: The search string in from the results
 */
const gchar*
seahorse_keyserver_results_get_search (SeahorseKeyserverResults* self)
{
    g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), NULL);
    return self->search_string;
}
