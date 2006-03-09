/*
 * Seahorse
 *
 * Copyright (C) 2004 Nate Nielsen
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

#include <sys/types.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gnome.h>
#include <glade/glade-xml.h>

#include "eggtrayicon.h"
#include "seahorse-pgp-key.h"
#include "seahorse-agent.h"
#include "seahorse-context.h"
#include "seahorse-widget.h"
#include "seahorse-secure-memory.h"

/* 
 * Implements tray icon, menu and status window. Tray icon ideas 
 * came from gaim
 */

/* For the docklet icon */
static EggTrayIcon *g_docklet = NULL;
static GtkWidget *g_image = NULL;

/* For the popup window */
static SeahorseWidget *g_window = NULL;
static SeahorseContext *g_sctx = NULL;

/* -----------------------------------------------------------------------------
 *  Popup Window
 */

enum {
    KEY_COLUMN,
    N_COLUMNS
};

/* Called to close status window */
void
window_destroy ()
{
    if (g_window) {
        seahorse_widget_destroy (g_window);
        g_window = NULL;
    }
}

/* When window close clicked we close window */
static int
delete_event (GtkWidget *widget, GdkEvent *event, gpointer data)
{
    window_destroy ();
    return FALSE;
}

/* When close button clicked */
static void
close_clicked (GtkButton *button, SeahorseWidget *swidget)
{
    window_destroy ();
}

/* Clear button, clear cache and close */
static void
clear_clicked (GtkButton *button, SeahorseWidget *swidget)
{
    seahorse_agent_cache_clearall ();
    window_destroy ();
}

/* Add a row to the tree for a given password */
static void
add_row_for_key (gpointer id, gpointer reserved, gpointer user_data)
{
    GtkTreeStore *store = GTK_TREE_STORE (user_data);
    GtkTreeIter iter;
    gchar *userid;

    /* Add a new row to the model */
    gtk_tree_store_append (store, &iter, NULL);
    
    userid = seahorse_agent_cache_getname ((const gchar *) id);
    gtk_tree_store_set (store, &iter, KEY_COLUMN,
                        userid, -1);
    g_free (userid);
}

/* Called when the cache changes and window is open */
static void
window_update_keys ()
{
    GtkTreeViewColumn *column;
    GtkTreeStore *store;
    GtkTreeView *tree;

    g_assert (g_window != NULL);
    tree = GTK_TREE_VIEW (glade_xml_get_widget (g_window->xml, "key_list"));
    g_assert (tree != NULL);

    store = GTK_TREE_STORE (gtk_tree_view_get_model (tree));
    if (!store) {
        /* This is our first time so create a store */
        store = gtk_tree_store_new (N_COLUMNS, G_TYPE_STRING);
        gtk_tree_view_set_model (tree, GTK_TREE_MODEL (store));

        /* Make the column */
        column = gtk_tree_view_column_new_with_attributes (_("Cached Encryption Keys"),
                                                           gtk_cell_renderer_text_new
                                                           (), "text", KEY_COLUMN,
                                                           NULL);
        gtk_tree_view_append_column (tree, column);
    } else {
        /* Clear the store we refill below */
        gtk_tree_store_clear (store);
    }

    /* Add all the keys */
    seahorse_agent_cache_enum (add_row_for_key, store);
    g_object_unref (G_OBJECT (store));
}

/* Display the status window */
static void
window_show ()
{
    GtkWidget *w;
    GtkImage *img;
    
    if (g_window) {
        w = glade_xml_get_widget (g_window->xml, g_window->name);
        gtk_window_present (GTK_WINDOW (w));
        return;
    }

    if (!g_sctx)
        g_sctx = seahorse_context_new (TRUE, SKEY_PGP);
    
    g_window = seahorse_widget_new ("agent-cache");

    w = glade_xml_get_widget (g_window->xml, g_window->name);


    img = GTK_IMAGE (gtk_image_new_from_stock (GTK_STOCK_DIALOG_AUTHENTICATION, 
                                                GTK_ICON_SIZE_DIALOG));
    gtk_window_set_icon (GTK_WINDOW (w), gtk_image_get_pixbuf (img));

    g_signal_connect (G_OBJECT (w), "delete_event", G_CALLBACK (delete_event), NULL);

    glade_xml_signal_connect_data (g_window->xml, "close_clicked",
                                   G_CALLBACK (close_clicked), g_window);
    glade_xml_signal_connect_data (g_window->xml, "clear_clicked",
                                   G_CALLBACK (clear_clicked), g_window);

    /* The secure memory warning */
    if (!seahorse_secure_memory_have ()) {
        GdkColor color = { 0, 0xffff, 0, 0 };
        w = glade_xml_get_widget (g_window->xml, "insecure_label");
        gtk_widget_modify_fg (w, GTK_STATE_NORMAL, &color);
        gtk_widget_show (w);
    }

    window_update_keys ();
}

