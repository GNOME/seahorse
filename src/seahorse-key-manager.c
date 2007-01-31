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

#ifdef WITH_GNOME_KEYRING
#include "seahorse-gkeyring-item.h"
#include "seahorse-gkeyring-source.h"
#endif

#define TRACK_SELECTED_KEY    "track-selected-keyid"
#define TRACK_SELECTED_TAB    "track-selected-tabid"

/* The various tabs. If adding a tab be sure to fix 
   logic throughout this file. */
enum KeyManagerTabs {
    TAB_PUBLIC = 1,
    TAB_TRUSTED,
    TAB_PRIVATE,
    TAB_PASSWORD,
    HIGHEST_TAB
};

SeahorseKeyPredicate pred_public = {
    0,                                      /* ktype, set later */
    0,                                      /* keyid */
    SKEY_LOC_LOCAL,                         /* location */
    SKEY_PUBLIC,                            /* etype */
    0,                                      /* flags */
    SKEY_FLAG_TRUSTED | SKEY_FLAG_IS_VALID, /* nflags */
    NULL,                                   /* sksrc */
};

SeahorseKeyPredicate pred_trusted = {
    0,                                      /* ktype, set later */
    0,                                      /* keyid */
    SKEY_LOC_LOCAL,                         /* location */
    SKEY_PUBLIC,                            /* etype */
    SKEY_FLAG_TRUSTED | SKEY_FLAG_IS_VALID, /* flags */
    0,                                      /* nflags */
    NULL,                                   /* sksrc */
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

SeahorseKeyPredicate pred_password = {
    0,                  /* ktype, set later */
    0,                  /* keyid */
    SKEY_LOC_LOCAL,     /* location */
    SKEY_CREDENTIALS,   /* etype */
    0,                  /* flags */
    0,                  /* nflags */
    NULL,               /* sksrc */
};

#define SEC_RING "/secring.gpg"
#define PUB_RING "/pubring.gpg"

static void selection_changed (GtkTreeSelection *notused, SeahorseWidget *swidget);

/* SIGNAL CALLBACKS --------------------------------------------------------- */

#ifdef WITH_GNOME_KEYRING

/* Loads gnome-keyring keys when first accessed */
static void
load_gkeyring_items (SeahorseWidget *swidget, SeahorseContext *sctx)
{
    SeahorseGKeyringSource *gsrc;
    SeahorseOperation *op;
    
    if (g_object_get_data (G_OBJECT (sctx), "loaded-gnome-keyring-items"))
        return;
    
    g_object_set_data (G_OBJECT (sctx), "loaded-gnome-keyring-items", GUINT_TO_POINTER (TRUE));
    
    gsrc = seahorse_gkeyring_source_new (NULL);
    seahorse_context_take_key_source (sctx, SEAHORSE_KEY_SOURCE (gsrc));
    op = seahorse_key_source_load (SEAHORSE_KEY_SOURCE (gsrc), 0);
    
    /* Monitor loading progress */
    seahorse_progress_status_set_operation (swidget, op);
}

#endif /* WITH_GNOME_KEYRING */

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

    pages = gtk_notebook_get_n_pages (notebook);
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
            selection_changed (NULL, swidget);
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
    case TAB_PASSWORD:
        name = "password-list";
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
    return seahorse_key_manager_store_get_selected_keys (view);
}

static SeahorseKey*
get_selected_key (SeahorseWidget *swidget, guint *uid)
{
    GtkTreeView *view = get_current_view (swidget);
    g_return_val_if_fail (view != NULL, NULL);
    return seahorse_key_manager_store_get_selected_key (view, uid);
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

        seahorse_key_manager_store_set_selected_keys (view, tab_lists[i]);
        g_list_free (tab_lists[i]);
    }
    
    if (last != 0)
        set_current_tab (swidget, last);
}

