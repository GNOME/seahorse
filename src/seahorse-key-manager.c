/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2005 Nate Nielsen
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
#include <gconf/gconf-client.h>
#include <libgnomevfs/gnome-vfs.h>

#include "seahorse-gpgmex.h"
#include "seahorse-windows.h"
#include "seahorse-widget.h"
#include "seahorse-progress.h"
#include "seahorse-preferences.h"
#include "seahorse-operation.h"
#include "seahorse-util.h"
#include "seahorse-validity.h"
#include "seahorse-key-manager-store.h"
#include "seahorse-key-dialogs.h"
#include "seahorse-key-op.h"
#include "seahorse-key-widget.h"
#include "seahorse-op.h"
#include "seahorse-gpg-options.h"
#include "seahorse-gconf.h"

#define KEY_LIST "key_list"

#define SEC_RING "/secring.gpg"
#define PUB_RING "/pubring.gpg"

/* SIGNAL CALLBACKS --------------------------------------------------------- */

/* Quits seahorse */
static void
quit (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_context_destroy (swidget->sctx);
}

/* Quits seahorse */
static gboolean
delete_event (GtkWidget *widget, GdkEvent *event, SeahorseWidget *swidget)
{
	quit (widget, swidget);
	return TRUE;
}

/* Loads generate dialog */
static void
generate_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_generate_select_show (swidget->sctx);
}
/* Loads Key generation assistant for first time users */
static void
new_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_generate_druid_show (swidget->sctx);
}

static void
set_numbered_status (SeahorseWidget *swidget, const gchar *t1, const gchar *t2, guint num)
{
    GnomeAppBar *status;
    gchar *msg;
    
    msg = g_strdup_printf (ngettext (t1, t2, num), num);
    status = GNOME_APPBAR (glade_xml_get_widget (swidget->xml, "status"));
	gnome_appbar_set_default (status, msg);
    g_free (msg);
}

/* Loads import dialog */
static void
import_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKeySource *sksrc;
    GtkWidget *dialog;
    char* uri = NULL;
    GError *err = NULL;
    gint keys;
    
    sksrc = seahorse_context_get_key_source (swidget->sctx);
    g_return_if_fail (sksrc != NULL);
    
    dialog = seahorse_util_chooser_open_new (_("Import Key"), 
                GTK_WINDOW(glade_xml_get_widget (swidget->xml, "key-manager")));
    seahorse_util_chooser_show_key_files (dialog);

    uri = seahorse_util_chooser_open_prompt (dialog);
    if(uri) {
        keys = seahorse_op_import_file (sksrc, uri, &err);
        
        if (err != NULL)
            seahorse_util_handle_error (err, _("Couldn't import keys from \"%s\""), 
                seahorse_util_uri_get_last (uri));
        else 
            set_numbered_status (swidget, _("Imported %d key"), 
                                          _("Imported %d keys"), keys);

        g_free (uri);
    }
}

/* Callback for pasting from clipboard */
static void
clipboard_received (GtkClipboard *board, const gchar *text, SeahorseWidget *swidget)
{
    SeahorseKeySource *sksrc;
    GError *err = NULL;
    gint keys;
    
    sksrc = seahorse_context_get_key_source (swidget->sctx);
    g_return_if_fail (sksrc != NULL);
 
    keys = seahorse_op_import_text (sksrc, text, &err);
 
    if (err != NULL)
        seahorse_util_handle_error (err, _("Couldn't import keys from clipboard"));
    else
        set_numbered_status (swidget, _("Imported %d key"), 
                                      _("Imported %d keys"), keys);
}

