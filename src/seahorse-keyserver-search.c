/*
 * Seahorse
 *
 * Copyright (C) 2004-2005 Stefan Walter
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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include "seahorse-context.h"
#include "seahorse-dns-sd.h"
#include "seahorse-keyserver-results.h"
#include "seahorse-preferences.h"
#include "seahorse-servers.h"
#include "seahorse-util.h"
#include "seahorse-widget.h"

/**
 * SECTION:seahorse-keyserver-search
 * @short_description: Contains the functions to start a search for keys on a
 * keyserver.
 **/

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
 * swidget: the window/main widget
 *
 * extracts all keyservers in the sub-widgets "key-server-list" and
 * "shared-keys-list" and fills a KeyserverSelection structure.
 *
 * returns the selection
 **/
static KeyserverSelection*
get_keyserver_selection (SeahorseWidget *swidget)
{
	KeyserverSelection *selection;
	GtkWidget *widget;

	selection = g_new0 (KeyserverSelection, 1);
	selection->all = TRUE;
	selection->uris = g_ptr_array_new_with_free_func (g_free);
	selection->names = g_ptr_array_new_with_free_func (g_free);

	/* Key servers */
	widget = seahorse_widget_get_widget (swidget, "key-server-list");
	g_return_val_if_fail (widget != NULL, selection);
	gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback)get_checks, selection);

	/* Shared Key */
	widget = seahorse_widget_get_widget (swidget, "shared-keys-list");
	g_return_val_if_fail (widget != NULL, selection);
	gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback)get_checks, selection);

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

/**
 * swidget: sub widgets in here will be  checked
 *
 * returns TRUE if at least one of the CHECK_BUTTONS in "key-server-list" or
 * "shared-keys-list" is TRUE
 **/
static gboolean
have_keyserver_selection (SeahorseWidget *swidget)
{
    GtkWidget *w;
    gboolean checked = FALSE;
    
    /* Key servers */
    w = GTK_WIDGET (seahorse_widget_get_widget (swidget, "key-server-list"));
    g_return_val_if_fail (w != NULL, FALSE);
    gtk_container_foreach (GTK_CONTAINER (w), (GtkCallback)have_checks, &checked);

    /* Shared keys */
    w = GTK_WIDGET (seahorse_widget_get_widget (swidget, "shared-keys-list"));
    g_return_val_if_fail (w != NULL, FALSE);
    gtk_container_foreach (GTK_CONTAINER (w), (GtkCallback)have_checks, &checked);        
    
    return checked;
}

/**
 * on_keyserver_search_control_changed:
 * @widget: ignored
 * @swidget: main widget
 *
 *
 * Enables the "search" button if the edit-field contains text and at least a
 * server is selected
 */
G_MODULE_EXPORT void
on_keyserver_search_control_changed (GtkWidget *widget, SeahorseWidget *swidget)
{
    gboolean enabled = TRUE;
    GtkWidget *w;
    gchar *text;
    
    /* Need to have at least one key server selected ... */
    if (!have_keyserver_selection (swidget))
        enabled = FALSE;
    
    /* ... and some search text */
    else {
        w = GTK_WIDGET (seahorse_widget_get_widget (swidget, "search-text"));
        text = gtk_editable_get_chars (GTK_EDITABLE (w), 0, -1);
        if (!text || !text[0])
            enabled = FALSE;
        g_free (text);
    }
        
    w = GTK_WIDGET (seahorse_widget_get_widget (swidget, "search"));
    gtk_widget_set_sensitive (w, enabled);
}

/* Initial Selection -------------------------------------------------------- */

static void
foreach_child_select_checks (GtkWidget *widget, gpointer user_data)
{
	gchar **names = user_data;
	gboolean checked;
	gchar *name;
	guint i;

	if (GTK_IS_CHECK_BUTTON (widget)) {
		name = g_utf8_casefold (gtk_button_get_label (GTK_BUTTON (widget)), -1);
		checked = names ? FALSE : TRUE;
		for (i = 0; names && names[i] != NULL; i++) {
			if (g_utf8_collate (names[i], name) == 0) {
				checked = TRUE;
				break;
			}
		}
		g_free (name);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), checked);
	}
}

/**
 * swidget: the main widget
 *
 * Reads key servers from gconf and updates the UI content.
 *
 **/
static void
select_inital_keyservers (SeahorseWidget *swidget)
{
	gchar **names;
	GtkWidget *widget;
	gchar *name;
	guint i;

	names = g_settings_get_strv (seahorse_context_settings (NULL), "last-search-servers");

	/* Close the expander if all servers are selected */
	widget = seahorse_widget_get_widget (swidget, "search-where");
	g_return_if_fail (widget != NULL);
	gtk_expander_set_expanded (GTK_EXPANDER (widget), names != NULL);

	/* We do case insensitive matches */
	for (i = 0; names[i] != NULL; i++) {
		name = g_utf8_casefold (names[i], -1);
		g_free (names[i]);
		names[i] = name;
	}

	widget = seahorse_widget_get_widget (swidget, "key-server-list");
	g_return_if_fail (widget != NULL);
	gtk_container_foreach (GTK_CONTAINER (widget), foreach_child_select_checks, names);

	widget = seahorse_widget_get_widget (swidget, "shared-keys-list");
	g_return_if_fail (widget != NULL);
	gtk_container_foreach (GTK_CONTAINER (widget), foreach_child_select_checks, names);
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
* swidget: the main widget
* box: the GTK_CONTAINER with the checkboxes
* uris: the uri list of the keyservers
* names: the keyserver names
*
* Updates the box and adds checkboxes containing names/uris. The check-status
* of already existing check boxes is not changed.
**/
static void
populate_keyserver_list (SeahorseWidget *swidget, GtkWidget *box, gchar **uris,
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
		g_signal_connect (check, "toggled", G_CALLBACK (on_keyserver_search_control_changed), swidget);
		gtk_widget_show (check);

		/* Save URI and set it as the tooltip */
		g_object_set_data_full (G_OBJECT (check), "keyserver-uri", g_strdup (uris[i]), g_free);
		gtk_widget_set_tooltip_text (check, uris[i]);

		gtk_container_add (cont, check);
	}

	g_hash_table_destroy (unchecked);

	/* Only display the container if we had some checks */
	if (any)
		gtk_widget_show (box);
	else
		gtk_widget_hide (box);
}

