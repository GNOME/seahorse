/*
 * Seahorse
 *
 * Copyright (C) 2007 Stefan Walter
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
#include <gnome-keyring.h>

#include "seahorse-context.h"
#include "seahorse-prefs.h"
#include "seahorse-util.h"
#include "seahorse-gtkstock.h"

enum {
    KEYRING_MARKUP,
    KEYRING_COMMENTS,
    KEYRING_NAME,
    KEYRING_N_COLUMNS
};

static void
update_wait_cursor (GtkWidget *tab, gpointer unused)
{
    GdkCursor *cursor;
    
    g_return_if_fail (tab->window);
    
    /* No request active? */
    if (!g_object_get_data (G_OBJECT (tab), "keyring-request")) {
        gdk_window_set_cursor (tab->window, NULL);
        return;
    }
    
    /* 
     * Get the wait cursor. Create a new one and cache it on the widget 
     * if first time.
     */
    cursor = (GdkCursor*)g_object_get_data (G_OBJECT (tab), "wait-cursor");
    if (!cursor) {
        cursor = gdk_cursor_new (GDK_WATCH);
        g_object_set_data_full (G_OBJECT (tab), "wait-cursor", cursor, 
                                (GDestroyNotify)gdk_cursor_unref);
    }
    
    /* Indicate that we're loading stuff */
    gdk_window_set_cursor (tab->window, cursor);
}

static void
clear_request (SeahorseWidget *swidget, gboolean cancel)
{
    GtkWidget *tab;
    gpointer request;
    
    tab = seahorse_widget_get_widget (swidget, "keyring-tab");
    g_return_if_fail (GTK_IS_WIDGET (tab));

    request = g_object_steal_data (G_OBJECT (tab), "keyring-request");
    if (request && cancel)
        gnome_keyring_cancel_request (request);
        
    if (GTK_WIDGET_REALIZED (tab))
        update_wait_cursor (tab, NULL);
    gtk_widget_set_sensitive (tab, TRUE);
}

static void
setup_request (SeahorseWidget *swidget, gpointer request)
{
    GtkWidget *tab;

    g_return_if_fail (request);
    
    tab = seahorse_widget_get_widget (swidget, "keyring-tab");
    g_return_if_fail (GTK_IS_WIDGET (tab));
    
    /* Cancel any old operation going on */
    clear_request (swidget, TRUE);
    
    /* 
     * Start the operation and tie it to the widget so that it will get 
     * cancelled if the widget is destroyed before the operation is complete
     */ 
    g_object_set_data_full (G_OBJECT (tab), "keyring-request", request, 
                            gnome_keyring_cancel_request); 
    
    if (GTK_WIDGET_REALIZED (tab))
        update_wait_cursor (tab, NULL);
    else
        g_signal_connect (tab, "realize", G_CALLBACK (update_wait_cursor), tab);
    
    gtk_widget_set_sensitive (tab, FALSE);    
}

static void
combo_set_active_with_updating (GtkComboBox *combo, GtkTreeIter *iter)
{
    g_object_set_data (G_OBJECT (combo), "updating", "updating");
    if (iter)
        gtk_combo_box_set_active_iter (combo, iter);
    else
        gtk_combo_box_set_active (combo, -1);
    g_object_set_data (G_OBJECT (combo), "updating", NULL);
}