/* Pastes key from keyboard */
static void 
paste_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    GdkAtom atom;
    GtkClipboard *board;
       
    atom = gdk_atom_intern ("CLIPBOARD", FALSE);
    board = gtk_clipboard_get (atom);
    gtk_clipboard_request_text (board,
         (GtkClipboardTextReceivedFunc)clipboard_received, swidget);
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
    guint num;
  
    keys = seahorse_key_store_get_selected_keys (GTK_TREE_VIEW (
                glade_xml_get_widget (swidget->xml, KEY_LIST)));
       
    num = g_list_length (keys);
    if (num == 0)
        return;
               
    text = seahorse_op_export_text (keys, FALSE, &err);

    if (text == NULL)
        seahorse_util_handle_error (err, _("Couldn't export key to clipboard"));
    else {
        atom = gdk_atom_intern ("CLIPBOARD", FALSE);
        board = gtk_clipboard_get (atom);
        gtk_clipboard_set_text (board, text, strlen (text));
        g_free (text);

        set_numbered_status (swidget, _("Copied %d key"), 
                                      _("Copied %d keys"), num);
    }
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

/* Loads export dialog if a key is selected */
static void
export_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    GtkWidget *dialog;
    gchar* uri = NULL;
    GError *err = NULL;
    GList *keys;

    keys = seahorse_key_store_get_selected_keys (GTK_TREE_VIEW (
                        glade_xml_get_widget (swidget->xml, KEY_LIST)));
    
    dialog = seahorse_util_chooser_save_new (_("Export Key"), 
                GTK_WINDOW(glade_xml_get_widget (swidget->xml, "key-manager")));
    seahorse_util_chooser_show_key_files (dialog);
    seahorse_util_chooser_set_filename (dialog, keys);
     
    uri = seahorse_util_chooser_save_prompt (dialog);
    if(uri) {

        seahorse_op_export_file (keys, FALSE, uri, &err); 
		        
        if (err != NULL)
            seahorse_util_handle_error (err, _("Couldn't export key to \"%s\""),
                    seahorse_util_uri_get_last (uri));
                    
        g_free (uri);
    }
    
    g_list_free (keys);
}

/*Archives public and private keyrings*/
static void
backup_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    GtkWidget *dialog;
	gchar *uri = NULL;
    gchar **uris;
    gchar *ext, *t;
    const gchar* home_dir = NULL;
    
    dialog = seahorse_util_chooser_save_new (_("Backup Keyrings to Archive"), 
                GTK_WINDOW(glade_xml_get_widget (swidget->xml, "key-manager")));
    seahorse_util_chooser_show_archive_files (dialog);	    	    	    

    uri = seahorse_util_chooser_save_prompt (dialog);
    if(uri)  {	    	
       
        /* Save the extension */
        t = strchr(uri, '.');
        if(t != NULL) {
            t++;
            if(t[0] != 0) 
                seahorse_gconf_set_string (MULTI_EXTENSION_KEY, t);
        } else {
            if ((ext = seahorse_gconf_get_string (MULTI_EXTENSION_KEY)) == NULL)
    	        ext = g_strdup ("zip"); /* Yes this happens when the schema isn't installed */
	        
        	t = seahorse_util_uri_replace_ext (uri, ext);
        	g_free(uri);
        	g_free(ext);
        	uri = t;
        }
	        
    	home_dir = (const gchar*)seahorse_gpg_homedir();
	    	
        uris = g_new0 (gchar*, 3);
    	uris[0] = g_strconcat (home_dir, PUB_RING, NULL);
    	uris[1] = g_strconcat (home_dir, SEC_RING, NULL);
	    	
    	seahorse_util_uris_package (uri, (const gchar**)uris);

        g_strfreev (uris);
    }
}

static void
sign_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	GList *list = NULL;
	
	list = seahorse_key_store_get_selected_keys (GTK_TREE_VIEW (
		glade_xml_get_widget (swidget->xml, KEY_LIST)));
	seahorse_sign_show (swidget->sctx, list);
}

#ifdef WITH_KEYSERVER
static void
search_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    seahorse_keyserver_search_show (swidget->sctx);
}

