/*
 * Seahorse
 *
 * Copyright (C) 2004 - 2005 Nate Nielsen
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
#include "seahorse-pgp-key.h"
#include "seahorse-operation.h"
#include "seahorse-progress.h"
#include "seahorse-key-manager-store.h"
#include "seahorse-key-widget.h"
#include "seahorse-key-dialogs.h"
#include "seahorse-vfs-data.h"

#define KEY_LIST "key_list"

/* SIGNAL CALLBACKS --------------------------------------------------------- */

static void
close_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    seahorse_widget_destroy (swidget);
}

static void
search_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    seahorse_keyserver_search_show ();
}

/* Loads key properties if a key is selected */
static void
properties_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    
    skey = seahorse_key_manager_store_get_selected_key (GTK_TREE_VIEW (
                        glade_xml_get_widget (swidget->xml, KEY_LIST)), NULL);
    
    /* TODO: Handle multiple types of keys here */
    if (skey != NULL && SEAHORSE_IS_PGP_KEY (skey))
        seahorse_key_properties_new (SEAHORSE_PGP_KEY (skey));
}

static void
set_numbered_status (SeahorseWidget *swidget, const gchar *t1, const gchar *t2, guint num)
{
    GtkStatusbar *status;
    guint id;
    gchar *msg;
    
    status = GTK_STATUSBAR (seahorse_widget_get_widget (swidget, "status"));
    g_return_if_fail (status != NULL);
    
    id = gtk_statusbar_get_context_id (status, "key-manager");
    gtk_statusbar_pop (status, id);
    
    msg = g_strdup_printf (ngettext (t1, t2, num), num);
    gtk_statusbar_push (status, id, msg);
    g_free (msg);
}

static void
import_done (SeahorseOperation *op, SeahorseWidget *swidget)
{
    GError *err = NULL;
    
    /* 
     * TODO: We should display the number of keys actually 
     * imported, but that's hard to do 
     */
    
    if (!seahorse_operation_is_successful (op)) {
        seahorse_operation_copy_error (op, &err);
        seahorse_util_handle_error (err, _("Couldn't import keys into keyring"));
    }
}

static void
import_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseOperation *op;
    GList *keys;
    
    keys = seahorse_key_manager_store_get_selected_keys (GTK_TREE_VIEW (
                glade_xml_get_widget (swidget->xml, KEY_LIST)));

    /* No keys, nothing to do */
    if (keys == NULL)
        return;
    
    op = seahorse_context_transfer_keys (SCTX_APP (), keys, NULL);
    g_return_if_fail (op != NULL);
    
    if (seahorse_operation_is_running (op)) {
        seahorse_progress_show (op, _("Importing keys from key servers"), TRUE);
        seahorse_operation_watch (op, G_CALLBACK (import_done), NULL, swidget);
    }
    
    /* Running operation refs itself */
    g_object_unref (op);
} 

static void
export_done (SeahorseOperation *op, SeahorseWidget *swidget)
{
    GError *err = NULL;
    gchar *uri;

    uri = g_object_get_data (G_OBJECT (op), "export-uri");
    g_return_if_fail (uri != NULL);
    
    if (!seahorse_operation_is_successful (op)) {
        seahorse_operation_copy_error (op, &err);
        seahorse_util_handle_error (err, _("Couldn't write keys to file: %s"), 
                                           seahorse_util_uri_get_last (uri));
    }
    
}

