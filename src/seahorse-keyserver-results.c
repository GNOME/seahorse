/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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
#include <gnome.h>

#include "seahorse-gpgmex.h"
#include "seahorse-windows.h"
#include "seahorse-widget.h"
#include "seahorse-preferences.h"
#include "seahorse-util.h"
#include "seahorse-keyserver-op.h"
#include "seahorse-operation.h"
#include "seahorse-key.h"
#include "seahorse-key-manager-store.h"
#include "seahorse-key-widget.h"
#include "seahorse-key-dialogs.h"

#define KEY_LIST "key_list"

/* Quits seahorse */
static void
close_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    seahorse_widget_destroy (swidget);
}

static void
search_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    seahorse_keyserver_search_show (swidget->sctx);
}

/* Loads key properties if a key is selected */
static void
properties_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (
		glade_xml_get_widget (swidget->xml, KEY_LIST)));
	if (skey != NULL)
		seahorse_key_properties_new (swidget->sctx, skey);
}

static GHashTable*
split_keys_by_server (GList *keys)
{
    SeahorseKeySource *sksrc;
    SeahorseKey *skey;
    GHashTable *tbl;
    gchar *server;
    GList *k;
    
    tbl = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    for (; keys; keys = g_list_next (keys)) {
        
        skey = SEAHORSE_KEY (keys->data);
        sksrc = seahorse_key_get_source (skey);
        
        server = NULL;
        g_object_get (sksrc, "key-server", &server, NULL);
        
        if (server == NULL)
            continue;
      
        k = (GList*)g_hash_table_lookup (tbl, server);
        k = g_list_prepend (k, skey);
        g_hash_table_replace (tbl, server, k);
    }   
    
    return tbl;
}

static void
import_done (SeahorseOperation *operation, SeahorseKeySource *sksrc)
{
    GError *err = NULL;
    g_return_if_fail (SEAHORSE_IS_OPERATION (operation));
 
    if (!seahorse_operation_is_successful (operation)) {
        seahorse_operation_steal_error (operation, &err);
        seahorse_util_handle_gerror (err, _("Couldn't import keys from key server"));
    }
}

static void
import_keyserver_keys (gchar *server, GList *keys, SeahorseKeySource *sksrc)
{
    SeahorseOperation *operation;
    
    g_return_if_fail (server && server[0]);
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc));
    
    if (keys == NULL)
        return;
    
    /* TODO: We really should be doing something with the operation
     * like monitoring progress etc... */
    operation = seahorse_keyserver_op_import (sksrc, keys, server);
    g_signal_connect (operation, "done", G_CALLBACK (import_done), sksrc);
    g_object_unref (operation);
}

static void
import_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    GHashTable *keys_by_server;
    SeahorseKeySource *sksrc;
    GList *keys;
    
    keys = seahorse_key_store_get_selected_keys (GTK_TREE_VIEW (
                glade_xml_get_widget (swidget->xml, KEY_LIST)));

    /* No keys, nothing to do */
    if (keys == NULL)
        return;

    /* Returns a hash table of lists of keys, indexed on server name */                        
    keys_by_server = split_keys_by_server (keys);
    g_return_if_fail (keys_by_server != NULL);

    g_list_free (keys);
    
    /* The default key source */
    sksrc = seahorse_context_get_key_source (swidget->sctx);
    
    /* This frees the individual lists in the hash table */
    g_hash_table_foreach (keys_by_server, (GHFunc)import_keyserver_keys, sksrc);
    g_hash_table_destroy (keys_by_server);
}

static void
expand_all_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	gtk_tree_view_expand_all (GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, KEY_LIST)));
}

static void
collapse_all_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	gtk_tree_view_collapse_all (GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, KEY_LIST)));
}

/* Loads key properties of activated key */
static void
row_activated (GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *arg2, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_key_from_path (GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, KEY_LIST)), path);
	if (skey != NULL)
		seahorse_key_properties_new (swidget->sctx, skey);
}

static void
set_key_options_sensitive (SeahorseWidget *swidget, gboolean selected, gboolean secret, SeahorseKey *skey)
{
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "properties"), selected);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_properties"), selected);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "properties_button"), selected);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "import"), selected);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_import"), selected);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "import_button"), selected);
}

static void
selection_changed (GtkTreeSelection *selection, SeahorseWidget *swidget)
{
	gint rows = 0;
	gboolean selected = FALSE, secret = FALSE;
	SeahorseKey *skey = NULL;
	
	rows = gtk_tree_selection_count_selected_rows (selection);
	selected = rows > 0;
	
	if (selected) {
		GnomeAppBar *status;
		
		status = GNOME_APPBAR (glade_xml_get_widget (swidget->xml, "status"));
		gnome_appbar_set_status (status, g_strdup_printf (_("Selected %d keys"), rows));
	}
	
	if (rows == 1) {
		skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (
			glade_xml_get_widget (swidget->xml, KEY_LIST)));
		secret = (skey != NULL && SEAHORSE_IS_KEY_PAIR (skey));
	}
	
	set_key_options_sensitive (swidget, selected, secret, skey);
}