static void
sync_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	GList *list = NULL;
    GtkTreeView *view;
    
    view = GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, KEY_LIST));
	
	list = seahorse_key_store_get_selected_keys (view);
    if (list == NULL)
        list = seahorse_key_store_get_all_keys (view);
    seahorse_keyserver_sync_show (swidget->sctx, list);
}
#endif

/* Loads delete dialog if a key is selected */
static void
delete_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	GList *list = NULL;
    GtkTreeView *view = 
        GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, KEY_LIST));
	
	list = seahorse_key_store_get_selected_keys (view);
    
    /* Special behavior for a single selection */
    if (g_list_length (list) == 1) {
        SeahorseKey *skey;
        guint uid;
        
        skey = seahorse_key_store_get_selected_key (view, &uid);
        if (uid > 0) 
            seahorse_delete_userid_show (swidget->sctx, skey, uid);
        else    
        	seahorse_delete_show (swidget->sctx, list);

    /* Multiple keys */
    } else {    
	    seahorse_delete_show (swidget->sctx, list);
    }
    
  	g_list_free (list);
}

static void
add_uid_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (
		glade_xml_get_widget (swidget->xml, KEY_LIST)), NULL);
	if (skey != NULL)
		seahorse_add_uid_new (swidget->sctx, skey);
}

static void
add_revoker_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (
		glade_xml_get_widget (swidget->xml, KEY_LIST)), NULL);
	if (skey != NULL)
		seahorse_add_revoker_new (swidget->sctx, skey);
}

/* Loads preferences dialog */
static void
preferences_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_preferences_show (swidget->sctx, 0);
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

/* Shows about dialog */
static void
about_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	static GtkWidget *about = NULL;
	GdkPixbuf *pixbuf = NULL;

	gchar *authors[] = {
		"Jacob Perkins <jap1@users.sourceforge.net>",
		"Jose Carlos Garcia Sogo <jsogo@users.sourceforge.net>",
		"Jean Schurger <yshark@schurger.org>",
        "Nate Nielsen <nielsen@memberwebs.com>",
        "",
        "Contributions:",
        "Adam Schreiber <sadam@clemson.edu>",
        "Albrecht Dreß <albrecht.dress@arcor.de>",
		NULL
	};

	gchar *documenters[] = {
		"Jacob Perkins <jap1@users.sourceforge.net>",
		NULL
	};

	gchar *translator_credits = _("translator_credits");

	if (about != NULL) {
		gtk_window_present (GTK_WINDOW (about));
		return;
	}

	pixbuf = gdk_pixbuf_new_from_file (PIXMAPSDIR "seahorse.png", NULL);
	if (pixbuf != NULL) {
		GdkPixbuf *temp_pixbuf = NULL;
		
		temp_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
			gdk_pixbuf_get_width (pixbuf),
			gdk_pixbuf_get_height (pixbuf),
			GDK_INTERP_HYPER);
		g_object_unref (pixbuf);

		pixbuf = temp_pixbuf;
	}

	about = gnome_about_new (_("seahorse"), VERSION,
		"Copyright \xc2\xa9 2002, 2003, 2004 Seahorse Project",
		"http://seahorse.sourceforge.net/",
		(const char **)authors, (const char **)documenters,
		strcmp (translator_credits, "translator_credits") != 0 ?
			translator_credits : NULL,
		pixbuf);
	gtk_window_set_transient_for (GTK_WINDOW (about), GTK_WINDOW (
		glade_xml_get_widget (swidget->xml, swidget->name)));
	gtk_window_set_destroy_with_parent (GTK_WINDOW (about), TRUE);

	if (pixbuf != NULL)
		g_object_unref (pixbuf);

	g_signal_connect (GTK_OBJECT (about), "destroy",
		G_CALLBACK (gtk_widget_destroyed), &about);
	gtk_widget_show (about);
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
selection_changed (GtkTreeSelection *selection, SeahorseWidget *swidget)
{
    GtkActionGroup *actions;
	SeahorseKey *skey = NULL;
    gboolean selected = FALSE;
    gboolean secret = FALSE;
	gint rows = 0;
	
	rows = gtk_tree_selection_count_selected_rows (selection);
	selected = rows > 0;
	
	if (selected) {
        set_numbered_status (swidget, _("Selected %d key"),
                                      _("Selected %d keys"), rows);
	}
	
	if (rows == 1) {
		skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (
			glade_xml_get_widget (swidget->xml, KEY_LIST)), NULL);
		secret = (skey != NULL && SEAHORSE_IS_KEY_PAIR (skey));
	}
    
    actions = seahorse_widget_find_actions (swidget, "key");
    gtk_action_group_set_sensitive (actions, selected);
    
    actions = seahorse_widget_find_actions (swidget, "keypair");
    gtk_action_group_set_sensitive (actions, selected && secret);
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
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (widget), NULL);
	if (skey != NULL)
		show_context_menu (swidget, 0, gtk_get_current_event_time ());
       
    return FALSE;
}