/* -----------------------------------------------------------------------------
 *  Tray Icon Code
 */

/* Menu item for clearing cache */
static void
on_clear_cache_activate (GtkWidget *item, gpointer data)
{
    seahorse_agent_cache_clearall ();
}

/* Menu item for showing status */
static void
on_show_window_activate (GtkWidget *item, gpointer data)
{
    window_show ();
}

static void
on_settings_activate (GtkWidget *item, gpointer data)
{
    GError *err = NULL;
    g_spawn_command_line_async ("seahorse-preferences --cache", &err);
    
    if (err != NULL) {
        g_warning ("couldn't execute seahorse-preferences: %s", err->message);
        g_error_free (err);
    }
}

/* Called when icon destroyed */
static void
tray_destroyed (GtkWidget *widget, void *data)
{
    g_object_unref (G_OBJECT (g_docklet));
    g_docklet = NULL;
}

/* Called when icon clicked */
static void
tray_clicked (GtkWidget *button, GdkEventButton *event, void *data)
{
    if (event->type != GDK_BUTTON_PRESS)
        return;

    /* Right click, show menu */
    if (event->button == 3) {
        GtkWidget *menu;
        GladeXML *xml;
        xml =
            glade_xml_new (SEAHORSE_GLADEDIR "seahorse-agent-cache.glade",
                           "context-menu", NULL);
        menu = glade_xml_get_widget (xml, "context-menu");
        glade_xml_signal_connect_data (xml, "on_clear_cache_activate",
                                       G_CALLBACK (on_clear_cache_activate), NULL);
        glade_xml_signal_connect_data (xml, "on_show_window_activate",
                                       G_CALLBACK (on_show_window_activate), NULL);
        glade_xml_signal_connect_data (xml, "on_settings_activate",
                                       G_CALLBACK (on_settings_activate), NULL);

        gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                        event->button, gtk_get_current_event_time ());
        gtk_widget_show (menu);
        g_object_unref (xml);
    }

    /* Left click show status */
    else if (event->button == 1) {
        window_show ();
    }
}

/* Remove tray icon */
static void
docklet_destroy ()
{
    if (g_docklet) {
        g_signal_handlers_disconnect_by_func (G_OBJECT (g_docklet),
                                              G_CALLBACK (tray_destroyed), NULL);
        gtk_widget_destroy (GTK_WIDGET (g_docklet));

        g_object_unref (G_OBJECT (g_docklet));
        g_docklet = NULL;
    }
}

static void
docklet_create ()
{
    GtkWidget *box;

    if (g_docklet) {
        /* 
         * If this is being called when a tray icon exists, it's 
         * because something messed up. try destroying it before 
         * we proceed, although docklet_refcount may be all hosed. 
         * hopefully won't happen. 
         */

        g_warning ("trying to create icon but it already exists");
        docklet_destroy ();
    }

    g_docklet = egg_tray_icon_new ("seahorse-agent");
    box = gtk_event_box_new ();

    /* 
     * TODO: Is loading an external file a security risk? It may well be
     * with all the image vulnerabilities going around. We may want to 
     * encode this image and include in the code.
     */

    g_image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_AUTHENTICATION, 
                                        GTK_ICON_SIZE_LARGE_TOOLBAR);

    g_signal_connect (G_OBJECT (g_docklet), "destroy", G_CALLBACK (tray_destroyed),
                      NULL);
    g_signal_connect (G_OBJECT (box), "button-press-event",
                      G_CALLBACK (tray_clicked), NULL);

    gtk_container_add (GTK_CONTAINER (box), g_image);
    gtk_container_add (GTK_CONTAINER (g_docklet), box);

    if (!gtk_check_version (2, 4, 0))
        g_object_set (G_OBJECT (box), "visible-window", FALSE, NULL);

    gtk_widget_show_all (GTK_WIDGET (g_docklet));

    /* ref the docklet before we bandy it about the place */
    g_object_ref (G_OBJECT (g_docklet));
}

/* Called when quiting */
void
seahorse_agent_status_cleanup ()
{
    docklet_destroy ();

    if (g_window)
        window_destroy ();
}

/* Cache calls this when changes occur */
void
seahorse_agent_status_update ()
{
    gboolean have = (seahorse_agent_cache_count () > 0);

    if (have && !g_docklet)
        docklet_create ();

    else if (!have && g_docklet)
        docklet_destroy ();

    if (g_window)
        window_update_keys ();
}
