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
#include "seahorse-operation.h"
#include "seahorse-key.h"
#include "seahorse-op.h"
#include "seahorse-operation.h"
#include "seahorse-progress.h"
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
		glade_xml_get_widget (swidget->xml, KEY_LIST)), NULL);
	if (skey != NULL)
		seahorse_key_properties_new (swidget->sctx, skey);
}

static void
import_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKeySource *sksrc;
    gpgme_error_t gerr;
    gpgme_data_t data;
    GError *err = NULL;
    gchar *text;
    GList *keys;
    guint n;
    
    keys = seahorse_key_store_get_selected_keys (GTK_TREE_VIEW (
                glade_xml_get_widget (swidget->xml, KEY_LIST)));

    /* No keys, nothing to do */
    if (keys == NULL)
        return;

    text = seahorse_op_export_text (keys, FALSE, &err);
    g_list_free (keys);
    if (text == NULL) {
        seahorse_util_handle_error (err, _("Couldn't retrieve key data from key server"));
        return;
    } 
    
    gerr = gpgme_data_new_from_mem (&data, text, strlen (text), 0);
    g_return_if_fail (GPG_IS_OK (gerr));

    /* The default key source */
    sksrc = seahorse_context_get_key_source (swidget->sctx);
    g_return_if_fail (sksrc != NULL);
    
    n = seahorse_op_import_text (sksrc, text, &err);
    if (n == -1) 
        seahorse_util_handle_error (err, _("Couldn't import keys into keyring"));

    gpgme_data_release (data);
    g_free (text);
} 

static void
export_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    GtkWidget *dialog;
    gchar* uri = NULL;
    GError *err = NULL;
    GList *keys;
     
    keys = seahorse_key_store_get_selected_keys (GTK_TREE_VIEW (
                glade_xml_get_widget (swidget->xml, KEY_LIST)));

    /* No keys, nothing to do */
    if (keys == NULL)
        return;
        
    dialog = seahorse_util_chooser_save_new (_("Save Remote Keys"), 
                GTK_WINDOW(glade_xml_get_widget (swidget->xml, "keyserver-results")));
    seahorse_util_chooser_show_key_files (dialog);
    seahorse_util_chooser_set_filename (dialog, keys);
                
    uri = seahorse_util_chooser_save_prompt (dialog);

    if(uri) {
        if (!seahorse_op_export_file (keys, FALSE, uri, &err))
            seahorse_util_handle_error (err, _("Couldn't export key to \"%s\""),
                                        seahorse_util_uri_get_last (uri));
    }      

    g_free (uri);
    g_list_free (keys);
}

/* Copies key to clipboard */
static void
copy_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    GdkAtom atom;
    GtkClipboard *board;
    gchar *text;
    GError *err = NULL;
    GList *keys;
  
    keys = seahorse_key_store_get_selected_keys (GTK_TREE_VIEW (
                glade_xml_get_widget (swidget->xml, KEY_LIST)));
       
    if (g_list_length (keys) == 0)
        return;
               
    text = seahorse_op_export_text (keys, FALSE, &err);

    if (text == NULL)
        seahorse_util_handle_error (err, _("Couldn't retrieve key data from key server"));
    else {
        atom = gdk_atom_intern ("CLIPBOARD", FALSE);
        board = gtk_clipboard_get (atom);
        gtk_clipboard_set_text (board, text, strlen (text));
        g_free (text);
    }
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
	
	skey = seahorse_key_store_get_key_from_path (GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, KEY_LIST)), path, NULL);
	if (skey != NULL)
		seahorse_key_properties_new (swidget->sctx, skey);
}

static void
set_key_options_sensitive (SeahorseWidget *swidget, guint selected, gboolean secret, SeahorseKey *skey)
{
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "properties"), selected == 1);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_properties"), selected == 1);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "properties_button"), selected == 1);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "import"), selected > 0);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_import"), selected > 0);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "import_button"), selected > 0);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "export"), selected > 0);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_export"), selected > 0);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "export_button"), selected > 0);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "copy"), selected > 0);
    gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "edit_copy"), selected > 0);
}

static void
selection_changed (GtkTreeSelection *selection, SeahorseWidget *swidget)
{
	gint rows = 0;
	gboolean secret = FALSE;
	SeahorseKey *skey = NULL;
	
	rows = gtk_tree_selection_count_selected_rows (selection);
	
	if (rows > 0) {
		GnomeAppBar *status;
		
		status = GNOME_APPBAR (glade_xml_get_widget (swidget->xml, "status"));
		gnome_appbar_set_status (status, g_strdup_printf (_("Selected %d keys"), rows));
	}
	
	if (rows == 1) {
		skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (
			glade_xml_get_widget (swidget->xml, KEY_LIST)), NULL);
		secret = (skey != NULL && SEAHORSE_IS_KEY_PAIR (skey));
	}
	
	set_key_options_sensitive (swidget, rows, secret, skey);
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
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (widget), NULL);
	if (skey != NULL)
		show_context_menu (swidget, 0, gtk_get_current_event_time ());
       
    return FALSE;
}

static void 
window_destroyed (GtkWindow *window, SeahorseWidget *swidget)
{
    SeahorseKeySource *sksrc = 
        SEAHORSE_KEY_SOURCE (g_object_get_data (G_OBJECT (swidget), "key-source"));
        
    seahorse_key_source_stop (sksrc);
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
    SeahorseOperation *operation;
	SeahorseWidget *swidget;
	GtkTreeView *view;
	GtkTreeSelection *selection;
    SeahorseKeyStore *skstore;
    GtkWidget *w;
    gchar *title;
	
	swidget = seahorse_widget_new_allow_multiple ("keyserver-results", sctx);
    g_return_val_if_fail (swidget != NULL, NULL);
    
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
	set_key_options_sensitive (swidget, 0, FALSE, NULL);
	
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
    glade_xml_signal_connect_data (swidget->xml, "export_activate",
        G_CALLBACK (export_activate), swidget);
    glade_xml_signal_connect_data (swidget->xml, "copy_activate",
        G_CALLBACK (copy_activate), swidget);

    /* Features not yet available */
		
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

    /* Load the keys */
    seahorse_key_source_refresh_async (sksrc, SEAHORSE_KEY_SOURCE_ALL);

    /* Hook progress bar in */
    operation = seahorse_key_source_get_operation (sksrc);
    g_return_val_if_fail (operation != NULL, win);
    
    w = glade_xml_get_widget (swidget->xml, "status");
    seahorse_progress_appbar_add_operation (w, operation);

    skstore = seahorse_key_manager_store_new (sksrc, view);
	selection_changed (selection, swidget);
   
    return win;
}
