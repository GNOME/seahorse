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
#include "config.h"

#include <glib/gi18n.h>

#include "seahorse-combo-keys.h"
#include "seahorse-gtkstock.h"
#include "seahorse-keyserver-control.h"
#include "seahorse-prefs.h"
#include "seahorse-registry.h"
#include "seahorse-servers.h"
#include "seahorse-util.h"
#include "seahorse-widget.h"

/* From seahorse-prefs-cache.c */
void seahorse_prefs_cache (SeahorseWidget *widget);

/* Key Server Prefs --------------------------------------------------------- */

#ifdef WITH_KEYSERVER

#define UPDATING_MODEL    "updating"

enum 
{
    KEYSERVER_COLUMN,
    N_COLUMNS
};


/* Called when a cell has completed being edited */
static void
keyserver_cell_edited (GtkCellRendererText *cell, gchar *path, gchar *text,
                       GtkTreeModel *model)
{
    GtkTreeIter iter;
    gboolean ret;
    
    if (!seahorse_servers_is_valid_uri (text)) {
        seahorse_util_show_error (NULL, _("Not a valid Key Server address."), 
                                  _("For help contact your system adminstrator or the administrator of the key server." ));
        return;
    }
    
    ret = gtk_tree_model_get_iter_from_string (model, &iter, path);
    g_assert (ret);
    gtk_tree_store_set (GTK_TREE_STORE (model), &iter, KEYSERVER_COLUMN, text, -1);
}

/* The selection changed on the tree */
static void
keyserver_sel_changed (GtkTreeSelection *selection, SeahorseWidget *swidget)
{
    GtkWidget *widget = seahorse_widget_get_widget (swidget, "keyserver_remove");
    gtk_widget_set_sensitive (widget, (gtk_tree_selection_count_selected_rows (selection) > 0));
}

/* Callback to remove selected rows */
static void
remove_row (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    gtk_tree_store_remove (GTK_TREE_STORE (model), iter);
}

/* User wants to remove selected rows */
G_MODULE_EXPORT void
on_prefs_keyserver_remove_clicked (GtkWidget *button, SeahorseWidget *swidget)
{
    GtkTreeView *treeview;
    GtkTreeSelection *selection;
    
    treeview = GTK_TREE_VIEW (seahorse_widget_get_widget (swidget, "keyservers"));
    selection = gtk_tree_view_get_selection (treeview);
    gtk_tree_selection_selected_foreach (selection, remove_row, NULL);
}

/* Write key server list to settings */
static void
save_keyservers (GtkTreeModel *model)
{
	GPtrArray *values;
	GtkTreeIter iter;
	gchar *keyserver;

	values = g_ptr_array_new_with_free_func (g_free);
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gtk_tree_model_get (model, &iter, KEYSERVER_COLUMN, &keyserver, -1);
			g_return_if_fail (keyserver != NULL);
			g_ptr_array_add (values, keyserver);
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	g_ptr_array_add (values, NULL);
	g_settings_set_strv (seahorse_context_pgp_settings (NULL), "keyservers",
	                     (const gchar* const*)values->pdata);
	g_ptr_array_free (values, TRUE);
}

/* Called when values in a row changes */
static void
keyserver_row_changed (GtkTreeModel *model, GtkTreePath *arg1, 
                       GtkTreeIter *arg2, gpointer user_data)
{
    /* If currently updating (see populate_keyservers) ignore */
    if (g_object_get_data (G_OBJECT (model), UPDATING_MODEL) != NULL)
        return;

    save_keyservers (model); 
}

/* Called when a row is removed */
static void 
keyserver_row_deleted (GtkTreeModel *model, GtkTreePath *arg1,
                       gpointer user_data)
{
    /* If currently updating (see populate_keyservers) ignore */
    if (g_object_get_data (G_OBJECT (model), UPDATING_MODEL) != NULL)
        return;
        
    save_keyservers (model);
}

