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

#include "config.h"
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
#include "seahorse-pgp-key-op.h"
#include "seahorse-key-widget.h"
#include "seahorse-gpg-options.h"
#include "seahorse-gconf.h"
#include "seahorse-gtkstock.h"
#include "seahorse-key-source.h"
#include "seahorse-vfs-data.h"

#ifdef WITH_KEYSERVER
#include "seahorse-keyserver-sync.h"
#endif

#ifdef WITH_SSH
#include "seahorse-ssh-key.h"
#endif

/* The various tabs. If adding a tab be sure to fix 
   logic throughout this file. */
enum KeyManagerTabs {
    TAB_PUBLIC = 1,
    TAB_TRUSTED,
    TAB_PRIVATE,
    HIGHEST_TAB
};

SeahorseKeyPredicate pred_public = {
    0,                  /* ktype, set later */
    0,                  /* keyid */
    SKEY_LOC_LOCAL,     /* location */
    SKEY_PUBLIC,        /* etype */
    0,                  /* flags */
    SKEY_FLAG_TRUSTED,  /* nflags */
    NULL,               /* sksrc */
};

SeahorseKeyPredicate pred_trusted = {
    0,                  /* ktype, set later*/
    0,                  /* keyid */
    SKEY_LOC_LOCAL,     /* location */
    SKEY_PUBLIC,        /* etype */
    SKEY_FLAG_TRUSTED,  /* flags */
    0,                  /* nflags */
    NULL,               /* sksrc */
};

SeahorseKeyPredicate pred_private = {
    0,                  /* ktype, set later */
    0,                  /* keyid */
    SKEY_LOC_LOCAL,     /* location */
    SKEY_PRIVATE,       /* etype */
    0,                  /* flags */
    0,                  /* nflags */
    NULL,               /* sksrc */
};

#define SEC_RING "/secring.gpg"
#define PUB_RING "/pubring.gpg"

/* SIGNAL CALLBACKS --------------------------------------------------------- */

/* Quits seahorse */
static void
quit (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_context_destroy (SCTX_APP());
}

/* Quits seahorse */
static gboolean
delete_event (GtkWidget *widget, GdkEvent *event, SeahorseWidget *swidget)
{
	quit (widget, swidget);
	return TRUE;
}

static GtkWidget*
get_tab_for_key (SeahorseWidget* swidget, SeahorseKey *skey)
{
    SeahorseKeyset *skset;
    GtkNotebook *notebook;
    GtkWidget *widget;
    gint pages, i;
    
    notebook = GTK_NOTEBOOK (glade_xml_get_widget (swidget->xml, "notebook"));
    g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);

    pages = gtk_notebook_get_n_pages(notebook);
    for(i = 0; i < pages; i++)
    {
        widget = gtk_notebook_get_nth_page (notebook, i);
        skset = g_object_get_data (G_OBJECT (widget), "keyset");
        
        if (seahorse_keyset_has_key (skset, skey))
            return widget;
    }

    return NULL;
}

static guint
get_tab_id (GtkWidget *tab)
{
    return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (tab), "tab-id"));
}

/* Get the currently selected key store */
static GtkWidget* 
get_current_tab (SeahorseWidget *swidget)
{
    GtkNotebook *notebook;
    GtkWidget *widget;
    guint page;
    
    notebook = GTK_NOTEBOOK (glade_xml_get_widget (swidget->xml, "notebook"));
    g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), 0);
    
    page = gtk_notebook_get_current_page (notebook);
    g_return_val_if_fail (page >= 0, 0);
    
    widget = gtk_notebook_get_nth_page (notebook, page);
    g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);
    
    return widget;
}

static void
set_current_tab (SeahorseWidget *swidget, guint tabid)
{
    GtkNotebook *notebook;
    guint pages, i;
    
    notebook = GTK_NOTEBOOK (glade_xml_get_widget (swidget->xml, "notebook"));
    g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
    
    pages = gtk_notebook_get_n_pages(notebook);
    for(i = 0; i < pages; i++)
    {
        if (get_tab_id (gtk_notebook_get_nth_page (notebook, i)) == tabid)
        {
            gtk_notebook_set_current_page (notebook, i);
            return;
        }
    }
    
    g_return_if_reached ();
}

