/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins, Adam Schreiber
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

#include <gnome.h>
#include <eel/eel.h>

#include "seahorse-widget.h"
#include "seahorse-util.h"
#include "seahorse-gpgmex.h"
#include "seahorse-context.h"
#include "seahorse-windows.h"
#include "seahorse-server-source.h"
#include "seahorse-multi-source.h"

#define KEYSERVER_LIST  "keyserver-list"
#define NOTIFY_ID       "notify-id"

/* Forward declaration */
static void populate_combo (SeahorseWidget *swidget, gboolean first);

static void
start_keyserver_search (SeahorseMultiSource *msrc, SeahorseKeySource *lsksrc,
                        const gchar *keyserver, const gchar *search)
{
    SeahorseServerSource *ssrc;
    
    ssrc = seahorse_server_source_new (lsksrc, keyserver);
    seahorse_multi_source_add (msrc, SEAHORSE_KEY_SOURCE (ssrc), FALSE);

    seahorse_server_source_search (ssrc, search);
}

static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
    SeahorseKeySource *lsksrc;
    SeahorseMultiSource *msrc;
    const gchar *keyserver;
    const gchar *search;
    GSList *ks;
	GtkWidget *w;
    gint n;
        
    w = glade_xml_get_widget (swidget->xml, "keyservers");
    g_return_if_fail (w != NULL);
    
    ks = (GSList*)g_object_get_data (G_OBJECT (w), KEYSERVER_LIST);
    n = gtk_combo_box_get_active (GTK_COMBO_BOX (w));
    g_return_if_fail (n >= 0);
    
    if (n > 0)
        keyserver = g_slist_nth_data (ks, n - 1);
    else 
        keyserver = NULL; /* all key servers */
        
    w = glade_xml_get_widget (swidget->xml, "search-text");
    g_return_if_fail (w != NULL);
    
    search = gtk_entry_get_text (GTK_ENTRY (w));
    g_return_if_fail (search != NULL && search[0] != 0);

    eel_gconf_set_string (LASTSERVER_KEY, keyserver ? keyserver : "");
    eel_gconf_set_string (LASTSEARCH_KEY, search);
    
    /* This should be the default key source */
    lsksrc = seahorse_context_get_key_source (swidget->sctx);
    g_return_if_fail (lsksrc != NULL);

    /* Container for all the remote sources */
    msrc = seahorse_multi_source_new ();

    /* Open the new result window */    
    seahorse_keyserver_results_show (swidget->sctx, SEAHORSE_KEY_SOURCE (msrc), search);
            
    /* The default key server or null for all */
    if (keyserver != NULL && keyserver[0] != 0) {
        start_keyserver_search (msrc, lsksrc, keyserver, search);

    /* ... search on all key servers */
    } else {
        for (; ks; ks = g_slist_next (ks))
            start_keyserver_search (msrc, lsksrc, (const gchar*)(ks->data), search);
    }

    seahorse_widget_destroy (swidget);
}

static void
entry_changed (GtkEditable *editable, SeahorseWidget *swidget)
{
    GtkWidget *w;
    gchar *text;
  
    text = gtk_editable_get_chars (editable, 0, -1);
    w = glade_xml_get_widget (swidget->xml, "search");
    
    gtk_widget_set_sensitive (w, text && strlen(text) > 0);
    g_free (text);
}

void 
combo_destroyed (GObject *object, gpointer user_data)
{
    GSList *ks;
    guint notify_id;
    
    ks = (GSList*)g_object_get_data (object, KEYSERVER_LIST);
    g_object_set_data (object, KEYSERVER_LIST, NULL);

    notify_id = GPOINTER_TO_INT (g_object_get_data (object, NOTIFY_ID));
    g_object_set_data (object, NOTIFY_ID, NULL);
        
    seahorse_util_string_slist_free (ks);
    
    if (notify_id >= 0)
        eel_gconf_notification_remove (notify_id);
}