/* Fill in the list with values in ks */
static void
populate_keyservers (SeahorseWidget *swidget, gchar **keyservers)
{
    GtkTreeView *treeview;
    GtkTreeStore *store;
    GtkTreeModel *model;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeIter iter;
    gboolean cont;
    gchar *value;
    guint i = 0;

    treeview = GTK_TREE_VIEW (seahorse_widget_get_widget (swidget, "keyservers"));
    model = gtk_tree_view_get_model (treeview);
    store = GTK_TREE_STORE (model);
    
    /* This is our first time so create a store */
    if (!model) {

        store = gtk_tree_store_new (N_COLUMNS, G_TYPE_STRING);
        model = GTK_TREE_MODEL (store);
        gtk_tree_view_set_model (treeview, model);

        /* Make the column */
        renderer = gtk_cell_renderer_text_new ();
        g_object_set (renderer, "editable", TRUE, NULL);
        g_signal_connect(renderer, "edited", G_CALLBACK (keyserver_cell_edited), store);
        column = gtk_tree_view_column_new_with_attributes (_("URL"), renderer , 
                                        "text", KEYSERVER_COLUMN, NULL);
        gtk_tree_view_append_column (treeview, column);        
    }

	/* Mark this so we can ignore events */
	g_object_set_data (G_OBJECT (model), UPDATING_MODEL, GINT_TO_POINTER (1));

	/* We try and be inteligent about updating so we don't throw
	 * away selections and stuff like that */

	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gtk_tree_model_get (model, &iter, KEYSERVER_COLUMN, &value, -1);
			if (keyservers[i] != NULL && value != NULL &&
			    g_utf8_collate (keyservers[i], value) == 0) {
				cont = gtk_tree_model_iter_next (model, &iter);
				i++;
			} else {
				cont = gtk_tree_store_remove (store, &iter);
			}
			g_free (value);
		} while (cont);
	}

	/* Any remaining extra rows */
	for ( ; keyservers[i] != NULL; i++) {
		gtk_tree_store_append (store, &iter, NULL);
		gtk_tree_store_set (store, &iter, KEYSERVER_COLUMN, keyservers[i], -1);
	}

	/* Done updating */
	g_object_set_data (G_OBJECT (model), UPDATING_MODEL, NULL);
}

static void
on_settings_keyserver_changed (GSettings *settings, const gchar *key, gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	gchar **keyservers;

	keyservers = g_settings_get_strv (settings, key);
	populate_keyservers (swidget, keyservers);
	g_strfreev (keyservers);
}

static gchar*
calculate_keyserver_uri (SeahorseWidget *swidget)
{
    const gchar *scheme = NULL;
    const gchar *host = NULL;
    const gchar *port = NULL;
    GtkWidget *widget;
    GList *types;
    gint active;
    gchar *uri;

    /* Figure out the scheme */
    widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "keyserver-type"));
    g_return_val_if_fail (widget != NULL, NULL);

	active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
	if (active >= 0) {
		types = g_object_get_data (G_OBJECT (swidget), "keyserver-types");
		scheme = (const gchar*)g_list_nth_data (types, active);
		if (scheme && !scheme[0])
			scheme = NULL;
	}

    /* The host */
    widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "keyserver-host"));
    g_return_val_if_fail (widget != NULL, NULL);
    
    host = gtk_entry_get_text (GTK_ENTRY (widget));
    g_return_val_if_fail (host != NULL, NULL);
    
    /* Custom URI? */
    if (scheme == NULL) {
        if (seahorse_servers_is_valid_uri (host))
            return g_strdup (host);
        return NULL;
    }
    
    /* The port */
    widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "keyserver-port"));
    g_return_val_if_fail (widget != NULL, NULL);
    
    port = gtk_entry_get_text (GTK_ENTRY (widget));
    if (port && !port[0])
        port = NULL;
    
    uri = g_strdup_printf("%s://%s%s%s", scheme, host, port ? ":" : "", port ? port : "");
    if (!seahorse_servers_is_valid_uri (uri)) {
        g_free (uri);
        uri = NULL;
    }

    return uri; 
}