static GtkTreeView* 
get_tab_view (SeahorseWidget *swidget, guint tabid)
{
    const gchar *name;
    
    switch (tabid) {
    case 0: /* This happens before initialization */
        return NULL; 
    case TAB_PUBLIC:
        name = "pub-key-list";
        break;
    case TAB_PRIVATE:
        name = "sec-key-list";
        break;
    case TAB_TRUSTED:
        name = "trust-key-list";
        break;
    default:
        g_return_val_if_reached (NULL);
        break;
    };

    return GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, name));
}

static GtkTreeView*
get_current_view (SeahorseWidget *swidget)
{
    GtkWidget *tab;

    tab = get_current_tab (swidget);
    if(tab == NULL)
        return NULL;
    
    return get_tab_view (swidget, get_tab_id (tab));
}

static GList*
get_selected_keys (SeahorseWidget *swidget)
{
    GtkTreeView *view = get_current_view (swidget);
    g_return_val_if_fail (view != NULL, NULL);
    return seahorse_key_store_get_selected_keys (view);
}

static SeahorseKey*
get_selected_key (SeahorseWidget *swidget, guint *uid)
{
    GtkTreeView *view = get_current_view (swidget);
    g_return_val_if_fail (view != NULL, NULL);
    return seahorse_key_store_get_selected_key (view, uid);
}

static void
set_selected_keys (SeahorseWidget *swidget, GList* keys)
{
    GList* tab_lists[HIGHEST_TAB];
    GtkTreeView *view;
    GtkWidget *tab;
    GList* l;
    guint tabid;
    guint i, last = 0;
    
    memset(tab_lists, 0, sizeof(tab_lists));
    
    for(l = keys; l; l = g_list_next (l)) {
        tab = get_tab_for_key (swidget, SEAHORSE_KEY (l->data));
        if (tab == NULL)
            continue;
        
        tabid = get_tab_id (tab);
        if(!tabid)
            continue;
        
        g_assert(tabid < HIGHEST_TAB);
        tab_lists[tabid] = g_list_prepend (tab_lists[tabid], l->data);
    }
    
    for(i = 1; i < HIGHEST_TAB; i++) {
        
        view = get_tab_view (swidget, i);
        g_assert(view);
        
        if(tab_lists[i])
            last = i;

        seahorse_key_store_set_selected_keys (view, tab_lists[i]);
        g_list_free (tab_lists[i]);
    }
    
    if (last != 0)
        set_current_tab (swidget, last);
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

/* Shows the properties for a given key */
static void
show_properties (SeahorseKey *skey)
{
    g_assert (skey);
    
    if (SEAHORSE_IS_PGP_KEY (skey))
		seahorse_key_properties_new (SEAHORSE_PGP_KEY (skey));
#ifdef WITH_SSH    
    else if (SEAHORSE_IS_SSH_KEY (skey))
        seahorse_ssh_key_properties_new (SEAHORSE_SSH_KEY (skey));
#endif 
    
}

/* Loads generate dialog */
static void
generate_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    seahorse_generate_select_show ();
}

/* Loads Key generation assistant for first time users */
static void
new_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    seahorse_generate_select_show ();
}

static void
imported_keys (SeahorseOperation *op, SeahorseWidget *swidget)
{
    GError *err = NULL;
    GList *keys;
    guint nkeys;

    if (!seahorse_operation_is_successful (op)) {
        seahorse_operation_steal_error (op, &err);
        seahorse_util_handle_error (err, _("Couldn't import keys"));
        return;
    }
    
    /* This key list is freed with the operation */
    keys = (GList*)seahorse_operation_get_result (op);
    set_selected_keys (swidget, keys);
    
    nkeys = g_list_length (keys);
    set_numbered_status (swidget, _("Imported %d key"), 
                                  _("Imported %d keys"), nkeys);
}

static void
import_files (SeahorseWidget *swidget, const gchar **uris)
{
    SeahorseKeySource *sksrc;
    SeahorseOperation *op;
    gpgme_data_t data;
    GError *err = NULL;
    
    data = seahorse_vfs_data_read_multi (uris, &err);
    if (!data) {
        seahorse_util_handle_error (err, _("Couldn't import keys"));
        return;
    }

    /* TODO: This needs work once we get more key types */
    sksrc = seahorse_context_find_key_source (SCTX_APP (), SKEY_PGP, SKEY_LOC_LOCAL);
    g_return_if_fail (sksrc && SEAHORSE_IS_PGP_SOURCE (sksrc));
        
    /* data is freed here */
    op = seahorse_key_source_import (sksrc, data);
    g_return_if_fail (op != NULL);

    g_signal_connect (op, "done", G_CALLBACK (imported_keys), swidget);
    seahorse_progress_show (op, _("Importing Keys"), TRUE);
    
    /* A running operation refs itself */
    g_object_unref (op);
}

