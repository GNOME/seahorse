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

#include "seahorse-discovery.h"
#include "seahorse-keyserver-results.h"
#include "seahorse-pgp-backend.h"

#include "seahorse-common.h"

#include "libseahorse/seahorse-util.h"
#include "libseahorse/seahorse-widget.h"

/**
 * SECTION:seahorse-keyserver-search
 * @short_description: Contains the functions to start a search for keys on a
 * keyserver.
 **/

struct _SeahorseKeyserverSearch {
	GtkDialog parent_instance;

	GtkWidget *search_entry;
	GtkWidget *key_server_list;
	GtkWidget *shared_keys_list;
	GtkWidget *searchbutton;
};

G_DEFINE_TYPE (SeahorseKeyserverSearch, seahorse_keyserver_search, GTK_TYPE_DIALOG)

/**
 * KeyserverSelection:
 * @names: A list of keyserver names
 * @uris: A list of keyserver URIs
 * @all: TRUE if all keyservers are selected
 **/
typedef struct _KeyserverSelection {
	GPtrArray *names;
	GPtrArray *uris;
	gboolean all;
} KeyserverSelection;


/* Selection Retrieval ------------------------------------------------------ */

/**
 * widget: CHECK_BUTTON widget to read
 * selection: will be updated depending on the state of the widget
 *
 * Adds the name/uri of the checked widget to the selection
 *
 **/
static void
get_checks (GtkWidget *widget, KeyserverSelection *selection)
{
	const gchar *value;

	if (!GTK_IS_CHECK_BUTTON (widget))
		return;

	/* Pull in the selected name and uri */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
		value = gtk_button_get_label (GTK_BUTTON (widget));
		g_ptr_array_add (selection->names, g_strdup (value));
		value = g_object_get_data (G_OBJECT (widget), "keyserver-uri");
		g_ptr_array_add (selection->uris, g_strdup (value));

	/* Note that not all checks are selected */
	} else {
		selection->all = FALSE;
	}
}

/**
 * extracts all keyservers in the sub-widgets "key-server-list" and
 * "shared-keys-list" and fills a KeyserverSelection structure.
 *
 * returns the selection
 **/
static KeyserverSelection*
get_keyserver_selection (SeahorseKeyserverSearch *self)
{
	KeyserverSelection *selection;

	selection = g_new0 (KeyserverSelection, 1);
	selection->all = TRUE;
	selection->uris = g_ptr_array_new_with_free_func (g_free);
	selection->names = g_ptr_array_new_with_free_func (g_free);

	/* Key servers */
	gtk_container_foreach (GTK_CONTAINER (self->key_server_list), (GtkCallback)get_checks, selection);
	/* Shared Key */
	gtk_container_foreach (GTK_CONTAINER (self->shared_keys_list), (GtkCallback)get_checks, selection);

	g_ptr_array_add (selection->uris, NULL);
	g_ptr_array_add (selection->names, NULL);

	return selection;
}

/**
 * selection: The selection to free
 *
 * All data (string lists, structures) are freed
 *
 **/
static void
free_keyserver_selection (KeyserverSelection *selection)
{
	if (selection) {
		g_ptr_array_free (selection->uris, TRUE);
		g_ptr_array_free (selection->names, TRUE);
		g_free (selection);
	}
}

/**
 * widget: a CHECK_BUTTON
 * checked: out- TRUE if the button is active, stays the same else.
 *
 **/
static void
have_checks (GtkWidget *widget, gboolean *checked)
{
    if (GTK_IS_CHECK_BUTTON (widget)) {
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
            *checked = TRUE;
    }
}

/* returns TRUE if at least one of the key servers was selected */
static gboolean
have_keyserver_selection (SeahorseKeyserverSearch *self)
{
    gboolean checked = FALSE;

    /* Key servers */
    gtk_container_foreach (GTK_CONTAINER (self->key_server_list), (GtkCallback)have_checks, &checked);
    /* Shared keys */
    gtk_container_foreach (GTK_CONTAINER (self->shared_keys_list), (GtkCallback)have_checks, &checked);

    return checked;
}

/* Enables the "search" button if the edit-field contains text and at least a
 * server is selected */
static void
on_keyserver_search_control_changed (GtkWidget *widget, gpointer user_data)
{
	SeahorseKeyserverSearch *self = SEAHORSE_KEYSERVER_SEARCH (user_data);
    gboolean enabled = TRUE;

    /* Need to have at least one key server selected ... */
    if (!have_keyserver_selection (self)) {
        enabled = FALSE;

    /* ... and some search text */
	} else {
		g_autofree gchar *text = NULL;

        text = gtk_editable_get_chars (GTK_EDITABLE (self->search_entry), 0, -1);
        if (!text || !text[0])
            enabled = FALSE;
    }

    gtk_widget_set_sensitive (self->searchbutton, enabled);
}