G_MODULE_EXPORT void
on_prefs_add_keyserver_uri_changed (GtkWidget *button, SeahorseWidget *swidget)
{
    GtkWidget *widget;
    gchar **types;
    gchar *t;
    gint active;

    widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "ok"));
    g_return_if_fail (widget != NULL);
    
    t = calculate_keyserver_uri (swidget);
    gtk_widget_set_sensitive (widget, t != NULL);
    g_free (t);

    widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "keyserver-type"));
    g_return_if_fail (widget != NULL);

    /* Show or hide the port section based on whether 'custom' is selected */    
    active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
    if (active > -1) {
        types = g_object_get_data (G_OBJECT (swidget), "keyserver-types");

        widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "port-block"));
        g_return_if_fail (widget != NULL);

        if (types && types[active] && types[active][0])
            gtk_widget_show (widget);
        else
            gtk_widget_hide (widget);
    }
}

G_MODULE_EXPORT void
on_prefs_keyserver_add_clicked (GtkButton *button, SeahorseWidget *sw)
{
    SeahorseWidget *swidget;
    gchar **types;
    GtkWidget *widget;
    gint response;
    gchar *result = NULL;
    GtkTreeView *treeview;
    GtkTreeStore *store;
    GtkTreeIter iter;
    guint i;

    swidget = seahorse_widget_new_allow_multiple ("add-keyserver",
                                                  GTK_WINDOW (seahorse_widget_get_widget (sw, sw->name)));
    g_return_if_fail (swidget != NULL);
    
    widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "keyserver-type"));
    g_return_if_fail (widget != NULL);
    
    /* The list of types */
    types = seahorse_servers_get_types ();

    /* The description for the key server types, plus custom */
    for (i = 0; types[i] != NULL; i++)
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget),
                                        seahorse_servers_get_description (types[i]));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget),
                                    _("Custom"));
    gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (widget), 0);
    gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

    /* Save these away for later */
    g_object_set_data_full (G_OBJECT (swidget), "keyserver-types", types, 
                            (GDestroyNotify)g_strfreev);

    response = gtk_dialog_run (GTK_DIALOG (seahorse_widget_get_toplevel (swidget)));
    if (response == GTK_RESPONSE_ACCEPT) {
        result = calculate_keyserver_uri (swidget);
        if (result != NULL) {        
            
            treeview = GTK_TREE_VIEW (seahorse_widget_get_widget (sw, "keyservers"));
            g_return_if_fail (treeview != NULL);
            
            store = GTK_TREE_STORE (gtk_tree_view_get_model (treeview));
            gtk_tree_store_append (store, &iter, NULL);
            gtk_tree_store_set (store, &iter, KEYSERVER_COLUMN, result, -1);
        }
        
        g_free (result);
    }
        
    seahorse_widget_destroy (swidget);
}

/* Perform keyserver page initialization */
static void
setup_keyservers (SeahorseWidget *swidget)
{
	GtkTreeView *treeview;
	SeahorseKeyserverControl *skc;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkWidget *widget;
	gchar **keyservers;

	keyservers = seahorse_servers_get_uris ();
	populate_keyservers (swidget, keyservers);
	g_strfreev (keyservers);

	treeview = GTK_TREE_VIEW (seahorse_widget_get_widget (swidget, "keyservers"));
	model = gtk_tree_view_get_model (treeview);
	g_signal_connect (model, "row-changed", G_CALLBACK (keyserver_row_changed), swidget);
	g_signal_connect (model, "row-deleted", G_CALLBACK (keyserver_row_deleted), swidget);

	selection = gtk_tree_view_get_selection (treeview);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (selection, "changed", G_CALLBACK (keyserver_sel_changed), swidget);

	gtk_builder_connect_signals (swidget->gtkbuilder, swidget);
	g_signal_connect_object (seahorse_context_pgp_settings (NULL), "changed::keyserver",
	                         G_CALLBACK (on_settings_keyserver_changed), swidget, 0);

	widget = seahorse_widget_get_widget (swidget, "keyserver-publish");
	g_return_if_fail (widget != NULL);

	skc = seahorse_keyserver_control_new ("server-publish-to", _("None: Don't publish keys"));
	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (skc));
	gtk_widget_show_all (widget);

	widget = seahorse_widget_get_widget (swidget, "keyserver-publish-to-label");
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), GTK_WIDGET (skc));

	widget = seahorse_widget_get_widget (swidget, "auto_retrieve");
	g_return_if_fail (widget != NULL);
	g_settings_bind (seahorse_context_settings (NULL), "server-auto-retrieve",
	                 widget, "active", G_SETTINGS_BIND_DEFAULT);

	widget = seahorse_widget_get_widget (swidget, "auto_sync");
	g_return_if_fail (widget != NULL);
	g_settings_bind (seahorse_context_settings (NULL), "server-auto-publish",
	                 widget, "active", G_SETTINGS_BIND_DEFAULT);
}