static void
import_text (SeahorseWidget *swidget, const gchar *text)
{
    SeahorseKeySource *sksrc;
    SeahorseOperation *op;
    gpgme_data_t data;
    
    data = gpgmex_data_new_from_mem (text, strlen (text), TRUE);
    g_return_if_fail (data != NULL);

    /* TODO: This needs work once we get more key types */
    sksrc = seahorse_context_find_key_source (SCTX_APP (), SKEY_PGP, SKEY_LOC_LOCAL);
    g_return_if_fail (sksrc && SEAHORSE_IS_PGP_SOURCE (sksrc));
    
    /* data is freed here */
    op = seahorse_key_source_import (sksrc, data);
    g_return_if_fail (op != NULL);
    
    g_signal_connect (op, "done", G_CALLBACK (imported_keys), swidget);
    seahorse_progress_show (op, _("Importing Keys"), TRUE);
    
    /* A running operation refs itself */
    g_object_unref (op);
}

/* Loads import dialog */
static void
import_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    GtkWidget *dialog;
    char* uri = NULL;
    
    dialog = seahorse_util_chooser_open_new (_("Import Key"), 
                GTK_WINDOW(glade_xml_get_widget (swidget->xml, "key-manager")));
    seahorse_util_chooser_show_key_files (dialog);

    uri = seahorse_util_chooser_open_prompt (dialog);
    if(uri) {
        const gchar *uris[] = { uri, NULL };
        import_files (swidget, uris);
    }
    g_free (uri);
}

/* Callback for pasting from clipboard */
static void
clipboard_received (GtkClipboard *board, const gchar *text, SeahorseWidget *swidget)
{
    import_text (swidget, text);
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
        seahorse_operation_steal_error (op, &err);
        seahorse_util_handle_error (err, _("Couldn't retrieve data from key server"));
    }
    
    data = (gpgme_data_t)seahorse_operation_get_result (op);
    g_return_if_fail (data != NULL);
    
    text = seahorse_util_write_data_to_text (data, FALSE);
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
  
    keys = get_selected_keys (swidget);
    num = g_list_length (keys);
    
    if (num == 0)
        return;
    
    op = seahorse_key_source_export_keys (keys, NULL);
    g_return_if_fail (op != NULL);
    
    g_object_set_data (G_OBJECT (op), "num-keys", GINT_TO_POINTER (num));
        
    if (seahorse_operation_is_running (op)) {
        seahorse_progress_show (op, _("Retrieving keys"), TRUE);
        g_signal_connect (op, "done", G_CALLBACK (copy_done), swidget);
    } else {
        copy_done (op, swidget);
    }
        
    /* Running operation refs itself */
    g_object_unref (op);
    g_list_free (keys);
}

/* Loads key properties if a key is selected */
static void
properties_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey = get_selected_key (swidget, NULL);
	if (skey != NULL)
        show_properties (skey);
}

/* Loads export dialog if a key is selected */
static void
export_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseOperation *op;
    GtkWidget *dialog;
    gchar* uri = NULL;
    GError *err = NULL;
    gpgme_data_t data;
    GList *keys;

    keys = get_selected_keys (swidget);
    if (keys == NULL)
        return;
    
    dialog = seahorse_util_chooser_save_new (_("Export Key"), 
                GTK_WINDOW(glade_xml_get_widget (swidget->xml, "key-manager")));
    seahorse_util_chooser_show_key_files (dialog);
    seahorse_util_chooser_set_filename (dialog, keys);
     
    uri = seahorse_util_chooser_save_prompt (dialog);
    if(uri) {
        data = seahorse_vfs_data_create (uri, SEAHORSE_VFS_WRITE, &err);
        if (data) {
    
            op = seahorse_key_source_export_keys (keys, data);
            g_return_if_fail (op != NULL);
        
            seahorse_operation_wait (op);
            gpgmex_data_release (data);
    
            if (!seahorse_operation_is_successful (op)) 
                seahorse_operation_steal_error (op, &err);
        }
        
        if (err) 
            seahorse_util_handle_error (err, _("Couldn't export key to \"%s\""),
                                        seahorse_util_uri_get_last (uri));
    }
    
    g_free (uri);
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
    SeahorseKey *skey;
    guint uid;
    GList *keys = NULL;

    skey = get_selected_key (swidget, &uid);
    if (SEAHORSE_IS_PGP_KEY (skey)) {
        seahorse_sign_show (SEAHORSE_PGP_KEY (skey), uid);
        
#ifdef WITH_KEYSERVER
        if (seahorse_gconf_get_boolean(AUTOSYNC_KEY) == TRUE) {
            keys = g_list_append (keys, skey);
            seahorse_keyserver_sync (keys);
            g_list_free(keys);
        }
#endif
    }
}