/* Initial Selection -------------------------------------------------------- */

static void
foreach_child_select_checks (GtkWidget *widget, gpointer user_data)
{
	gchar **names = user_data;
	guint i;

	if (GTK_IS_CHECK_BUTTON (widget)) {
		g_autofree gchar *name = NULL;
		gboolean checked;

		name = g_utf8_casefold (gtk_button_get_label (GTK_BUTTON (widget)), -1);
		checked = !(names != NULL && names[0] != NULL);
		for (i = 0; names && names[i] != NULL; i++) {
			if (g_utf8_collate (names[i], name) == 0) {
				checked = TRUE;
				break;
			}
		}
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), checked);
	}
}

/* Reads key servers from settings and updates the UI content. */
static void
select_inital_keyservers (SeahorseKeyserverSearch *self)
{
	SeahorseAppSettings *app_settings;
	gchar **names;
	guint i;

	app_settings = seahorse_app_settings_instance ();
	names = seahorse_app_settings_get_last_search_servers (app_settings, NULL);

	/* We do case insensitive matches */
	for (i = 0; names[i] != NULL; i++) {
		gchar *name;

		name = g_utf8_casefold (names[i], -1);
		g_free (names[i]);
		names[i] = name;
	}

	gtk_container_foreach (GTK_CONTAINER (self->key_server_list), foreach_child_select_checks, names);
	gtk_container_foreach (GTK_CONTAINER (self->shared_keys_list), foreach_child_select_checks, names);
}

/* Populating Lists --------------------------------------------------------- */

/**
 * widget: a check button
 * unchecked: a hash table containing the state of the servers
 *
 * If the button is not checked, the hash table entry associate with it will be
 * replaced with ""
 *
 **/
static void
remove_checks (GtkWidget *widget, GHashTable *unchecked)
{
    if (GTK_IS_CHECK_BUTTON (widget)) {

        if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
            g_hash_table_replace (unchecked,
                g_strdup (gtk_button_get_label (GTK_BUTTON (widget))), "");

        gtk_widget_destroy (widget);
    }
}

/**
* box: the GTK_CONTAINER with the checkboxes
* uris: the uri list of the keyservers
* names: the keyserver names
*
* Updates the box and adds checkboxes containing names/uris. The check-status
* of already existing check boxes is not changed.
**/
static void
populate_keyserver_list (SeahorseKeyserverSearch *self, GtkWidget *box, gchar **uris,
                         gchar **names)
{
	GtkContainer *cont = GTK_CONTAINER (box);
	GHashTable *unchecked;
	gboolean any = FALSE;
	GtkWidget *check;
	guint i;

	/* Remove all checks, and note which ones were unchecked */
	unchecked = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	gtk_container_foreach (cont, (GtkCallback)remove_checks, unchecked);

	/* Now add the new ones back */
	for (i = 0; uris && uris[i] && names && names[i]; i++) {
		any = TRUE;

		/* A new checkbox with this the name as the label */
		check = gtk_check_button_new_with_label (names[i]);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
		                              g_hash_table_lookup (unchecked, names[i]) == NULL);
		g_signal_connect (check, "toggled", G_CALLBACK (on_keyserver_search_control_changed), self);
		gtk_widget_show (check);

		/* Save URI and set it as the tooltip */
		g_object_set_data_full (G_OBJECT (check), "keyserver-uri", g_strdup (uris[i]), g_free);
		gtk_widget_set_tooltip_text (check, uris[i]);

		gtk_container_add (cont, check);
	}

	g_hash_table_destroy (unchecked);

	/* Only display the container if we had some checks */
	gtk_widget_set_visible (box, any);
}

static void
on_settings_keyservers_changed (GSettings *settings, const gchar *key, gpointer user_data)
{
	SeahorseKeyserverSearch *self = SEAHORSE_KEYSERVER_SEARCH (user_data);
	g_auto(GStrv) keyservers = NULL;
	g_auto(GStrv) names = NULL;

	keyservers = seahorse_servers_get_uris ();
	names = seahorse_servers_get_names ();
	populate_keyserver_list (self, self->key_server_list, keyservers, names);
}