static void
set_selected_key (SeahorseWidget *swidget, SeahorseKey *skey)
{
    GList *keys = g_list_prepend (NULL, skey);
    set_selected_keys (swidget, keys);
    g_list_free (keys);
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
#ifdef WITH_GNOME_KEYRING
    else if (SEAHORSE_IS_GKEYRING_ITEM (skey))
        seahorse_gkeyring_item_properties_new (SEAHORSE_GKEYRING_ITEM (skey));
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
        seahorse_operation_copy_error (op, &err);
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
    SeahorseMultiOperation *mop;
    SeahorseOperation *op;
    gpgme_data_t data;
    GString *errmsg = NULL;
    GError *err = NULL;
    const gchar **u;
    GQuark ktype;
    
    mop = seahorse_multi_operation_new ();
    errmsg = g_string_new ("");
    
    for (u = uris; *u; u++) {
        
        if(!(*u)[0])
            continue;
        
        /* Figure out where to import to */
        ktype = seahorse_util_detect_file_type (*u);
        if (!ktype) {
            g_string_append_printf (errmsg, "%s: Invalid file format\n", *u);
            continue;
        }

        /* All our supported key types have local */
        sksrc = seahorse_context_find_key_source (SCTX_APP (), ktype, SKEY_LOC_LOCAL);
        g_return_if_fail (sksrc);
        
        /* Load up the file */
        data = seahorse_vfs_data_create (*u, SEAHORSE_VFS_READ, &err);
        if (!data) {
            g_string_append_printf (errmsg, "%s: %s", *u, 
                        err && err->message ? err->message : _("Couldn't read file"));
            continue;
        }
        
        /* data is freed here */
        op = seahorse_key_source_import (sksrc, data);
        g_return_if_fail (op != NULL);
        seahorse_multi_operation_take (mop, op);
    }

    if (seahorse_operation_is_running (SEAHORSE_OPERATION (mop))) {
        seahorse_progress_show (SEAHORSE_OPERATION (mop), _("Importing Keys"), TRUE);
        seahorse_operation_watch (SEAHORSE_OPERATION (mop), G_CALLBACK (imported_keys), NULL, swidget);
    }
    
    /* A running operation refs itself */
    g_object_unref (mop);
        
    if (errmsg->len) {
        seahorse_util_show_error (GTK_WINDOW (seahorse_widget_get_top (swidget)),
                                  _("Couldn't import keys"), errmsg->str);
    }
    
    g_string_free (errmsg, TRUE);
}

static void
import_text (SeahorseWidget *swidget, const gchar *text)
{
    SeahorseKeySource *sksrc;
    SeahorseOperation *op;
    gpgme_data_t data;
    GQuark ktype;
    guint len;
    
    len = strlen (text);
    
    /* Figure out what key format we're dealing with here */
    ktype = seahorse_util_detect_data_type (text, len);
    if (!ktype) {
        seahorse_util_show_error (GTK_WINDOW (seahorse_widget_get_top (swidget)),
                _("Couldn't import keys"), _("Unrecognized key type, or invalid data format"));
        return;
    }
    
    /* All our supported key types have local */
    sksrc = seahorse_context_find_key_source (SCTX_APP (), ktype, SKEY_LOC_LOCAL);
    g_return_if_fail (sksrc && SEAHORSE_IS_PGP_SOURCE (sksrc));

    data = gpgmex_data_new_from_mem (text, strlen (text), TRUE);
    g_return_if_fail (data != NULL);

    /* data is freed here */
    op = seahorse_key_source_import (sksrc, data);
    g_return_if_fail (op != NULL);
    
    seahorse_operation_watch (op, G_CALLBACK (imported_keys), NULL, swidget);
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
  
    keys = get_selected_keys (swidget);
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

/* Loads key properties if a key is selected */
static void
properties_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey = get_selected_key (swidget, NULL);
	if (skey != NULL)
        show_properties (skey);
}