static void
search_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
#ifdef WITH_KEYSERVER
    seahorse_keyserver_search_show ();
#endif
}

static void
sync_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
#ifdef WITH_KEYSERVER
    GList *keys = get_selected_keys (swidget);
    GList *l;
    
    /* Only supported on PGP keys */
    for (l = keys; l; l = g_list_next (l)) {
        if (seahorse_key_get_ktype (SEAHORSE_KEY (l->data)) != SKEY_PGP) {
            keys = l = g_list_delete_link (keys, l);
            if (keys == NULL)
                break;
        }
    }
    
    if (keys == NULL)
        keys = seahorse_context_find_keys (SCTX_APP (), SKEY_PGP, 0, SKEY_LOC_LOCAL);
    seahorse_keyserver_sync_show (keys);
    g_list_free (keys);
#endif
}

static void
setup_sshkey_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
#ifdef WITH_SSH
    GList *keys = get_selected_keys (swidget);
    GList *l;
    
    /* Only supported on SSH keys */
    for (l = keys; l; l = g_list_next (l)) {
        if (seahorse_key_get_ktype (SEAHORSE_KEY (l->data)) != SKEY_SSH) {
            keys = l = g_list_delete_link (keys, l);
            if (keys == NULL)
                break;
        }
    }    
    
    if (keys != NULL)
        seahorse_ssh_upload_prompt (keys);
#endif 
}

/* Loads delete dialog if a key is selected */
static void
delete_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    GList *keys = get_selected_keys (swidget);
    
    /* Special behavior for a single selection */
    if (g_list_length (keys) == 1) {
        SeahorseKey *skey;
        guint uid = 0;
        
        skey = get_selected_key (swidget, &uid);
        if (uid > 0 && SEAHORSE_IS_PGP_KEY (skey)) 
            /* User ids start from one in this case, strange */
            seahorse_delete_userid_show (skey, uid + 1);
        else    
            seahorse_delete_show (keys);

    /* Multiple keys */
    } else {    
        seahorse_delete_show (keys);
    }
    
    g_list_free (keys);
}

/* Loads preferences dialog */
static void
preferences_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    seahorse_preferences_show (NULL);
}

/* Makes URL in About Dialog Clickable */
static void about_dialog_activate_link_cb (GtkAboutDialog *about,
                                           const gchar *url,
                                           gpointer data)
{
    gnome_url_show (url, NULL);
}

/* Shows about dialog */
static void
about_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
    static gboolean been_here = FALSE;
    
	static const gchar *authors[] = {
		"Jacob Perkins <jap1@users.sourceforge.net>",
		"Jose Carlos Garcia Sogo <jsogo@users.sourceforge.net>",
		"Jean Schurger <yshark@schurger.org>",
        "Nate Nielsen <nielsen@memberwebs.com>",
        "Adam Schreiber <sadam@clemson.edu>",
        "",
        "Contributions:",
        "Albrecht Dreß <albrecht.dress@arcor.de>",
        "Jim Pharis <binbrain@gmail.com>",
		NULL
	};

	static const gchar *documenters[] = {
		"Jacob Perkins <jap1@users.sourceforge.net>",
        "Adam Schreiber <sadam@clemson.edu>",
		NULL
	};

    static const gchar *artists[] = {
        "Jacob Perkins <jap1@users.sourceforge.net>",
        "Nate Nielsen <nielsen@memberwebs.com>",
        NULL
    };

    if (!been_here)
	{
		been_here = TRUE;
		gtk_about_dialog_set_url_hook (about_dialog_activate_link_cb, NULL, NULL);
	}
	
	gtk_show_about_dialog (NULL, 
                "name", _("seahorse"),
                "version", VERSION,
                "comments", _("Encryption Key Manager"),
                "copyright", "Copyright \xc2\xa9 2002, 2003, 2004, 2005, 2006 Seahorse Project",
                "authors", authors,
                "documenters", documenters,
                "artists", artists,
                "translator-credits", _("translator-credits"),
                "logo-icon-name", "seahorse",
                "website", "http://seahorse.sf.net",
                "website-label", _("Seahorse Project Homepage"),
                NULL);
}