static void
update_default_keyring (SeahorseWidget *swidget)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkWidget *tab;
    GtkComboBox *combo;
    const gchar* keyring;
    gboolean match, valid;
    gchar *t;
    
    tab = seahorse_widget_get_widget (swidget, "keyring-tab");
    g_return_if_fail (GTK_IS_WIDGET (tab));
    
    combo = GTK_COMBO_BOX (seahorse_widget_get_widget (swidget, "default-keyring"));
    g_return_if_fail (GTK_IS_COMBO_BOX (combo)); 
    
    keyring = (const gchar*)g_object_get_data (G_OBJECT (tab), "default-keyring");
    if (keyring && !keyring[0])
        keyring = NULL;
        
    /* No default keyring somehow */
    if (!keyring) {
        combo_set_active_with_updating (combo, NULL);
        return;
    }

    model = gtk_combo_box_get_model (combo);
    g_return_if_fail (combo);    
    
    /* Find the default keyring and select it */
    valid = gtk_tree_model_get_iter_first (model, &iter);
    while (valid) {
        gtk_tree_model_get (model, &iter, KEYRING_NAME, &t, -1);
        g_assert (t);
        
        match = (strcmp (t, keyring) == 0);
        g_free (t);
         
        if (match) {
            combo_set_active_with_updating (combo, &iter);
            return;
        }
        
        valid = gtk_tree_model_iter_next (model, &iter);
    }
    
    /* Nothing was selected */
    combo_set_active_with_updating (combo, NULL);
}

static void
refresh_keyring_model (SeahorseWidget *swidget, GList *list)
{
    GtkTreeModel *model;
    GtkListStore *store;
    GtkTreeView *tree;
    const gchar *name, *comments;
    GtkTreeIter iter;
    gboolean valid;
    
    tree = GTK_TREE_VIEW (seahorse_widget_get_widget (swidget, "keyring-list"));
    g_return_if_fail (GTK_IS_TREE_VIEW (tree));
    
    /* Create a new model if necessary */
    model = gtk_tree_view_get_model (tree);
    g_return_if_fail (model);
    store = GTK_LIST_STORE (model);

    /* Fill in the tree store, replacing any rows already present */
    valid = gtk_tree_model_get_iter_first (model, &iter);
    for ( ; list; list = g_list_next (list)) {
    
        /* We don't list the 'session' keyring */
        name = (gchar*)list->data;
        if (strcmp (name, "session") == 0)
            continue;

        if (!valid)
            gtk_list_store_append (store, &iter);

        /* Figure out the name and markup */
        if (strcmp (name, "login") == 0)
            comments = _("<i>Automatically unlocked when user logs in.</i>");
        else
            comments = "";
        
        gtk_list_store_set (store, &iter, KEYRING_NAME, name, KEYRING_MARKUP, name, 
                            KEYRING_COMMENTS, comments, -1);
        
        if (valid)
            valid = gtk_tree_model_iter_next (model, &iter);
    }
    
    /* Remove all the remaining rows */
    while (valid)
        valid = gtk_list_store_remove (store, &iter);
}

static void
refreshed_default (GnomeKeyringResult result, const gchar* name, gpointer data)
{
    SeahorseWidget *swidget;
    GtkWidget *tab;

    swidget = SEAHORSE_WIDGET (data);
    g_return_if_fail (swidget);

    /* Clear the operation without cancelling it since it is complete */
    clear_request (swidget, FALSE);
    
    tab = seahorse_widget_get_widget (swidget, "keyring-tab");
    g_return_if_fail (GTK_IS_WIDGET (tab));

    if (result == GNOME_KEYRING_RESULT_OK) { 
        g_object_set_data_full (G_OBJECT (tab), "default-keyring", g_strdup (name), g_free);
        update_default_keyring (swidget);
        
    } else if (result != GNOME_KEYRING_RESULT_CANCELLED) {
        seahorse_util_show_error (GTK_WINDOW (seahorse_widget_get_top (swidget)),
                                  _("Couldn't get default password keyring"),
                                  gnome_keyring_result_to_message (result));
    }    
}    