static void
show_context_menu (SeahorseWidget *swidget, guint button, guint32 time)
{
	GtkWidget *menu;
	
	menu = glade_xml_get_widget (swidget->xml, "context_menu");
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, time);
	gtk_widget_show (menu);
}

static gboolean
key_list_button_pressed (GtkWidget *widget, GdkEventButton *event, SeahorseWidget *swidget)
{
	if (event->button == 3)
		show_context_menu (swidget, event->button, event->time);
	
	return FALSE;
}

static gboolean
key_list_popup_menu (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (widget));
	if (skey != NULL)
		show_context_menu (swidget, 0, gtk_get_current_event_time ());
       
    return FALSE;
}

static void
show_progress (SeahorseKeySource *sksrc, const gchar *op, gdouble fract, 
               SeahorseWidget *swidget)
{
    GnomeAppBar *status;
    GtkProgressBar *progress;

    status = GNOME_APPBAR (glade_xml_get_widget (swidget->xml, "status"));
    
    if (op != NULL)
        gnome_appbar_set_status (status, op);
    
    progress = gnome_appbar_get_progress (status);
    
    /* Progress */
    if (fract <= 1 && fract > 0) {
        gtk_progress_bar_set_fraction (progress, fract);
        
    /* Note that for our purposes, we totally ignore fract */
    } else {
       
        if (seahorse_key_source_get_state (sksrc) & SEAHORSE_KEY_SOURCE_LOADING) {
            gtk_progress_bar_set_pulse_step (progress, 0.05);
            gtk_progress_bar_pulse (progress);
        } else {
            gtk_progress_bar_set_fraction (progress, 0);
        }
        
    }
}

static void 
window_destroyed (GtkWindow *window, SeahorseWidget *swidget)
{
    SeahorseKeySource *sksrc = 
        SEAHORSE_KEY_SOURCE (g_object_get_data (G_OBJECT (swidget), "key-source"));
        
    seahorse_key_source_stop (sksrc);
    g_signal_handlers_disconnect_by_func (sksrc, show_progress, swidget);
    g_object_unref (sksrc);
}

/**
 * seahorse_keyserver_results_show
 * @sctx: The SeahorseContext 
 * @sksrc: The SeahorseKeySource with the remote keys 
 * @search: The search text (for display in titles and such).
 * 
 * Returns the new window created.
 **/
GtkWindow* 
seahorse_keyserver_results_show (SeahorseContext *sctx, SeahorseKeySource *sksrc,
                                 const gchar *search)
{
    GtkWindow *win;
	SeahorseWidget *swidget;
	GtkTreeView *view;
	GtkTreeSelection *selection;
    SeahorseKeyStore *skstore;
    gchar *title;
	
	swidget = seahorse_widget_new_allow_multiple ("keyserver-results", sctx);
    g_return_val_if_fail (swidget != NULL, NULL);
    
    g_signal_connect (sksrc, "progress", G_CALLBACK (show_progress), swidget);
    
    win = GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name));
    g_return_val_if_fail (win != NULL, NULL);

    g_object_set_data (G_OBJECT (swidget), "key-source", sksrc);
    g_signal_connect (win, "destroy", G_CALLBACK (window_destroyed), swidget);
    
    if (!search)
        title = g_strdup (_("Remote Keys"));
    else 
        title = g_strdup_printf(_("Remote Keys Containing '%s'"), search);
    gtk_window_set_title (win, title);
    g_free (title);
    
	/* construct key context menu */
	glade_xml_construct (swidget->xml, SEAHORSE_GLADEDIR "seahorse-keyserver-results.glade",
		                 "context_menu", NULL);
	set_key_options_sensitive (swidget, FALSE, FALSE, NULL);
	
	glade_xml_signal_connect_data (swidget->xml, "expand_all_activate",
		G_CALLBACK (expand_all_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "collapse_all_activate",
		G_CALLBACK (collapse_all_activate), swidget);
    glade_xml_signal_connect_data (swidget->xml, "close_activate",
        G_CALLBACK (close_activate), swidget);
    glade_xml_signal_connect_data (swidget->xml, "search_activate",
        G_CALLBACK (search_activate), swidget);
    glade_xml_signal_connect_data (swidget->xml, "import_activate",
        G_CALLBACK (import_activate), swidget);

    /* Features not yet available */
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "export"), FALSE);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_export"), FALSE);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "export_button"), FALSE);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "copy"), FALSE);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "edit_copy"), FALSE);
		
	/* tree view signals */	
	glade_xml_signal_connect_data (swidget->xml, "row_activated",
		G_CALLBACK (row_activated), swidget);
	glade_xml_signal_connect_data (swidget->xml, "key_list_button_pressed",
		G_CALLBACK (key_list_button_pressed), swidget);
	glade_xml_signal_connect_data (swidget->xml, "key_list_popup_menu",
		G_CALLBACK (key_list_popup_menu), swidget);
	/* selected key signals */
	glade_xml_signal_connect_data (swidget->xml, "properties_activate",
		G_CALLBACK (properties_activate), swidget);

	/* init key list & selection settings */
	view = GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, KEY_LIST));
	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (selection, "changed",
		G_CALLBACK (selection_changed), swidget);

    skstore = seahorse_key_manager_store_new (sksrc, view);
	selection_changed (selection, swidget);
   
    return win;
}