/* Loads key properties of activated key */
static void
row_activated (GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *arg2, 
               SeahorseWidget *swidget)
{
    GtkTreeView *view = get_current_view (swidget);
	SeahorseKey *skey;
    
    g_return_if_fail (view != NULL);
	
	skey = seahorse_key_store_get_key_from_path (view, path, NULL);
	if (skey != NULL)
        show_properties (skey);
}

static void
selection_changed (GtkTreeSelection *notused, SeahorseWidget *swidget)
{
    GtkTreeView *view;
    GtkActionGroup *actions;
    GList *keys, *l;
	SeahorseKey *skey = NULL;
    gint ktype = 0;
    gboolean selected = FALSE;
    gboolean secret = FALSE;
	gint rows = 0;
    
    view = get_current_view (swidget);
    if (view != NULL) {
        GtkTreeSelection *selection = gtk_tree_view_get_selection (view);
    	rows = gtk_tree_selection_count_selected_rows (selection);
	    selected = rows > 0;
    }
	
	if (selected) {
        set_numbered_status (swidget, _("Selected %d key"),
                                      _("Selected %d keys"), rows);
        
        keys = get_selected_keys (swidget);

        if (keys) {
            secret = TRUE;
            ktype = -1;
        }
            
        for (l = keys; l; l = g_list_next (l)) {
            skey = SEAHORSE_KEY (l->data);
            if (seahorse_key_get_etype (skey) != SKEY_PRIVATE)
                secret = FALSE;
            if (ktype == -1)
                ktype = (gint)seahorse_key_get_ktype (skey);
            else if (ktype != seahorse_key_get_ktype (skey))
                ktype = 0;
        }
        
        g_list_free (keys);
	}
    
    actions = seahorse_widget_find_actions (swidget, "key");
    gtk_action_group_set_sensitive (actions, selected);
    
    actions = seahorse_widget_find_actions (swidget, "pgp");
    gtk_action_group_set_sensitive (actions, ((ktype == SKEY_PGP) && (seahorse_key_get_etype (skey) != SKEY_PRIVATE)));
    
#ifdef WITH_SSH    
    actions = seahorse_widget_find_actions (swidget, "ssh");
    gtk_action_group_set_sensitive (actions, ktype == SKEY_SSH);
#endif    
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
key_list_button_pressed (GtkWidget *widget, GdkEventButton *event, 
                         SeahorseWidget *swidget)
{
	if (event->button == 3)
		show_context_menu (swidget, event->button, event->time);
	
	return FALSE;
}

static gboolean
key_list_popup_menu (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = get_selected_key (swidget, NULL);
	if (skey != NULL)
		show_context_menu (swidget, 0, gtk_get_current_event_time ());
       
    return FALSE;
}

static void
target_drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
                           GtkSelectionData *data, guint info, guint time, SeahorseWidget *swidget)
{
    gchar** uris;
    gchar** u;
    
    DBG_PRINT(("DragDataReceived -->\n"));
    g_return_if_fail (data != NULL);
    
    switch(info) {
    case TEXT_PLAIN:
        import_text (swidget, (const gchar*)(data->data));
        break;
        
    case TEXT_URIS:
        uris = g_strsplit ((const gchar*)data->data, "\n", 0);
        for(u = uris; *u; u++)
            g_strstrip (*u);
        import_files (swidget, (const gchar**)uris);
        g_strfreev (uris);
        break;
        
    default:
        g_assert_not_reached ();
        break;
    } 

    DBG_PRINT(("DragDataReceived <--\n"));
}

/* Refilter the keys when the filter text changes */
static void
filter_changed (GtkWidget *widget, SeahorseKeyStore* skstore)
{
    const gchar* text = gtk_entry_get_text (GTK_ENTRY (widget));
    g_object_set (skstore, "filter", text, NULL);
}