static void
gconf_notification (GConfClient *gclient, guint id, GConfEntry *entry, SeahorseWidget *swidget)
{
	const gchar *key;
	GConfValue *value;
	
	key = gconf_entry_get_key (entry);
	value = gconf_entry_get_value (entry);

    /* 
     * Removed last two gconf notification items, although I'm sure
     * we'll get more, so leaving this code here... 
     */
}

static void
target_drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
                           GtkSelectionData *data, guint info, guint time, SeahorseWidget *swidget)
{
    SeahorseKeySource *sksrc;
    gint keys = 0;
    GError *err = NULL;
    gchar** uris;
    gchar** u;
    
    DBG_PRINT(("DragDataReceived -->\n"));
    g_return_if_fail (data != NULL);
    
    sksrc = seahorse_context_get_key_source (swidget->sctx);
    g_return_if_fail (sksrc != NULL);
    
    switch(info) {
    case TEXT_PLAIN:
        keys = seahorse_op_import_text (sksrc, data->data, &err);
        break;
        
    case TEXT_URIS:
        uris = g_strsplit (data->data, "\n", 0);
        for(u = uris; *u; u++) {
            g_strstrip (*u);
            if ((*u)[0]) { /* Make sure it's not an empty line */
                keys += seahorse_op_import_file (sksrc, *u, &err);  
                if (err != NULL)
                    break;
            }
        }
        g_strfreev (uris);
        break;
        
    default:
        g_assert_not_reached ();
        break;
    } 

    if (err != NULL)
        seahorse_util_handle_error (err, _("Couldn't import key from \"%s\""),
                                    seahorse_util_uri_get_last (*u));
    else
        set_numbered_status (swidget, _("Imported %d key"), 
                                      _("Imported %d keys"), keys);
    
    DBG_PRINT(("DragDataReceived <--\n"));
}

/* Refilter the keys when the filter text changes */
static void
filter_changed (GtkWidget *widget, SeahorseKeyStore* skstore)
{
    const gchar* text = gtk_entry_get_text (GTK_ENTRY (widget));
    g_object_set (skstore, "filter", text, NULL);
}

static void
help_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    seahorse_widget_show_help (swidget);
}

/* BUILDING THE MAIN WINDOW ------------------------------------------------- */