static void
export_done (SeahorseOperation *op, SeahorseWidget *swidget)
{
    gchar *uri;
    GError *err = NULL;
    
    if (!seahorse_operation_is_successful (op)) {
        seahorse_operation_copy_error (op, &err);
        uri = (gchar*)g_object_get_data (G_OBJECT (op), "exported-file-uri");
        seahorse_util_handle_error (err, _("Couldn't export key"),
                                    seahorse_util_uri_get_last (uri));
    }
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
    
    dialog = seahorse_util_chooser_save_new (_("Export public key"), 
                GTK_WINDOW(glade_xml_get_widget (swidget->xml, "key-manager")));
    seahorse_util_chooser_show_key_files (dialog);
    seahorse_util_chooser_set_filename (dialog, keys);
     
    uri = seahorse_util_chooser_save_prompt (dialog);
    if(uri) {
        
        data = seahorse_vfs_data_create (uri, SEAHORSE_VFS_WRITE, &err);
        if (!data) {
            seahorse_util_handle_error (err, _("Couldn't export key to \"%s\""),
                                        seahorse_util_uri_get_last (uri));
            return;
        }
        
        op = seahorse_key_source_export_keys (keys, data);
        g_return_if_fail (op != NULL);
        
        g_object_set_data_full (G_OBJECT (op), "exported-file-uri", uri, g_free);
        g_object_set_data_full (G_OBJECT (op), "exported-file-data", data, 
                                (GDestroyNotify)gpgmex_data_release);
        
        seahorse_progress_show (op, _("Exporting keys"), TRUE);
        seahorse_operation_watch (op, G_CALLBACK (export_done), NULL, swidget);
        
        g_object_unref (op);
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
    
    dialog = seahorse_util_chooser_save_new (_("Back up Keyrings to Archive"), 
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

    skey = get_selected_key (swidget, &uid);
    if (SEAHORSE_IS_PGP_KEY (skey))
        seahorse_sign_show (SEAHORSE_PGP_KEY (skey), uid);
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
        "Milo Casagrande <milo_casagrande@yahoo.it>",
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
                "website", "http://www.gnome.org/projects/seahorse",
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

    skey = seahorse_key_manager_store_get_key_from_path (view, path, NULL);
    if (skey != NULL)
        show_properties (skey);
}

static void
selection_changed (GtkTreeSelection *notused, SeahorseWidget *swidget)
{
    GtkTreeView *view;
    GtkWidget *tab;
    GtkActionGroup *actions;
    GList *keys, *l;
    SeahorseKey *skey = NULL;
    gint ktype = 0;
    gboolean dotracking = TRUE;
    gboolean selected = FALSE;
    gboolean secret = FALSE;
    gint rows = 0;
    GQuark keyid;
    guint tabid;
    
    view = get_current_view (swidget);
    if (view != NULL) {
        GtkTreeSelection *selection = gtk_tree_view_get_selection (view);
        rows = gtk_tree_selection_count_selected_rows (selection);
        selected = rows > 0;
    }
    
    /* See which tab we're on, if different from previous, no tracking */
    tabid = get_tab_id (get_current_tab (swidget));
    if (tabid != GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (swidget), TRACK_SELECTED_TAB))) {
        dotracking = FALSE;
        g_object_set_data (G_OBJECT (swidget), TRACK_SELECTED_TAB, GUINT_TO_POINTER (tabid));
    }
    

    /* Retrieve currently tracked, and reset tracking */
    keyid = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (swidget), TRACK_SELECTED_KEY));
    g_object_set_data (G_OBJECT (swidget), TRACK_SELECTED_KEY, NULL);
    
    /* no selection, see if selected key moved to another tab */
    if (rows == 0 && keyid) {
        
        /* Find it */
        skey = seahorse_context_find_key (SCTX_APP (), keyid, SKEY_LOC_LOCAL);
        if (skey) {
            
            /* If it's still around, then select it */
            tab = get_tab_for_key (swidget, skey);
            if (tab && tab != get_current_tab (swidget)) {

                /* Make sure we don't end up in a loop  */
                g_assert (!g_object_get_data (G_OBJECT (swidget), TRACK_SELECTED_KEY));
                set_selected_key (swidget, skey);
            }
        }
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
        
        /* If one key is selected then mark it down for following across tabs */
        if (g_list_length (keys) == 1) {
            skey = SEAHORSE_KEY (keys->data);
            g_object_set_data (G_OBJECT (swidget), TRACK_SELECTED_KEY, 
                               GUINT_TO_POINTER (seahorse_key_get_keyid (skey)));
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
filter_changed (GtkWidget *widget, SeahorseKeyManagerStore* skstore)
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
    
    /* Don't track the selected key when tab is changed on purpose */
    g_object_set_data (G_OBJECT (swidget), TRACK_SELECTED_KEY, NULL);
    
    selection_changed (NULL, swidget);
    
    /* 
     * Because gnome-keyring can throw prompts etc... we delay loading 
     * of the gnome keyring items until we first access them. 
     */
    
#ifdef WITH_GNOME_KEYRING
    if (get_tab_id (get_current_tab (swidget)) == TAB_PASSWORD)
        load_gkeyring_items (swidget, SCTX_APP ());
#endif
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

static void
view_type_column (GtkToggleAction *action, SeahorseWidget *swidget)
{
    seahorse_gconf_set_boolean (SHOW_TYPE_KEY,
                gtk_toggle_action_get_active (action));
}

static void
view_expiry_column (GtkToggleAction *action, SeahorseWidget *swidget)
{
    seahorse_gconf_set_boolean (SHOW_EXPIRES_KEY,
                gtk_toggle_action_get_active (action));
}

static void
view_validity_column (GtkToggleAction *action, SeahorseWidget *swidget)
{
    seahorse_gconf_set_boolean (SHOW_VALIDITY_KEY,
                gtk_toggle_action_get_active (action));
}

static void
view_trust_column (GtkToggleAction *action, SeahorseWidget *swidget)
{
    seahorse_gconf_set_boolean (SHOW_TRUST_KEY,
                gtk_toggle_action_get_active (action));
}

static void
gconf_notify (GConfClient *client, guint id, GConfEntry *entry, 
              SeahorseWidget *swidget)
{
    GtkActionGroup *actions;
    GtkAction *action;
    const gchar *key;
    
    actions = seahorse_widget_find_actions (swidget, "main");
    g_return_if_fail (actions);
    key = gconf_entry_get_key (entry);

    if (g_str_equal (SHOW_TRUST_KEY, key))
        action = gtk_action_group_get_action (actions, "view-trust");
    else if (g_str_equal (SHOW_TYPE_KEY, key))
        action = gtk_action_group_get_action (actions, "view-type");
    else if (g_str_equal (SHOW_EXPIRES_KEY, key))
        action = gtk_action_group_get_action (actions, "view-expires");
    else if (g_str_equal (SHOW_VALIDITY_KEY, key))
        action = gtk_action_group_get_action (actions, "view-validity");
    else
        return;
    
    g_return_if_fail (action);
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), 
                        gconf_value_get_bool (gconf_entry_get_value (entry)));
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
    { "key-backup", GTK_STOCK_SAVE, N_("_Back up Keyrings..."), "",
            N_("Back up all keys"), G_CALLBACK (backup_activate) }, 
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

static const GtkToggleActionEntry view_entries[] = {
    { "view-type", NULL, N_("T_ypes"), NULL,
             N_("Show type column"), G_CALLBACK (view_type_column), FALSE },
    { "view-expires", NULL, N_("_Expiry"), NULL,
             N_("Show expiry column"), G_CALLBACK (view_expiry_column), FALSE },
    { "view-trust", NULL, N_("_Trust"), NULL,
             N_("Show owner trust column"), G_CALLBACK (view_trust_column), FALSE },
    { "view-validity", NULL, N_("_Validity"), NULL,
             N_("Show validity column"), G_CALLBACK (view_validity_column), FALSE },
};

static const GtkActionEntry key_entries[] = {
    { "key-properties", GTK_STOCK_PROPERTIES, N_("P_roperties"), NULL,
            N_("Show key properties"), G_CALLBACK (properties_activate) }, 
    { "key-export-file", GTK_STOCK_SAVE_AS, N_("E_xport Public Key..."), NULL,
            N_("Export public part of key to a file"), G_CALLBACK (export_activate) }, 
    { "key-export-clipboard", GTK_STOCK_COPY, N_("_Copy Public Key"), "<control>C",
            N_("Copy public part of selected keys to the clipboard"), G_CALLBACK (copy_activate) },
    { "key-delete", GTK_STOCK_DELETE, N_("_Delete Key"), NULL,
            N_("Delete selected keys"), G_CALLBACK (delete_activate) }, 
};

static const GtkActionEntry pgp_entries[] = {
    { "key-sign", GTK_STOCK_INDEX, N_("_Sign..."), NULL,
            N_("Sign public key"), G_CALLBACK (sign_activate) }, 
};

static const GtkActionEntry ssh_entries[] = {
    { "remote-upload-ssh", NULL, N_("Set Up Computer for _Secure Shell..."), "",
            N_("Send public Secure Shell key to another machine, and enable logins using that key."), G_CALLBACK (setup_sshkey_activate) },
};

static const GtkActionEntry keyserver_entries[] = {
    { "remote-find", GTK_STOCK_FIND, N_("_Find Remote Keys..."), "",
            N_("Search for keys on a key server"), G_CALLBACK (search_activate) }, 
    { "remote-sync", GTK_STOCK_REFRESH, N_("_Sync and Publish Keys..."), "",
            N_("Publish and/or synchronize your keys with those online."), G_CALLBACK (sync_activate) }, 
};

void
initialize_tab (SeahorseWidget *swidget, const gchar *tabwidget, guint tabid, 
                const gchar *viewwidget, SeahorseKeyPredicate *pred)
{
    SeahorseKeyset *skset;
    SeahorseKeyManagerStore *skstore;
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
    SeahorseWidget *swidget;
    GtkNotebook *notebook;
    GtkWidget *w;
    GtkWindow *win;
    GtkActionGroup *actions;
    GtkAction *action;
    
    swidget = seahorse_widget_new ("key-manager");
    win = GTK_WINDOW (seahorse_widget_get_top (swidget));
    
    notebook = GTK_NOTEBOOK (seahorse_widget_get_widget (swidget, "notebook"));
    g_return_val_if_fail (notebook != NULL, win);
    
    /* General normal actions */
    actions = gtk_action_group_new ("main");
    gtk_action_group_set_translation_domain (actions, NULL);
    gtk_action_group_add_actions (actions, ui_entries, 
                                  G_N_ELEMENTS (ui_entries), swidget);
    gtk_action_group_add_toggle_actions (actions, view_entries,
                                  G_N_ELEMENTS (view_entries), swidget);
    seahorse_widget_add_actions (swidget, actions);
    
    /* Setup the initial values for the view toggles */
    action = gtk_action_group_get_action (actions, "view-type");
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), seahorse_gconf_get_boolean (SHOW_TYPE_KEY));
    action = gtk_action_group_get_action (actions, "view-trust");
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), seahorse_gconf_get_boolean (SHOW_TRUST_KEY));
    action = gtk_action_group_get_action (actions, "view-expires");
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), seahorse_gconf_get_boolean (SHOW_EXPIRES_KEY));
    action = gtk_action_group_get_action (actions, "view-validity");
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), seahorse_gconf_get_boolean (SHOW_VALIDITY_KEY));
    seahorse_gconf_notify_lazy (LISTING_SCHEMAS, (GConfClientNotifyFunc)gconf_notify, swidget, swidget);
    
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
    g_signal_connect_after (notebook, "switch-page", G_CALLBACK(tab_changed), swidget);
    
    /* Initialize the tabs, and associate them up */
    initialize_tab (swidget, "pub-key-tab", TAB_PUBLIC, "pub-key-list", &pred_public);
    initialize_tab (swidget, "trust-key-tab", TAB_TRUSTED, "trust-key-list", &pred_trusted);
    initialize_tab (swidget, "sec-key-tab", TAB_PRIVATE, "sec-key-list", &pred_private);

#ifdef WITH_GNOME_KEYRING
    initialize_tab (swidget, "password-tab", TAB_PASSWORD, "password-list", &pred_password);
#else
    {
        guint page;
        w = seahorse_widget_get_widget (swidget, "password-tab");
        g_return_val_if_fail (w, win);
        page = gtk_notebook_page_num (notebook, w);
        g_return_val_if_fail (page != -1, win);
        gtk_notebook_remove_page (notebook, page);
    }
#endif
    
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
    seahorse_progress_status_set_operation (swidget, op);

    /* To show first time dialog */
    g_timeout_add (1000, (GSourceFunc)first_timer, swidget);
    
    selection_changed (NULL, swidget);
    
    return win;
}
