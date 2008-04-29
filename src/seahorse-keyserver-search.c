/*
 * Seahorse
 *
 * Copyright (C) 2004-2005 Stefan Walter
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

#include "seahorse-widget.h"
#include "seahorse-util.h"
#include "seahorse-context.h"
#include "seahorse-windows.h"
#include "seahorse-preferences.h"
#include "seahorse-gconf.h"
#include "seahorse-context.h"
#include "seahorse-dns-sd.h"

#include "pgp/seahorse-server-source.h"

typedef struct _KeyserverSelection {
    GSList *names;
    GSList *uris;
    gboolean all;
} KeyserverSelection;

/* Selection Retrieval ------------------------------------------------------ */

static void
get_checks (GtkWidget *widget, KeyserverSelection *selection)
{
    if (GTK_IS_CHECK_BUTTON (widget)) {
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
            /* Pull in the selected name and uri */
            selection->names = g_slist_prepend (selection->names, 
                    g_strdup (gtk_button_get_label (GTK_BUTTON (widget))));
            selection->uris = g_slist_prepend (selection->uris, 
                    g_strdup (g_object_get_data (G_OBJECT (widget), "keyserver-uri")));
        } else {
            /* Note that not all checks are selected */
            selection->all = FALSE;
        }
    }
}

static KeyserverSelection*
get_keyserver_selection (SeahorseWidget *swidget)
{
    KeyserverSelection *selection;
    GtkWidget *w;
    
    selection = g_new0(KeyserverSelection, 1);
    selection->all = TRUE;
    
    /* Key servers */
    w = glade_xml_get_widget (swidget->xml, "key-server-list");
    g_return_val_if_fail (w != NULL, selection);
    gtk_container_foreach (GTK_CONTAINER (w), (GtkCallback)get_checks, selection);

    /* Shared Key */
    w = glade_xml_get_widget (swidget->xml, "shared-keys-list");
    g_return_val_if_fail (w != NULL, selection);
    gtk_container_foreach (GTK_CONTAINER (w), (GtkCallback)get_checks, selection);    
    
    return selection;
}

static void
free_keyserver_selection (KeyserverSelection *selection)
{
    if (selection) {
        seahorse_util_string_slist_free (selection->uris);
        seahorse_util_string_slist_free (selection->names);
        g_free (selection);
    }
}

static void
have_checks (GtkWidget *widget, gboolean *checked)
{
    if (GTK_IS_CHECK_BUTTON (widget)) {
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
            *checked = TRUE;
    }
}

static gboolean
have_keyserver_selection (SeahorseWidget *swidget)
{
    GtkWidget *w;
    gboolean checked = FALSE;
    
    /* Key servers */
    w = glade_xml_get_widget (swidget->xml, "key-server-list");
    g_return_val_if_fail (w != NULL, FALSE);
    gtk_container_foreach (GTK_CONTAINER (w), (GtkCallback)have_checks, &checked);

    /* Shared keys */
    w = glade_xml_get_widget (swidget->xml, "shared-keys-list");
    g_return_val_if_fail (w != NULL, FALSE);
    gtk_container_foreach (GTK_CONTAINER (w), (GtkCallback)have_checks, &checked);        
    
    return checked;
}

static void
control_changed (GtkWidget *widget, SeahorseWidget *swidget)
{
    gboolean enabled = TRUE;
    GtkWidget *w;
    gchar *text;
    
    /* Need to have at least one key server selected ... */
    if (!have_keyserver_selection (swidget))
        enabled = FALSE;
    
    /* ... and some search text */
    else {
        w = glade_xml_get_widget (swidget->xml, "search-text");
        text = gtk_editable_get_chars (GTK_EDITABLE (w), 0, -1);
        if (!text || !text[0])
            enabled = FALSE;
        g_free (text);
    }
        
    w = glade_xml_get_widget (swidget->xml, "search");
    gtk_widget_set_sensitive (w, enabled);
}

/* Initial Selection -------------------------------------------------------- */

static void
select_checks (GtkWidget *widget, GSList *names)
{
    if (GTK_IS_CHECK_BUTTON (widget)) {

        gchar *t = g_utf8_casefold (gtk_button_get_label (GTK_BUTTON (widget)), -1);
        gboolean checked = !names || 
                    g_slist_find_custom (names, t, (GCompareFunc)g_utf8_collate);
        g_free (t);
        
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), checked);
    }
}