#endif /* WITH_KEYSERVER */

/* -------------------------------------------------------------------------- */

/**
 * seahorse_prefs_new:
 * @parent: The #GtkWindow to set as the preferences dialog's parent
 * 
 * Create a new preferences window.
 * 
 * Returns: The preferences window.
 **/
SeahorseWidget *
seahorse_prefs_new (GtkWindow *parent)
{
    SeahorseWidget *swidget;

#if !(WITH_KEYSERVER && WITH_SHARING)
    GtkWidget *widget = NULL;
#endif

    swidget = seahorse_widget_new ("prefs", parent);
    
    if (swidget != NULL) {
    
#ifdef WITH_KEYSERVER
        setup_keyservers (swidget);
#else
        widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "keyserver-tab"));
        g_return_val_if_fail (GTK_IS_WIDGET (widget), swidget);
        seahorse_prefs_remove_tab (swidget, widget);
#endif

        seahorse_widget_show (swidget);
    } else {
        swidget = seahorse_widget_find ("prefs");
        gtk_window_present (GTK_WINDOW (seahorse_widget_get_widget (swidget, swidget->name)));
    }
    
    return swidget;
}

/**
 * seahorse_prefs_add_tab
 * @swidget: The preferences window
 * @label: Label for the tab to be added
 * @tab: Tab to be added
 * 
 * Add a tab to the preferences window
 **/
void                
seahorse_prefs_add_tab (SeahorseWidget *swidget, GtkWidget *label, GtkWidget *tab)
{
    GtkWidget *widget;
    widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "notebook"));
    gtk_widget_show (label);
    gtk_notebook_prepend_page (GTK_NOTEBOOK (widget), tab, label);
}

/**
 * seahorse_prefs_select_tab:
 * @swidget: The preferences window
 * @tab: The tab to be set active
 *
 * Sets the input tab to be the active one
 **/
void                
seahorse_prefs_select_tab (SeahorseWidget *swidget, GtkWidget *tab)
{
    GtkWidget *tabs;
    gint pos;
    
    g_return_if_fail (GTK_IS_WIDGET (tab));
    
    tabs = GTK_WIDGET (seahorse_widget_get_widget (swidget, "notebook"));
    g_return_if_fail (GTK_IS_NOTEBOOK (tabs));
    
    pos = gtk_notebook_page_num (GTK_NOTEBOOK (tabs), tab);
    if (pos != -1)
        gtk_notebook_set_current_page (GTK_NOTEBOOK (tabs), pos);
}

/**
 * seahorse_prefs_remove_tab:
 * @swidget: The preferences window
 * @tab: The tab to be removed
 *
 * Removes a tab from the preferences window
 **/
void 
seahorse_prefs_remove_tab (SeahorseWidget *swidget, GtkWidget *tab)
{
    GtkWidget *tabs;
    gint pos;
    
    g_return_if_fail (GTK_IS_WIDGET (tab));
    
    tabs = GTK_WIDGET (seahorse_widget_get_widget (swidget, "notebook"));
    g_return_if_fail (GTK_IS_NOTEBOOK (tabs));
    
    pos = gtk_notebook_page_num (GTK_NOTEBOOK (tabs), tab);
    if (pos != -1)
        gtk_notebook_remove_page (GTK_NOTEBOOK (tabs), pos);
}