/**
* ssd: the SeahorseServiceDiscovery. List-data is read from there
* name: ignored
*
* refreshes the "shared-keys-list"
*
**/
static void
refresh_shared_keys (SeahorseDiscovery *ssd,
                     const gchar *name,
                     gpointer user_data)
{
	SeahorseKeyserverSearch *self = SEAHORSE_KEYSERVER_SEARCH (user_data);
	g_auto(GStrv) keyservers = NULL;
	g_auto(GStrv) names = NULL;

	names = seahorse_discovery_list (ssd);
	keyservers = seahorse_discovery_get_uris (ssd, (const gchar **)names);
	populate_keyserver_list (self, self->shared_keys_list, keyservers, names);
}

/* -------------------------------------------------------------------------- */

/* Extracts data, stores it in settings and starts a search using the entered
 * search data. */
static void
on_keyserver_search_ok_clicked (GtkButton *button, gpointer user_data)
{
	SeahorseKeyserverSearch *self = SEAHORSE_KEYSERVER_SEARCH (user_data);
	KeyserverSelection *selection;
	const gchar *search;
	GtkWindow *parent;

	/* Get search text and save it for next time */
	search = gtk_entry_get_text (GTK_ENTRY (self->search_entry));
	g_return_if_fail (search != NULL && search[0] != 0);
	seahorse_app_settings_set_last_search_text (seahorse_app_settings_instance (), search);

	/* The keyservers to search, and save for next time */
	selection = get_keyserver_selection (self);
	g_return_if_fail (selection->uris != NULL);
	g_settings_set_strv (G_SETTINGS (seahorse_app_settings_instance ()), "last-search-servers",
	                     selection->all ? NULL : (const gchar * const*)selection->uris->pdata);

	/* Open the new result window; its transient parent is *our* transient
	 * parent (Seahorse's primary window), not ourselves, as *we* will
	 * disappear when "OK" is clicked.
	 */
	parent = gtk_window_get_transient_for (GTK_WINDOW (self));
	seahorse_keyserver_results_show (search, parent);

	free_keyserver_selection (selection);
}

static void
cleanup_signals (GtkWidget *widget, gpointer user_data)
{
	SeahorseKeyserverSearch *self = SEAHORSE_KEYSERVER_SEARCH (user_data);
	SeahorseDiscovery *ssd = seahorse_pgp_backend_get_discovery (NULL);
	g_signal_handlers_disconnect_by_func (ssd, refresh_shared_keys, self);
}

static void
seahorse_keyserver_search_init (SeahorseKeyserverSearch *self)
{
	g_autofree gchar *search_text = NULL;
	SeahorsePgpSettings *settings;
	SeahorseDiscovery *ssd;

	gtk_widget_init_template (GTK_WIDGET (self));

	search_text = seahorse_app_settings_get_last_search_text (seahorse_app_settings_instance ());
	if (search_text != NULL) {
		gtk_entry_set_text (GTK_ENTRY (self->search_entry), search_text);
		gtk_editable_select_region (GTK_EDITABLE (self->search_entry), 0, -1);
	}

	/* The key servers to list */
	settings = seahorse_pgp_settings_instance ();
	on_settings_keyservers_changed (G_SETTINGS (settings), "keyservers", self);
	g_signal_connect_object (settings, "changed::keyservers",
	                         G_CALLBACK (on_settings_keyservers_changed), self, 0);

	/* Any shared keys to list */
	ssd = seahorse_pgp_backend_get_discovery (NULL);
	refresh_shared_keys (ssd, NULL, self);
	g_signal_connect (ssd, "added", G_CALLBACK (refresh_shared_keys), self);
	g_signal_connect (ssd, "removed", G_CALLBACK (refresh_shared_keys), self);
	g_signal_connect (GTK_WINDOW (self), "destroy", G_CALLBACK (cleanup_signals), self);

	select_inital_keyservers (self);
	on_keyserver_search_control_changed (NULL, self);
}

static void
seahorse_keyserver_search_class_init (SeahorseKeyserverSearchClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Seahorse/seahorse-keyserver-search.ui");

	gtk_widget_class_bind_template_child (widget_class, SeahorseKeyserverSearch, search_entry);
	gtk_widget_class_bind_template_child (widget_class, SeahorseKeyserverSearch, key_server_list);
	gtk_widget_class_bind_template_child (widget_class, SeahorseKeyserverSearch, shared_keys_list);
	gtk_widget_class_bind_template_child (widget_class, SeahorseKeyserverSearch, searchbutton);

	gtk_widget_class_bind_template_callback (widget_class, on_keyserver_search_control_changed);
	gtk_widget_class_bind_template_callback (widget_class, on_keyserver_search_ok_clicked);
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

	self = g_object_new (SEAHORSE_TYPE_KEYSERVER_SEARCH,
	                     "use-header-bar", 1,
	                     NULL);

	return g_steal_pointer (&self);
}