static GtkActionEntry ui_entries[] = {
    
    /* Top menu items */
    { "key-menu", NULL, N_("_Key") },
    { "edit-menu", NULL, N_("_Edit") },
    { "view-menu", NULL, N_("_View") },
    { "help-menu", NULL, N_("_Help") },
    
    /* Key Actions */
    { "key-generate", GTK_STOCK_NEW, N_("_Create Key Pair..."), "<control>N", 
            N_("Create a new key pair"), G_CALLBACK (generate_activate) },
    { "key-import-file", GTK_STOCK_OPEN, N_("_Import..."), "<control>I",
            N_("Import keys into your keyring from a file"), G_CALLBACK (import_activate) },
    { "key-backup", GTK_STOCK_SAVE, N_("_Backup Keyrings..."), "",
            N_("Backup all keys"), G_CALLBACK (backup_activate) }, 
    { "key-import-clipboard", GTK_STOCK_PASTE, N_("Paste _Keys"), "<control>V",
            N_("Import keys from the clipboard"), G_CALLBACK (paste_activate) }, 
            
    { "app-quit", GTK_STOCK_QUIT, N_("_Quit"), "<control>Q",
            N_("Close this program"), G_CALLBACK (quit) }, 
    { "app-preferences", GTK_STOCK_PREFERENCES, N_("Prefere_nces"), NULL,
            N_("Change preferences for this program"), G_CALLBACK (preferences_activate) },
    { "app-about", "gnome-stock-about", N_("_About"), NULL, 
            N_("About this program"), G_CALLBACK (about_activate) }, 
            
    { "view-expand-all", GTK_STOCK_ADD, N_("_Expand All"), NULL,
            N_("Expand all listings"), G_CALLBACK (expand_all_activate) }, 
    { "view-collapse-all", GTK_STOCK_REMOVE, N_("_Collapse All"), NULL,
            N_("Collapse all listings"), G_CALLBACK (collapse_all_activate) }, 
            
    { "help-show", GTK_STOCK_HELP, N_("_Contents"), "F1",
            N_("Show Seahorse help"), G_CALLBACK (help_activate) }, 
};

static GtkActionEntry public_entries[] = {
    { "key-properties", GTK_STOCK_PROPERTIES, N_("P_roperties"), NULL,
            N_("Show key properties"), G_CALLBACK (properties_activate) }, 
    { "key-export-file", GTK_STOCK_SAVE_AS, N_("E_xport..."), NULL,
            N_("Export public key"), G_CALLBACK (export_activate) }, 
    { "key-export-clipboard", GTK_STOCK_COPY, N_("_Copy Key"), "<control>C",
            N_("Copy selected keys to the clipboard"), G_CALLBACK (copy_activate) }, 
    { "key-sign", GTK_STOCK_INDEX, N_("_Sign..."), NULL,
            N_("Sign public key"), G_CALLBACK (sign_activate) }, 
    { "key-delete", GTK_STOCK_DELETE, N_("_Delete Key"), NULL,
            N_("Delete selected keys"), G_CALLBACK (delete_activate) }, 
};

static GtkActionEntry private_entries[] = {
    { "key-add-userid", GTK_STOCK_ADD, N_("Add _User ID..."), NULL,
            N_("Add a new user ID"), G_CALLBACK (add_uid_activate) }, 
    { "key-add-revoker", GTK_STOCK_CANCEL, N_("Add _Revoker..."), NULL,
            N_("Add the default key as a revoker"), G_CALLBACK (add_revoker_activate) }, 
};

static GtkActionEntry remote_entries[] = {

    { "remote-menu", NULL, N_("_Remote") },
    { "remote-find", GTK_STOCK_FIND, N_("_Find Remote Keys..."), "",
            N_("Search for keys on a key server"), G_CALLBACK (search_activate) }, 
    { "remote-sync", GTK_STOCK_REFRESH, N_("_Sync and Publish Keys..."), "",
            N_("Publish and/or sync your keys with those online."), G_CALLBACK (sync_activate) }, 
};