static void
gconf_notify (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
    SeahorseWidget *swidget;

    if (g_str_equal (KEYSERVER_KEY, gconf_entry_get_key (entry))) {
        swidget = SEAHORSE_WIDGET (data);   
        populate_combo (swidget, FALSE);    
    }
}

static void
populate_combo (SeahorseWidget *swidget, gboolean first)
{
    GSList *ks, *l;
    gint i, n;
    gchar *chosen = NULL;
    guint notify_id;
    GtkComboBox *combo;
    
    g_return_if_fail (SEAHORSE_IS_WIDGET (swidget));
    
    combo = GTK_COMBO_BOX (glade_xml_get_widget (swidget->xml, "keyservers"));
    g_return_if_fail (combo != NULL);
    
    ks = (GSList*)g_object_get_data (G_OBJECT (combo), KEYSERVER_LIST);

    if (first) {

        /* The default key server or null for all */
        chosen = eel_gconf_get_string (LASTSERVER_KEY);
        
        /* On first call add a callback to free string list */
        g_signal_connect (combo, "destroy", G_CALLBACK (combo_destroyed), NULL);
        
        notify_id = eel_gconf_notification_add (KEYSERVER_KEY, gconf_notify, swidget);
        g_object_set_data (G_OBJECT (combo), NOTIFY_ID, GINT_TO_POINTER (notify_id));
        
    /* Not first time */
    } else {
           
        i = g_slist_length (ks);
        
        /* Get the selection so we can set it back */
        n = gtk_combo_box_get_active (combo);
        if (n > 0 && n <= i)
            chosen = g_strdup ((gchar*)g_slist_nth_data (ks, n - 1));
        
        seahorse_util_string_slist_free (ks);
        
        /* Remove saved data */
        while (i-- >= 0)
            gtk_combo_box_remove_text (combo, 0);
        
    }
        
    /* The all key servers option */
    gtk_combo_box_prepend_text (combo, _("All Key Servers"));
    
    /* The data, sorted */
    ks = eel_gconf_get_string_list (KEYSERVER_KEY);
    ks = g_slist_sort (ks, (GCompareFunc)g_utf8_collate);
    
    for (n = 0, i = 1, l = ks; l != NULL; l = g_slist_next (l), i++) {
        if (chosen && g_utf8_collate ((gchar*)l->data, chosen) == 0)
            n = i;
        gtk_combo_box_append_text (combo, (gchar*)l->data);
    }
    
    g_free (chosen);
    gtk_combo_box_set_active (combo, n);
        
    /* We save the list for later use */
    g_object_set_data (G_OBJECT (combo), KEYSERVER_LIST, ks);
}

/**
 * seahorse_keyserver_search_show
 * @sctx: The SeahorseContext
 * 
 * Shows a remote search window.
 * 
 * Returns the new window.
 **/
GtkWindow*
seahorse_keyserver_search_show (SeahorseContext *sctx)
{	
	SeahorseWidget *swidget;
    GtkWindow *win;
    GtkWidget *w;
    gchar *search;
        
	swidget = seahorse_widget_new ("keyserver-search", sctx);
	g_return_val_if_fail (swidget != NULL, NULL);
 
    win = GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name));

    w = glade_xml_get_widget (swidget->xml, "search-text");
    g_return_val_if_fail (w != NULL, win);

    search = eel_gconf_get_string (LASTSEARCH_KEY);
    if (search != NULL) {
        gtk_entry_set_text (GTK_ENTRY (w), search);
        g_free (search);
    }
   
    populate_combo(swidget, TRUE);

    entry_changed (GTK_EDITABLE (w), swidget);   
    glade_xml_signal_connect_data (swidget->xml, "search_changed",
                                   G_CALLBACK (entry_changed), swidget);
    
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		                           G_CALLBACK (ok_clicked), swidget);
                                   
    return win;
}
