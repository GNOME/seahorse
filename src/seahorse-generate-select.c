/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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

#include "seahorse-widget.h"
#include "seahorse-util.h"
#include "seahorse-key-dialogs.h"
#include "seahorse-gtkstock.h"

#include "pgp/seahorse-pgp-key.h"
#include "ssh/seahorse-ssh-key.h"

enum {
    KEY_TYPE,
    KEY_ICON,
    KEY_TEXT,
    KEY_N_COLUMNS
};

const GType key_columns[] = {
    G_TYPE_UINT,    /* type */
    G_TYPE_STRING,  /* icon */
    G_TYPE_STRING,  /* text */
};


static void
choose_keytype(SeahorseWidget *swidget)
{
    GtkWidget *widget;
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    SeahorseKeySource *sksrc;
    GtkTreeIter iter;
    GQuark ktype;

    /* Figure out the key type selected */
    widget = seahorse_widget_get_widget (swidget, "keytype-tree");
    g_return_if_fail (widget != NULL);
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
    if (!gtk_tree_selection_get_selected (selection, &model, &iter))
        g_return_if_reached ();
    gtk_tree_model_get (model, &iter, KEY_TYPE, &ktype, -1);
    
    /* Get an appropriate key source */
    sksrc = seahorse_context_find_key_source (SCTX_APP (), ktype, SKEY_LOC_LOCAL);
    g_return_if_fail (sksrc != NULL);
    
    if (ktype == SEAHORSE_PGP)
        seahorse_pgp_generate_show (SEAHORSE_PGP_SOURCE (sksrc), GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)));
        
#ifdef WITH_SSH 
    else if (ktype == SEAHORSE_SSH)
        seahorse_ssh_generate_show (SEAHORSE_SSH_SOURCE (sksrc), GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)));
#endif 
    
    else
        g_return_if_reached ();

    seahorse_widget_destroy (swidget);
}

static void
row_activated(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *arg2, 
              SeahorseWidget *swidget)
{
    choose_keytype(swidget);     
}

void
on_response (GtkDialog *dialog, guint response, SeahorseWidget *swidget)
{
    if (response != GTK_RESPONSE_OK) {
        seahorse_widget_destroy (swidget);
        return;
    }
    
    choose_keytype(swidget);
}

static void
add_key_type (GtkListStore *store, GQuark type, const char *stock_icon, 
              const char *text, const char *desc)
{
    GtkTreeIter iter;
    gchar *t;
    
    t = g_strdup_printf("<span size=\"larger\" weight=\"bold\">%s</span>\n%s", 
                        text, desc);
    
    /* PGP Key */
    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter, 
                        KEY_TYPE, type,
                        KEY_ICON, stock_icon,
                        KEY_TEXT, t,
                        -1);
    
    g_free (t);
}

void
seahorse_generate_select_show (GtkWindow *parent)
{
    SeahorseWidget *swidget;
    GtkListStore *store;
    GtkCellRenderer *renderer;
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    GtkWidget *widget;
    
    swidget = seahorse_widget_new ("generate-select", parent);
    
    /* Widget already exists */
    if (swidget == NULL)
        return;
    
    /* TODO: Once we're rid of the "Advanced" option, we can skip this
       whole dialog and cut straight to the chase if only one key type */
    
    /* Build our tree store */
    store = gtk_list_store_newv (KEY_N_COLUMNS, (GType*)key_columns);
    add_key_type (store, SEAHORSE_PGP, SEAHORSE_STOCK_SECRET, _("PGP Key"), 
                  _("Used to encrypt email and files"));
#ifdef WITH_SSH
    add_key_type (store, SEAHORSE_SSH, SEAHORSE_STOCK_KEY_SSH, _("Secure Shell Key"),
                  _("Used to access other computers (eg: via a terminal)"));
#endif /* WITH_SSH */
    
    /* Hook it into the view */
    widget = seahorse_widget_get_widget (swidget, "keytype-tree");
    g_return_if_fail (widget != NULL);
    
    /* Make the columns for the view */
    renderer = gtk_cell_renderer_pixbuf_new ();
    g_object_set (renderer, "stock-size", GTK_ICON_SIZE_DIALOG, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
                                                 -1, "", renderer,
                                                 "stock-id", KEY_ICON, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
                                                 -1, "", gtk_cell_renderer_text_new (), 
                                                 "markup", KEY_TEXT, NULL);
    gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL (store));
    
    /* Setup selection, select first item */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
    gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter);
    gtk_tree_selection_select_iter (selection, &iter);
    
    g_signal_connect (seahorse_widget_get_top (swidget), "response", 
                      G_CALLBACK (on_response), swidget);
    g_signal_connect (widget, "row_activated", 
                      G_CALLBACK(row_activated), swidget);                  
}