static void
on_settings_keyservers_changed (GSettings *settings, const gchar *key, gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	GtkWidget *widget;
	gchar **keyservers;
	gchar **names;

	widget = seahorse_widget_get_widget (swidget, "key-server-list");
	g_return_if_fail (widget != NULL);

	keyservers = seahorse_servers_get_uris ();
	names = seahorse_servers_get_names ();
	populate_keyserver_list (swidget, widget, keyservers, names);

	g_strfreev (keyservers);
	g_strfreev (names);
}

/**
* ssd: the SeahorseServiceDiscovery. List-data is read from there
* name: ignored
* swidget: The SeahorseWidget
*
* refreshes the "shared-keys-list"
*
**/
static void 
refresh_shared_keys (SeahorseServiceDiscovery *ssd, const gchar *name, SeahorseWidget *swidget)
{
	gchar **keyservers;
	gchar **names;
	GtkWidget *widget;

	widget = seahorse_widget_get_widget (swidget, "shared-keys-list");
	g_return_if_fail (widget != NULL);

	names = seahorse_service_discovery_list (ssd);
	keyservers = seahorse_service_discovery_get_uris (ssd, (const gchar **)names);
	populate_keyserver_list (swidget, widget, keyservers, names);

	g_strfreev (keyservers);
	g_strfreev (names);
}

/* -------------------------------------------------------------------------- */
 
/**
 * on_keyserver_search_ok_clicked:
 * @button: ignored
 * @swidget: The SeahorseWidget to work with
 *
 * Extracts data, stores it in GConf and starts a search using the entered
 * search data.
 *
 * This function gets the things done
 */
G_MODULE_EXPORT void
on_keyserver_search_ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	SeahorseOperation *op;
	KeyserverSelection *selection;
	const gchar *search;
	GtkWidget *widget;

	widget = seahorse_widget_get_widget (swidget, "search-text");
	g_return_if_fail (widget != NULL);

	/* Get search text and save it for next time */
	search = gtk_entry_get_text (GTK_ENTRY (widget));
	g_return_if_fail (search != NULL && search[0] != 0);
	g_settings_set_string (seahorse_context_settings (NULL), "last-search-text", search);

	/* The keyservers to search, and save for next time */
	selection = get_keyserver_selection (swidget);
	g_return_if_fail (selection->uris != NULL);
	g_settings_set_strv (seahorse_context_settings (NULL), "last-search-servers",
	                     selection->all ? NULL : (const gchar * const*)selection->names->pdata);

	op = seahorse_context_search_remote (SCTX_APP(), search);
	if (op == NULL)
		return;

	/* Open the new result window */
	seahorse_keyserver_results_show (op,
	                                 GTK_WINDOW (seahorse_widget_get_widget (swidget, swidget->name)),
	                                 search);

	free_keyserver_selection (selection);
	seahorse_widget_destroy (swidget);
}

/**
* widget: ignored
* swidget: the SeahorseWidget to remove the signals from
*
* Disconnects the added/removed signals
*
**/
static void
cleanup_signals (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseServiceDiscovery *ssd = seahorse_context_get_discovery (SCTX_APP());
    g_signal_handlers_disconnect_by_func (ssd, refresh_shared_keys, swidget);
}


/**
 * seahorse_keyserver_search_show:
 * @parent: the parent window to connect this window to
 *
 * Shows a remote search window.
 *
 * Returns: the new window.
 */
GtkWindow*
seahorse_keyserver_search_show (GtkWindow *parent)
{
	SeahorseServiceDiscovery *ssd;
	SeahorseWidget *swidget;
	GtkWindow *window;
	GtkWidget *widget;
	GSettings *settings;
	gchar *search;

	swidget = seahorse_widget_new ("keyserver-search", parent);
	g_return_val_if_fail (swidget != NULL, NULL);

	window = GTK_WINDOW (seahorse_widget_get_widget (swidget, swidget->name));

	widget = seahorse_widget_get_widget (swidget, "search-text");
	g_return_val_if_fail (widget != NULL, window);

	search = g_settings_get_string (seahorse_context_settings (NULL),
	                                "last-search-text");
	if (search != NULL) {
		gtk_entry_set_text (GTK_ENTRY (widget), search);
		gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);
		g_free (search);
	}

	/* The key servers to list */
	settings = seahorse_context_pgp_settings (NULL);
	on_settings_keyservers_changed (settings, "keyservers", swidget);
	g_signal_connect_object (settings, "changed::keyservers",
	                         G_CALLBACK (on_settings_keyservers_changed), swidget, 0);

	/* Any shared keys to list */
	ssd = seahorse_context_get_discovery (NULL);
	refresh_shared_keys (ssd, NULL, swidget);
	g_signal_connect (ssd, "added", G_CALLBACK (refresh_shared_keys), swidget);
	g_signal_connect (ssd, "removed", G_CALLBACK (refresh_shared_keys), swidget);
	g_signal_connect (window, "destroy", G_CALLBACK (cleanup_signals), swidget);

	select_inital_keyservers (swidget);
	on_keyserver_search_control_changed (NULL, swidget);

	return window;
}