static void
select_inital_keyservers (SeahorseWidget *swidget)
{
    GSList *l, *names;
    GtkWidget *w;
    
    names = seahorse_gconf_get_string_list (LASTSERVERS_KEY);

    /* Close the expander if all servers are selected */
    w = glade_xml_get_widget (swidget->xml, "search-where");
    g_return_if_fail (w != NULL);
    gtk_expander_set_expanded (GTK_EXPANDER (w), names != NULL);
    
    /* We do case insensitive matches */    
    for (l = names; l; l = g_slist_next (l)) {
        gchar *t = g_utf8_casefold (l->data, -1);
        g_free (l->data);
        l->data = t;
    }
    
    w = glade_xml_get_widget (swidget->xml, "key-server-list");
    g_return_if_fail (w != NULL);
    gtk_container_foreach (GTK_CONTAINER (w), (GtkCallback)select_checks, names);

    w = glade_xml_get_widget (swidget->xml, "shared-keys-list");
    g_return_if_fail (w != NULL);
    gtk_container_foreach (GTK_CONTAINER (w), (GtkCallback)select_checks, names);
}

/* Populating Lists --------------------------------------------------------- */

static void
remove_checks (GtkWidget *widget, GHashTable *unchecked)
{
    if (GTK_IS_CHECK_BUTTON (widget)) {
    
        if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) 
            g_hash_table_replace (unchecked, 
                g_strdup (gtk_button_get_label (GTK_BUTTON (widget))), "");
        
        gtk_widget_destroy (widget);
    }
}

static void
populate_keyserver_list (SeahorseWidget *swidget, GtkWidget *box, GSList *uris, 
                         GSList *names)
{
    GtkContainer *cont = GTK_CONTAINER (box);
    GHashTable *unchecked;
    gboolean any = FALSE;
    GtkWidget *check;
    GtkTooltips *tooltips;
    GSList *l, *n;
    
    tooltips = GTK_TOOLTIPS (g_object_get_data (G_OBJECT (box), "tooltips"));
    if (!tooltips) {
        tooltips = gtk_tooltips_new ();

        /* Special little reference counting for 'floating' reference */
        g_object_ref (tooltips);
        gtk_object_sink (GTK_OBJECT (tooltips));
        
        g_object_set_data_full (G_OBJECT (box), "tooltips", tooltips, (GDestroyNotify)g_object_unref);
    }
    
    /* Remove all checks, and note which ones were unchecked */
    unchecked = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    gtk_container_foreach (cont, (GtkCallback)remove_checks, unchecked);
    
    /* Now add the new ones back */
    for (l = uris, n = names; l && n; l = g_slist_next (l), n = g_slist_next (n)) {
        any = TRUE;

        /* A new checkbox with this the name as the label */        
        check = gtk_check_button_new_with_label ((const gchar*)n->data);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), 
                                      g_hash_table_lookup (unchecked, (gchar*)n->data) == NULL);
        g_signal_connect (check, "toggled", G_CALLBACK (control_changed), swidget);
        gtk_widget_show (check);

        /* Save URI and set it as the tooltip */
        g_object_set_data_full (G_OBJECT (check), "keyserver-uri", g_strdup ((gchar*)l->data), g_free);
        gtk_tooltips_set_tip (tooltips, check, (gchar*)l->data, "");
        
        gtk_container_add (cont, check);
    }

    g_hash_table_destroy (unchecked);   

    /* Only display the container if we had some checks */
    if (any)
        gtk_widget_show (box);
    else
        gtk_widget_hide (box);
}

static void
refresh_keyservers (GConfClient *client, guint id, GConfEntry *entry, SeahorseWidget *swidget)
{
    GSList *keyservers, *names;
    GtkWidget *w;

	if (entry && !g_str_equal (KEYSERVER_KEY, gconf_entry_get_key (entry)))
        return;

    w = glade_xml_get_widget (swidget->xml, "key-server-list");
    g_return_if_fail (w != NULL);

    keyservers = seahorse_gconf_get_string_list (KEYSERVER_KEY);
    names = seahorse_server_source_parse_keyservers (keyservers);
    populate_keyserver_list (swidget, w, keyservers, names);
    
    seahorse_util_string_slist_free (keyservers);
    seahorse_util_string_slist_free (names);        
}