static void
export_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseOperation *op;
    GtkWidget *dialog;
    gchar* uri = NULL;
    GError *err = NULL;
    gpgme_data_t data;
    GList *keys;
     
    keys = seahorse_key_manager_store_get_selected_keys (GTK_TREE_VIEW (
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
        
        data = seahorse_vfs_data_create (uri, SEAHORSE_VFS_WRITE, &err);
        if (!data) {
            seahorse_util_handle_error (err, _("Couldn't write keys to file: %s"), 
                                        seahorse_util_uri_get_last (uri));
            g_free (uri);
            
        } else { 
            
            op = seahorse_key_source_export_keys (keys, data);
            g_return_if_fail (op != NULL);
        
            g_object_set_data_full (G_OBJECT (op), "export-uri", uri, g_free);
            g_object_set_data_full (G_OBJECT (op), "export-data", data, 
                                    (GDestroyNotify)gpgmex_data_release);
    
            seahorse_progress_show (op, _("Retrieving keys"), TRUE);
            seahorse_operation_watch (op, G_CALLBACK (export_done), NULL, swidget);
        
            /* Running operation refs itself */
            g_object_unref (op);
        }
    }
    
    /* uri is freed with op */
    g_list_free (keys);
}

static void
copy_done (SeahorseOperation *op, SeahorseWidget *swidget)
{
    GdkAtom atom;
    GtkClipboard *board;
    gchar *text;
    GError *err = NULL;
    gpgme_data_t data;
    guint num;
    
    if (!seahorse_operation_is_successful (op)) {
        seahorse_operation_copy_error (op, &err);
        seahorse_util_handle_error (err, _("Couldn't retrieve data from key server"));
    }
    
    data = (gpgme_data_t)seahorse_operation_get_result (op);
    g_return_if_fail (data != NULL);
    
    text = seahorse_util_write_data_to_text (data, NULL);
    g_return_if_fail (text != NULL);
    
    atom = gdk_atom_intern ("CLIPBOARD", FALSE);
    board = gtk_clipboard_get (atom);
    gtk_clipboard_set_text (board, text, strlen (text));
    g_free (text);

    num = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (op), "num-keys"));
    if (num > 0) {
        set_numbered_status (swidget, _("Copied %d key"), 
                                      _("Copied %d keys"), num);
    }
}

/* Copies key to clipboard */
static void
copy_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseOperation *op;
    GList *keys;
    guint num;
  
    keys = seahorse_key_manager_store_get_selected_keys (GTK_TREE_VIEW (
                glade_xml_get_widget (swidget->xml, KEY_LIST)));
       
    num = g_list_length (keys);
    if (num == 0)
        return;
    
    op = seahorse_key_source_export_keys (keys, NULL);
    g_return_if_fail (op != NULL);
    
    g_object_set_data (G_OBJECT (op), "num-keys", GINT_TO_POINTER (num));
    
    seahorse_progress_show (op, _("Retrieving keys"), TRUE);
    seahorse_operation_watch (op, G_CALLBACK (copy_done), NULL, swidget);
        
    /* Running operation refs itself */
    g_object_unref (op);
    g_list_free (keys);
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
    
    skey = seahorse_key_manager_store_get_key_from_path (GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, KEY_LIST)), path, NULL);
    if (skey != NULL && SEAHORSE_IS_PGP_KEY (skey))
        seahorse_key_properties_new (SEAHORSE_PGP_KEY (skey));
}

static void
selection_changed (GtkTreeSelection *selection, SeahorseWidget *swidget)
{
    GtkActionGroup *actions;
	gint rows = 0;
	
	rows = gtk_tree_selection_count_selected_rows (selection);
	
	if (rows > 0) {
        set_numbered_status (swidget, _("Selected %d key"), 
                                      _("Selected %d keys"), rows);
	}
	
    actions = seahorse_widget_find_actions (swidget, "key");
    gtk_action_group_set_sensitive (actions, rows > 0);
}

static void
show_context_menu (SeahorseWidget *swidget, guint button, guint32 time)
{
	GtkWidget *menu;
    
    menu = seahorse_widget_get_ui_widget (swidget, "/KeyPopup");
    g_return_if_fail (menu != NULL);    
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
    
    skey = seahorse_key_manager_store_get_selected_key (GTK_TREE_VIEW (widget), NULL);
    if (skey != NULL)
        show_context_menu (swidget, 0, gtk_get_current_event_time ());
       
    return FALSE;
}