GtkWindow* 
seahorse_key_manager_show (SeahorseContext *sctx)
{
    GtkWindow *win;
    SeahorseOperation *operation;
	SeahorseWidget *swidget;
	GtkTreeView *view;
    GtkWidget *w;
	GtkTreeSelection *selection;
    SeahorseKeyStore *skstore;
    SeahorseKeySource *sksrc;
    GtkActionGroup *actions;
    GtkAction *action;
	
	swidget = seahorse_widget_new ("key-manager", sctx);
	gtk_object_sink (GTK_OBJECT (sctx));
    win = GTK_WINDOW (glade_xml_get_widget (swidget->xml, "key-manager"));
    
    /* General normal actions */
    actions = gtk_action_group_new ("main");
    gtk_action_group_add_actions (actions, ui_entries, 
                                  G_N_ELEMENTS (ui_entries), swidget);
    seahorse_widget_add_actions (swidget, actions);
    
    /* Actions that are allowed on all keys */
    actions = gtk_action_group_new ("key");
    gtk_action_group_add_actions (actions, public_entries, 
                                  G_N_ELEMENTS (public_entries), swidget);
    seahorse_widget_add_actions (swidget, actions);

    /* Mark the properties toolbar button as important */
    action = gtk_action_group_get_action (actions, "key-properties");
    g_return_val_if_fail (action, win);
    g_object_set (action, "is-important", TRUE, NULL);

    /* Actions for public keys */
    actions = gtk_action_group_new ("keypair");
    gtk_action_group_add_actions (actions, private_entries,
                                  G_N_ELEMENTS (private_entries), swidget);
    seahorse_widget_add_actions (swidget, actions);

    /* Actions for keyservers */
#ifdef WITH_KEYSERVER      
    actions = gtk_action_group_new ("remote");
    gtk_action_group_add_actions (actions, remote_entries, 
                                  G_N_ELEMENTS (remote_entries), swidget);
    seahorse_widget_add_actions (swidget, actions);                                  
#endif

    sksrc = seahorse_context_get_key_source (sctx);
    g_return_val_if_fail (sksrc != NULL, win);
    	
	/* close event */
	glade_xml_signal_connect_data (swidget->xml, "quit_event",
		G_CALLBACK (delete_event), swidget);
	
	/* tree view signals */	
	glade_xml_signal_connect_data (swidget->xml, "row_activated",
		G_CALLBACK (row_activated), swidget);
	glade_xml_signal_connect_data (swidget->xml, "key_list_button_pressed",
		G_CALLBACK (key_list_button_pressed), swidget);
	glade_xml_signal_connect_data (swidget->xml, "key_list_popup_menu",
		G_CALLBACK (key_list_popup_menu), swidget);
        
	/* init gclient */
	seahorse_gconf_notify_lazy (UI_SCHEMAS, (GConfClientNotifyFunc) gconf_notification, 
                                swidget, GTK_WIDGET (win));
	
	/* first time signals */
	glade_xml_signal_connect_data (swidget->xml, "import_button_clicked",
		G_CALLBACK (import_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "new_button_clicked",
		G_CALLBACK (new_button_clicked), swidget);
		
	/* init key list & selection settings */
	view = GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, KEY_LIST));
	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (selection, "changed",
		G_CALLBACK (selection_changed), swidget);
    skstore = seahorse_key_manager_store_new (sksrc, view);
	selection_changed (selection, swidget);

    /* To avoid flicker */
    seahorse_widget_show (swidget);
	

    /* Setup drops */
    gtk_drag_dest_set (GTK_WIDGET (win), GTK_DEST_DEFAULT_ALL, 
                seahorse_target_entries, seahorse_n_targets, GDK_ACTION_COPY);
    gtk_signal_connect (GTK_OBJECT (win), "drag_data_received",
                GTK_SIGNAL_FUNC (target_drag_data_received), swidget);
                        
    /* For the filtering */
    glade_xml_signal_connect_data(swidget->xml, "on_filter_changed",
                G_CALLBACK(filter_changed), skstore);

    /* Hook progress bar in */
    operation = seahorse_key_source_get_operation (sksrc);
    g_return_val_if_fail (operation != NULL, win);
    
    w = glade_xml_get_widget (swidget->xml, "status");
    seahorse_progress_appbar_set_operation (w, operation);

    /* Although not all the keys have completed we'll know whether we have 
     * any or not at this point */
	if (seahorse_key_source_get_count (sksrc, FALSE) == 0) {
		w = glade_xml_get_widget (swidget->xml, "first-time-box");
		gtk_widget_show (w);
	}
    
    return win;
}