/* Clear filter when the tab changes */
static void
tab_changed (GtkWidget *widget, GtkNotebookPage *page, guint page_num, 
             SeahorseWidget *swidget)
{
    GtkWidget *entry = glade_xml_get_widget (swidget->xml, "filter");
    g_return_if_fail (entry != NULL);
    gtk_entry_set_text (GTK_ENTRY (entry), "");
    
    selection_changed (NULL, swidget);
}

static gboolean
first_timer (SeahorseWidget *swidget)
{
    GtkWidget *w;
    
    /* A slight chance we may already be closed */
    if (!SEAHORSE_IS_WIDGET (swidget))
        return FALSE;
    
    /* Although not all the keys have completed we'll know whether we have 
     * any or not at this point */
    if (seahorse_context_get_count (SCTX_APP()) == 0) {
        w = seahorse_widget_get_widget (swidget, "first-time-box");
        gtk_widget_show (w);
    }
    
    /* Remove this timer */
    return FALSE;
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
    { "view-menu", NULL, N_("_View") },
    { "help-menu", NULL, N_("_Help") },
    
    /* Key Actions */
    { "key-generate", GTK_STOCK_NEW, N_("_Create New Key..."), "<control>N", 
            N_("Create a new personal key"), G_CALLBACK (generate_activate) },
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
            
    { "remote-menu", NULL, N_("_Remote") },
            
    { "help-show", GTK_STOCK_HELP, N_("_Contents"), "F1",
            N_("Show Seahorse help"), G_CALLBACK (help_activate) }, 
};

static const GtkActionEntry key_entries[] = {
    { "key-properties", GTK_STOCK_PROPERTIES, N_("P_roperties"), NULL,
            N_("Show key properties"), G_CALLBACK (properties_activate) }, 
    { "key-export-file", GTK_STOCK_SAVE_AS, N_("E_xport..."), NULL,
            N_("Export public key"), G_CALLBACK (export_activate) }, 
    { "key-export-clipboard", GTK_STOCK_COPY, N_("_Copy Key"), "<control>C",
            N_("Copy selected keys to the clipboard"), G_CALLBACK (copy_activate) },
    { "key-delete", GTK_STOCK_DELETE, N_("_Delete Key"), NULL,
            N_("Delete selected keys"), G_CALLBACK (delete_activate) }, 
};

static const GtkActionEntry pgp_entries[] = {
    { "key-sign", GTK_STOCK_INDEX, N_("_Sign..."), NULL,
            N_("Sign public key"), G_CALLBACK (sign_activate) }, 
};

static const GtkActionEntry ssh_entries[] = {
    { "remote-upload-ssh", NULL, N_("Configure SSH Key for Computer..."), "",
            N_("Send public SSH key to another machine, and enable logins using that key."), G_CALLBACK (setup_sshkey_activate) },
};

static const GtkActionEntry keyserver_entries[] = {
    { "remote-find", GTK_STOCK_FIND, N_("_Find Remote Keys..."), "",
            N_("Search for keys on a key server"), G_CALLBACK (search_activate) }, 
    { "remote-sync", GTK_STOCK_REFRESH, N_("_Sync and Publish Keys..."), "",
            N_("Publish and/or sync your keys with those online."), G_CALLBACK (sync_activate) }, 
};

void
initialize_tab (SeahorseWidget *swidget, const gchar *tabwidget, guint tabid, 
                const gchar *viewwidget, SeahorseKeyPredicate *pred)
{
    SeahorseKeyset *skset;
    SeahorseKeyStore *skstore;
    GtkTreeSelection *selection;
    GtkTreeView *view;
    GtkWidget *tab;

    tab = glade_xml_get_widget (swidget->xml, tabwidget);
    g_return_if_fail (tab != NULL);
    g_object_set_data (G_OBJECT (tab), "tab-id", GINT_TO_POINTER (tabid));
    
    skset = seahorse_keyset_new_full (pred);
    g_object_set_data_full (G_OBJECT (tab), "keyset", skset, g_object_unref);
    
    /* init key list & selection settings */
    view = GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, viewwidget));
    g_return_if_fail (view != NULL);
    
    selection = gtk_tree_view_get_selection (view);
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
    g_signal_connect (selection, "changed", G_CALLBACK (selection_changed), swidget);
    
    /* Add new key store and associate it */
    skstore = seahorse_key_manager_store_new (skset, view);
    g_object_set_data_full (G_OBJECT (view), "key-store", skstore, g_object_unref);
    
    /* For the filtering */
    glade_xml_signal_connect_data(swidget->xml, "on_filter_changed",
                                  G_CALLBACK(filter_changed), skstore);
}