static void 
refresh_shared_keys (SeahorseServiceDiscovery *ssd, const gchar *name, SeahorseWidget *swidget)
{
    GSList *keyservers, *names;
    GtkWidget *w;
    
    w = glade_xml_get_widget (swidget->xml, "shared-keys-list");
    g_return_if_fail (w != NULL);

    names = seahorse_service_discovery_list (ssd);
    keyservers = seahorse_service_discovery_get_uris (ssd, names);
    populate_keyserver_list (swidget, w, keyservers, names);

    seahorse_util_string_slist_free (keyservers);
    seahorse_util_string_slist_free (names);    
}

/* -------------------------------------------------------------------------- */
 
static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
    SeahorseOperation *op;
    KeyserverSelection *selection;
    const gchar *search;
	GtkWidget *w;
            
    w = glade_xml_get_widget (swidget->xml, "search-text");
    g_return_if_fail (w != NULL);
    
    /* Get search text and save it for next time */
    search = gtk_entry_get_text (GTK_ENTRY (w));
    g_return_if_fail (search != NULL && search[0] != 0);
    seahorse_gconf_set_string (LASTSEARCH_KEY, search);
    
    /* The keyservers to search, and save for next time */    
    selection = get_keyserver_selection (swidget);
    g_return_if_fail (selection->uris != NULL);
    seahorse_gconf_set_string_list (LASTSERVERS_KEY, 
                                    selection->all ? NULL : selection->names);
                                    
    op = seahorse_context_load_remote_keys (SCTX_APP(), search);
    
    /* Open the new result window */    
    seahorse_keyserver_results_show (op, 
                                     GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)), 
                                     search);

    free_keyserver_selection (selection);
    seahorse_widget_destroy (swidget);
}

static void
configure_clicked (GtkButton *button, SeahorseWidget *swidget)
{
    seahorse_preferences_show (GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)), "keyserver-tab");
}

static void
cleanup_signals (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseServiceDiscovery *ssd = seahorse_context_get_discovery (SCTX_APP());
    g_signal_handlers_disconnect_by_func (ssd, refresh_shared_keys, swidget);
}

/**
 * seahorse_keyserver_search_show
 * 
 * Shows a remote search window.
 * 
 * Returns the new window.
 **/
GtkWindow*
seahorse_keyserver_search_show (GtkWindow *parent)
{	
    SeahorseServiceDiscovery *ssd;
	SeahorseWidget *swidget;
    GtkWindow *win;
    GtkWidget *w;
    gchar *search;
        
	swidget = seahorse_widget_new ("keyserver-search", parent);
	g_return_val_if_fail (swidget != NULL, NULL);
 
    win = GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name));

    w = glade_xml_get_widget (swidget->xml, "search-text");
    g_return_val_if_fail (w != NULL, win);

    search = seahorse_gconf_get_string (LASTSEARCH_KEY);
    if (search != NULL) {
        gtk_entry_set_text (GTK_ENTRY (w), search);
        g_free (search);
    }
   
    glade_xml_signal_connect_data (swidget->xml, "search_changed",
                                   G_CALLBACK (control_changed), swidget);
    
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		                           G_CALLBACK (ok_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "configure_clicked",
		                           G_CALLBACK (configure_clicked), swidget);

    /* The key servers to list */
    refresh_keyservers (NULL, 0, NULL, swidget);
    w = glade_xml_get_widget (swidget->xml, swidget->name);
    seahorse_gconf_notify_lazy (KEYSERVER_KEY, (GConfClientNotifyFunc)refresh_keyservers, 
                                swidget, GTK_WIDGET (win));
    
    /* Any shared keys to list */    
    ssd = seahorse_context_get_discovery (SCTX_APP ());
    refresh_shared_keys (ssd, NULL, swidget);
    g_signal_connect (ssd, "added", G_CALLBACK (refresh_shared_keys), swidget);
    g_signal_connect (ssd, "removed", G_CALLBACK (refresh_shared_keys), swidget);
    g_signal_connect (win, "destroy", G_CALLBACK (cleanup_signals), swidget);
    
    select_inital_keyservers (swidget);
    control_changed (NULL, swidget);       
    
    return win;
}