static void
refreshed_keyrings (GnomeKeyringResult result, GList *list, gpointer data)
{
    SeahorseWidget *swidget;
    gpointer request;

    swidget = SEAHORSE_WIDGET (data);
    g_return_if_fail (swidget);

    /* Clear the operation without cancelling it since it is complete */
    clear_request (swidget, FALSE);
    
    if (result == GNOME_KEYRING_RESULT_OK) { 
        refresh_keyring_model (swidget, list);
        update_default_keyring (swidget);
        
    } else if (result != GNOME_KEYRING_RESULT_CANCELLED) {
        seahorse_util_show_error (GTK_WINDOW (seahorse_widget_get_top (swidget)),
                                  _("Couldn't get list of password keyrings"),
                                  gnome_keyring_result_to_message (result));
    }
    
    /* 
     * Start the operation and tie it to the widget so that it will get 
     * cancelled if the widget is destroyed before the operation is complete
     */ 
    request = gnome_keyring_get_default_keyring (refreshed_default, swidget, NULL);
    g_return_if_fail (request);
    setup_request (swidget, request);
}

static void
refresh_keyrings (SeahorseWidget *swidget)
{
    gpointer request;
    
    /* 
     * Start the operation and tie it to the widget so that it will get 
     * cancelled if the widget is destroyed before the operation is complete
     */ 
    request = gnome_keyring_list_keyring_names (refreshed_keyrings, swidget, NULL);
    g_return_if_fail (request);
    setup_request (swidget, request);
}

static void
keyring_default_done (GnomeKeyringResult result, gpointer data)
{
    SeahorseWidget *swidget;
    SeahorseKeySource *src;

    swidget = SEAHORSE_WIDGET (data);
    g_return_if_fail (swidget);

    /* Clear the operation without cancelling it since it is complete */
    clear_request (swidget, FALSE);
    
    /* Successful. Update the listings and stuff. */
    if (result == GNOME_KEYRING_RESULT_OK) {
        refresh_keyrings (swidget);
        
        /* 
         * Update the main seahorse gkeyring source 
         * TODO: We shouldn't have to define SEAHORSE_GKR here again
         */
        src = seahorse_context_find_key_source (SCTX_APP (), 
                                                g_quark_from_static_string ("gnome-keyring"), 
                                                SKEY_LOC_LOCAL);
        if (src)
            seahorse_key_source_load_async (src, 0);
        
    /* Setting the default keyring failed */
    } else if (result != GNOME_KEYRING_RESULT_CANCELLED) {     
        seahorse_util_show_error (GTK_WINDOW (seahorse_widget_get_top (swidget)),
                                  _("Couldn't set default password keyring"),
                                  gnome_keyring_result_to_message (result));
    }
}

static void
keyring_default_changed (GtkComboBox *combo, SeahorseWidget *swidget)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar* keyring;
    gpointer request;
    
    /* Change originated with us */
    if (g_object_get_data (G_OBJECT (combo), "updating"))
        return;

    if (!gtk_combo_box_get_active_iter (combo, &iter))
        return;    

    model = gtk_combo_box_get_model (combo);
    gtk_tree_model_get (model, &iter, KEYRING_NAME, &keyring, -1);
    g_return_if_fail (keyring);
    
    request = gnome_keyring_set_default_keyring (keyring, keyring_default_done, swidget, NULL);
    g_free (keyring);
    
    setup_request (swidget, request);
}

static void 
keyring_selection_changed (GtkTreeSelection *selection, SeahorseWidget *swidget)
{
    gint selected;
    selected = gtk_tree_selection_count_selected_rows (selection);
    seahorse_widget_set_sensitive (swidget, "add-keyring", TRUE);
    seahorse_widget_set_sensitive (swidget, "remove-keyring", selected > 0);
    seahorse_widget_set_sensitive (swidget, "password-keyring", selected > 0);
}

static gchar* 
selected_keyring (SeahorseWidget *swidget)
{
    GtkTreeSelection *selection;
    GtkTreeView *tree;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *keyring;
    
    tree = GTK_TREE_VIEW (seahorse_widget_get_widget (swidget, "keyring-list"));
    g_return_val_if_fail (tree, NULL);
    
    selection = gtk_tree_view_get_selection (tree);
    g_return_val_if_fail (tree, NULL);
    
    if (!gtk_tree_selection_get_selected (selection, &model, &iter))
        return NULL;
        
    gtk_tree_model_get (model, &iter, KEYRING_NAME, &keyring, -1);
    g_return_val_if_fail (keyring, NULL);
    
    return keyring;
}