GtkWindow* 
seahorse_key_manager_show (SeahorseOperation *op)
{
    GtkWindow *win;
    SeahorseWidget *swidget;
    GtkWidget *w;
    GtkActionGroup *actions;
    GtkAction *action;
    
    swidget = seahorse_widget_new ("key-manager");
    win = GTK_WINDOW (glade_xml_get_widget (swidget->xml, "key-manager"));
    
    /* General normal actions */
    actions = gtk_action_group_new ("main");
    gtk_action_group_set_translation_domain (actions, NULL);
    gtk_action_group_add_actions (actions, ui_entries, 
                                  G_N_ELEMENTS (ui_entries), swidget);
    seahorse_widget_add_actions (swidget, actions);
    
    /* Actions that are allowed on all keys */
    actions = gtk_action_group_new ("key");
    gtk_action_group_set_translation_domain (actions, NULL);
    gtk_action_group_add_actions (actions, key_entries, 
                                  G_N_ELEMENTS (key_entries), swidget);
    seahorse_widget_add_actions (swidget, actions);

    /* Mark the properties toolbar button as important */
    action = gtk_action_group_get_action (actions, "key-properties");
    g_return_val_if_fail (action, win);
    g_object_set (action, "is-important", TRUE, NULL);

    /* Actions for pgp keys */
    actions = gtk_action_group_new ("pgp");
    gtk_action_group_set_translation_domain (actions, NULL);
    gtk_action_group_add_actions (actions, pgp_entries,
                                  G_N_ELEMENTS (pgp_entries), swidget);
    seahorse_widget_add_actions (swidget, actions);                              
    
    /* Actions for SSH keys */
    actions = gtk_action_group_new ("ssh");
    gtk_action_group_set_translation_domain (actions, NULL);
    gtk_action_group_add_actions (actions, ssh_entries,
                                  G_N_ELEMENTS (ssh_entries), swidget);
    seahorse_widget_add_actions (swidget, actions);
    
#ifndef WITH_SSH
    gtk_action_group_set_visible (actions, FALSE);
#endif

    /* Actions for keyservers */
    actions = gtk_action_group_new ("keyserver");
    gtk_action_group_set_translation_domain (actions, NULL);
    gtk_action_group_add_actions (actions, keyserver_entries, 
                                  G_N_ELEMENTS (keyserver_entries), swidget);
    seahorse_widget_add_actions (swidget, actions);                                  
    
#ifndef WITH_KEYSERVER
    gtk_action_group_set_visible (actions, FALSE);
#endif
    
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
        
	/* first time signals */
	glade_xml_signal_connect_data (swidget->xml, "import_button_clicked",
		G_CALLBACK (import_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "new_button_clicked",
		G_CALLBACK (new_button_clicked), swidget);

    /* The notebook */
    glade_xml_signal_connect_data (swidget->xml, "on_tab_changed", 
                                  G_CALLBACK(tab_changed), swidget);    
    
    /* Initialize the tabs, and associate them up */
    initialize_tab (swidget, "pub-key-tab", TAB_PUBLIC, "pub-key-list", &pred_public);
    initialize_tab (swidget, "trust-key-tab", TAB_TRUSTED, "trust-key-list", &pred_trusted);
    initialize_tab (swidget, "sec-key-tab", TAB_PRIVATE, "sec-key-list", &pred_private);
    
    /* Set focus to the current key list */
    w = GTK_WIDGET (get_current_view (swidget));
    gtk_widget_grab_focus (w);
    
    /* To avoid flicker */
    seahorse_widget_show (swidget);
    
    /* Setup drops */
    gtk_drag_dest_set (GTK_WIDGET (win), GTK_DEST_DEFAULT_ALL, 
                seahorse_target_entries, seahorse_n_targets, GDK_ACTION_COPY);
    gtk_signal_connect (GTK_OBJECT (win), "drag_data_received",
                GTK_SIGNAL_FUNC (target_drag_data_received), swidget);
                        
    /* Hook progress bar in */
    w = glade_xml_get_widget (swidget->xml, "status");
    seahorse_progress_appbar_set_operation (w, op);

    /* To show first time dialog */
    g_timeout_add (1000, (GSourceFunc)first_timer, swidget);
    
    selection_changed (NULL, swidget);
    
    return win;
}