static void
operation_done (SeahorseOperation *op, SeahorseWidget *swidget)
{
    g_object_set_data (G_OBJECT (swidget), "operation", NULL);
}

static void 
window_destroyed (GtkWindow *window, SeahorseWidget *swidget)
{
    SeahorseOperation *op = 
        SEAHORSE_OPERATION (g_object_get_data (G_OBJECT (swidget), "operation"));
    
    if (op && seahorse_operation_is_running (op))
        seahorse_operation_cancel (op);
}

static void
help_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    seahorse_widget_show_help (swidget);
}

/* BUILDING THE MAIN WINDOW ------------------------------------------------- */

static const GtkActionEntry ui_entries[] = {
    
    /* Top menu items */
    { "key-menu", NULL, N_("_Key") },
    { "edit-menu", NULL, N_("_Edit") },
    { "help-menu", NULL, N_("_Help") },
    
    { "app-close", GTK_STOCK_CLOSE, N_("_Close"), "<control>W",
            N_("Close this window"), G_CALLBACK (close_activate) }, 
    { "help-show", GTK_STOCK_HELP, N_("_Contents"), "F1",
            N_("Show Seahorse help"), G_CALLBACK (help_activate) }, 

    { "view-expand-all", GTK_STOCK_ADD, N_("_Expand All"), NULL,
            N_("Expand all listings"), G_CALLBACK (expand_all_activate) }, 
    { "view-collapse-all", GTK_STOCK_REMOVE, N_("_Collapse All"), NULL,
            N_("Collapse all listings"), G_CALLBACK (collapse_all_activate) }, 
};

static const GtkActionEntry public_entries[] = {
    { "key-import-keyring", GTK_STOCK_ADD, N_("_Import"), "",
            N_("Import selected keys to local keyring"), G_CALLBACK (import_activate) },
    { "key-properties", GTK_STOCK_PROPERTIES, N_("P_roperties"), NULL,
            N_("Show key properties"), G_CALLBACK (properties_activate) }, 
    { "key-export-file", GTK_STOCK_SAVE_AS, N_("Save As..."), "<control><shift>S",
            N_("Save selected keys as a file"), G_CALLBACK (export_activate) }, 
    { "key-export-clipboard", GTK_STOCK_COPY, N_("_Copy Key"), "<control>C",
            N_("Copy selected keys to the clipboard"), G_CALLBACK (copy_activate) }, 
};

static const GtkActionEntry remote_entries[] = {
    { "remote-find", GTK_STOCK_FIND, N_("_Find Remote Keys..."), "",
            N_("Search for keys on a key server"), G_CALLBACK (search_activate) }, 
};

gboolean 
filter_keyset (SeahorseKey *skey, const gchar* search)
{
    gboolean match = FALSE;
    gchar *name, *t;

    name = seahorse_key_get_display_name (skey);
    t = g_utf8_casefold (name, -1);
    g_free (name);
    
    if (strstr (t, search))
        match = TRUE;
    
    g_free (t);
    return match;
}

/**
 * seahorse_keyserver_results_show
 * @sksrc: The SeahorseKeySource with the remote keys 
 * @search: The search text (for display in titles and such).
 * 
 * Returns the new window created.
 **/