static void
keyring_remove_done (GnomeKeyringResult result, gpointer data)
{
    SeahorseWidget *swidget;

    swidget = SEAHORSE_WIDGET (data);
    g_return_if_fail (swidget);

    /* Clear the operation without cancelling it since it is complete */
    clear_request (swidget, FALSE);
    
    /* Successful. Update the listings and stuff. */
    if (result == GNOME_KEYRING_RESULT_OK) {
        refresh_keyrings (swidget);
        
    /* Setting the default keyring failed */
    } else if (result != GNOME_KEYRING_RESULT_CANCELLED) {     
        seahorse_util_show_error (GTK_WINDOW (seahorse_widget_get_top (swidget)),
                                  _("Couldn't remove keyring"),
                                  gnome_keyring_result_to_message (result));
    }
}

static void 
keyring_remove_clicked (GtkButton *button, SeahorseWidget *swidget)
{
    GtkWidget *question;
    GtkWidget *action;
    gchar *keyring;
    gpointer request;

    keyring = selected_keyring (swidget);
    g_return_if_fail (keyring);

    question = gtk_message_dialog_new (GTK_WINDOW (seahorse_widget_get_top (swidget)), 
                                       GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                       _("Are you sure you want to permanently delete the '%s' keyring?"), 
                                       keyring);
                                       
    action = gtk_button_new_from_stock (GTK_STOCK_DELETE);
    gtk_dialog_add_action_widget (GTK_DIALOG (question), GTK_WIDGET (action), GTK_RESPONSE_ACCEPT);
    gtk_widget_show (action);

    action = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
    gtk_dialog_add_action_widget (GTK_DIALOG (question), GTK_WIDGET (action), GTK_RESPONSE_CANCEL);
    gtk_widget_show (action);
    
    if (gtk_dialog_run (GTK_DIALOG (question)) == GTK_RESPONSE_ACCEPT) {
        request = gnome_keyring_delete (keyring, keyring_remove_done, swidget, NULL);
        g_return_if_fail (request);
        setup_request (swidget, request);
    }
    
    gtk_widget_destroy (question);
    g_free (keyring);
}

static void
keyring_name_changed (GtkEntry *entry, SeahorseWidget *swadd)
{
    const gchar *keyring;
    
    keyring = gtk_entry_get_text (entry);
    seahorse_widget_set_sensitive (swadd, "ok", keyring && keyring[0]);
}

static void
keyring_add_done (GnomeKeyringResult result, gpointer data)
{
    SeahorseWidget *swidget;

    swidget = SEAHORSE_WIDGET (data);
    g_return_if_fail (swidget);

    /* Clear the operation without cancelling it since it is complete */
    clear_request (swidget, FALSE);
    
    /* Successful. Update the listings and stuff. */
    if (result == GNOME_KEYRING_RESULT_OK) {
        refresh_keyrings (swidget);
        
    /* Setting the default keyring failed */
    } else if (result != GNOME_KEYRING_RESULT_CANCELLED) {     
        seahorse_util_show_error (GTK_WINDOW (seahorse_widget_get_top (swidget)),
                                  _("Couldn't add keyring"),
                                  gnome_keyring_result_to_message (result));
    }
}

