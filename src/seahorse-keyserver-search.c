/*
 * Seahorse
 *
 * Copyright (C) 2004-2005 Nate Nielsen
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
#include "seahorse-gpgmex.h"
#include "seahorse-util.h"
#include "seahorse-context.h"
#include "seahorse-windows.h"
#include "seahorse-preferences.h"
#include "seahorse-server-source.h"
#include "seahorse-multi-source.h"

static void
start_keyserver_search (SeahorseMultiSource *msrc, SeahorseKeySource *lsksrc,
                        const gchar *keyserver, const gchar *search)
{
    SeahorseKeySource *sksrc;
    
    sksrc = SEAHORSE_KEY_SOURCE (seahorse_server_source_new (lsksrc, keyserver, search));
    g_return_if_fail (sksrc != NULL);
    
    seahorse_multi_source_add (msrc, sksrc, FALSE);
}

static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
    SeahorseKeySource *lsksrc;
    SeahorseMultiSource *msrc;
    const gchar *search;
    GSList *ks, *l;
	GtkWidget *w;
            
    w = glade_xml_get_widget (swidget->xml, "search-text");
    g_return_if_fail (w != NULL);
    
    search = gtk_entry_get_text (GTK_ENTRY (w));
    g_return_if_fail (search != NULL && search[0] != 0);

    eel_gconf_set_string (LASTSEARCH_KEY, search);
    
    /* This should be the default key source */
    lsksrc = seahorse_context_get_key_source (swidget->sctx);
    g_return_if_fail (lsksrc != NULL);

    /* Container for all the remote sources */
    msrc = seahorse_multi_source_new ();
            
    /* The default key server or null for all */
    ks = eel_gconf_get_string_list (KEYSERVER_KEY);

    for (l = ks; l; l = g_slist_next (l))
        start_keyserver_search (msrc, lsksrc, (const gchar*)(l->data), search);
    seahorse_util_string_slist_free (ks);
    
    /* Open the new result window */    
    seahorse_keyserver_results_show (swidget->sctx, SEAHORSE_KEY_SOURCE (msrc), search);

    seahorse_widget_destroy (swidget);
}

static void
configure_clicked (GtkButton *button, SeahorseWidget *swidget)
{
    seahorse_preferences_show (swidget->sctx, 3);
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
   
    entry_changed (GTK_EDITABLE (w), swidget);   
    glade_xml_signal_connect_data (swidget->xml, "search_changed",
                                   G_CALLBACK (entry_changed), swidget);
    
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		                           G_CALLBACK (ok_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "configure_clicked",
		                           G_CALLBACK (configure_clicked), swidget);

    return win;
}