GtkWindow* 
seahorse_keyserver_results_show (SeahorseOperation *op, const gchar *search)
{
    SeahorseKeyManagerStore *skstore;
    SeahorseKeyPredicate *pred;
    GtkWindow *win;
    SeahorseKeyset *skset;
    SeahorseWidget *swidget;
    GtkTreeView *view;
    GtkTreeSelection *selection;
    GtkActionGroup *actions;
    GtkAction *action;
    gchar *title, *t;
    
    swidget = seahorse_widget_new_allow_multiple ("keyserver-results");
    g_return_val_if_fail (swidget != NULL, NULL);
    
    win = GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name));
    g_return_val_if_fail (win != NULL, NULL);
    
    g_object_set_data (G_OBJECT (swidget), "operation", op);
    seahorse_operation_watch (op, G_CALLBACK (operation_done), NULL, swidget);
    g_signal_connect (win, "destroy", G_CALLBACK (window_destroyed), swidget);

    if (!search)
        title = g_strdup (_("Remote Keys"));
    else 
        title = g_strdup_printf(_("Remote Keys Containing '%s'"), search);
    gtk_window_set_title (win, title);
    g_free (title);
    
    /* General normal actions */
    actions = gtk_action_group_new ("main");
    gtk_action_group_set_translation_domain (actions, NULL);
    gtk_action_group_add_actions (actions, ui_entries, 
                                  G_N_ELEMENTS (ui_entries), swidget);
    seahorse_widget_add_actions (swidget, actions);

    /* Actions that are allowed on all keys */
    actions = gtk_action_group_new ("key");
    gtk_action_group_set_translation_domain (actions, NULL);
    gtk_action_group_add_actions (actions, public_entries, 
                                  G_N_ELEMENTS (public_entries), swidget);
    seahorse_widget_add_actions (swidget, actions);

    /* Mark the properties toolbar button as important */
    action = gtk_action_group_get_action (actions, "key-properties");
    g_return_val_if_fail (action, win);
    g_object_set (action, "is-important", TRUE, NULL);
    
    /* Mark the import toolbar button as important */
    action = gtk_action_group_get_action (actions, "key-import-keyring");
    g_return_val_if_fail (action, win);
    g_object_set (action, "is-important", TRUE, NULL);
    
    actions = gtk_action_group_new ("remote");
    gtk_action_group_set_translation_domain (actions, NULL);
    gtk_action_group_add_actions (actions, remote_entries, 
                                  G_N_ELEMENTS (remote_entries), swidget);
    seahorse_widget_add_actions (swidget, actions);    

    /* tree view signals */	
    glade_xml_signal_connect_data (swidget->xml, "row_activated",
        G_CALLBACK (row_activated), swidget);
    glade_xml_signal_connect_data (swidget->xml, "key_list_button_pressed",
        G_CALLBACK (key_list_button_pressed), swidget);
    glade_xml_signal_connect_data (swidget->xml, "key_list_popup_menu",
        G_CALLBACK (key_list_popup_menu), swidget);

    /* init key list & selection settings */
    view = GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, KEY_LIST));
    selection = gtk_tree_view_get_selection (view);
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
    g_signal_connect (selection, "changed",
        G_CALLBACK (selection_changed), swidget);

    /* Set focus to the current key list */
    gtk_widget_grab_focus (GTK_WIDGET (view));

    /* To avoid flicker */
    seahorse_widget_show (swidget);

    /* Hook progress bar in */
    seahorse_progress_status_set_operation (swidget, op);
    
    /* Our predicate for filtering keys */
    pred = g_new0 (SeahorseKeyPredicate, 1);
    pred->ktype = SKEY_PGP;
    pred->etype = SKEY_PUBLIC;
    pred->location = SKEY_LOC_REMOTE;
    pred->custom = (SeahorseKeyPredFunc)filter_keyset;
    pred->custom_data = t = g_utf8_casefold (search, -1);
  
    /* Our keyset all nicely filtered */
    skset = seahorse_keyset_new_full (pred);

    /* Free these when the keyset goes away */
    g_object_set_data_full (G_OBJECT (skset), "search-text", t, g_free);
    g_object_set_data_full (G_OBJECT (skset), "predicate", pred, g_free);
    
    /* A new key store only showing public keys */
    skstore = seahorse_key_manager_store_new (skset, view);
    g_object_unref (skset);
    
    selection_changed (selection, swidget);

    return win;
}