static void 
keyring_add_clicked (GtkButton *button, SeahorseWidget *swidget)
{
    SeahorseWidget *swadd;
    const gchar *keyring;
    gpointer request;
    GtkEntry *entry;
    gint response;
    
    swadd = seahorse_widget_new_allow_multiple ("add-keyring",
                                                GTK_WINDOW (seahorse_widget_get_top (swidget)));
    g_return_if_fail (swadd);
    
    entry = GTK_ENTRY (seahorse_widget_get_widget (swadd, "keyring-name"));
    g_return_if_fail (entry); 
    
    glade_xml_signal_connect_data (swadd->xml, "keyring_name_changed", 
                                   G_CALLBACK (keyring_name_changed), swadd);
    keyring_name_changed (entry, swadd);

    response = gtk_dialog_run (GTK_DIALOG (seahorse_widget_get_top (swadd)));
    if (response == GTK_RESPONSE_ACCEPT) {
        
        keyring = gtk_entry_get_text (entry);
        g_return_if_fail (keyring && keyring[0]);
        
        request = gnome_keyring_create (keyring, NULL, keyring_add_done, swidget, NULL);
        g_return_if_fail (request);
        setup_request (swidget, request);
    }
    
    seahorse_widget_destroy (swadd);        
}

static void
keyring_password_done (GnomeKeyringResult result, gpointer data)
{
    SeahorseWidget *swidget;

    swidget = SEAHORSE_WIDGET (data);
    g_return_if_fail (swidget);

    /* Clear the operation without cancelling it since it is complete */
    clear_request (swidget, FALSE);
    
    /* Successful. Update the listings and stuff. */
    if (result == GNOME_KEYRING_RESULT_OK) {
        refresh_keyrings (swidget);
        
    /* Setting the default keyring failed */
    } else if (result != GNOME_KEYRING_RESULT_CANCELLED) {     
        seahorse_util_show_error (GTK_WINDOW (seahorse_widget_get_top (swidget)),
                                  _("Couldn't change keyring password"),
                                  gnome_keyring_result_to_message (result));
    }
}

static void 
keyring_password_clicked (GtkButton *button, SeahorseWidget *swidget)
{
    gpointer request;
    gchar *keyring;
    
    keyring = selected_keyring (swidget);
    
    request = gnome_keyring_change_password (keyring, NULL, NULL, 
                                             keyring_password_done, swidget, NULL);
    g_free (keyring);
    
    g_return_if_fail (request);
    setup_request (swidget, request);
}

void
seahorse_prefs_keyrings (SeahorseWidget *swidget)
{
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkListStore *store;
    GtkComboBox *combo;
    GtkTreeView *tree;
    GtkTreeSelection *selection;
    
    store = gtk_list_store_new (KEYRING_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    tree = GTK_TREE_VIEW (seahorse_widget_get_widget (swidget, "keyring-list"));
    g_return_if_fail (GTK_IS_TREE_VIEW (tree));
    gtk_tree_view_set_model (tree, GTK_TREE_MODEL (store));
    
      /* Markup column */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("name", renderer, 
                                                       "markup", KEYRING_MARKUP, NULL);
    gtk_tree_view_append_column (tree, column);        

      /* Markup column */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("comments", renderer, 
                                                       "markup", KEYRING_COMMENTS, NULL);
    gtk_tree_view_append_column (tree, column);        

    selection = gtk_tree_view_get_selection (tree);
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    g_signal_connect (selection, "changed", G_CALLBACK (keyring_selection_changed), swidget);
    keyring_selection_changed (selection, swidget);
    

    combo = GTK_COMBO_BOX (seahorse_widget_get_widget (swidget, "default-keyring"));
    g_return_if_fail (GTK_IS_COMBO_BOX (combo));
    gtk_combo_box_set_model (combo, GTK_TREE_MODEL (store));

    gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo));
    renderer = gtk_cell_renderer_text_new ();
        
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), renderer, "markup", KEYRING_MARKUP);                            

    g_signal_connect (combo, "changed", G_CALLBACK (keyring_default_changed), swidget);

    glade_xml_signal_connect_data (swidget->xml, "keyring_remove_clicked",
            G_CALLBACK (keyring_remove_clicked), swidget);
    glade_xml_signal_connect_data (swidget->xml, "keyring_add_clicked",
            G_CALLBACK (keyring_add_clicked), swidget);
    glade_xml_signal_connect_data (swidget->xml, "keyring_password_clicked",
            G_CALLBACK (keyring_password_clicked), swidget);
            
    refresh_keyrings (swidget);
}
