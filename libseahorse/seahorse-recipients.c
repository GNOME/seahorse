/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#include <stdlib.h>
#include <libintl.h>
#include <gnome.h>

#include "seahorse-operation.h"
#include "seahorse-progress.h"
#include "seahorse-libdialogs.h"
#include "seahorse-widget.h"
#include "seahorse-validity.h"
#include "seahorse-recipients-store.h"
#include "seahorse-default-key-control.h"
#include "seahorse-gconf.h"

#define VIEW "keys"
#define OK "ok"

static void
selection_changed (GtkTreeSelection *selection, SeahorseWidget *swidget)
{
	GList *list = NULL, *keys = NULL;
	gint selected = 0, invalid = 0;
	gchar *msg, *s1, *s2;
   
	list = seahorse_key_store_get_selected_keys (GTK_TREE_VIEW (
		glade_xml_get_widget (swidget->xml, VIEW)));
	selected = g_list_length (list);
	
	for (keys = list; keys != NULL; keys = g_list_next (keys)) {
		if (seahorse_key_get_validity (keys->data) < SEAHORSE_VALIDITY_FULL)
			invalid++;
	}
 
    if(invalid == 0) {
        msg = g_strdup_printf(
            ngettext("Selected %d recipient", "Selected %d recipients", selected), selected);
            
    /* For translators */
    } else if (invalid == selected) {
        msg = g_strdup_printf (ngettext("Selected %d not fully valid recipient", 
                                        "Selected %d not fully valid recipients", selected), 
                               selected);

    } else {
       
        /* TRANSLATOR: This string will become
          * "Selected %d recipients (%d not fully valid)" */
        s1 = g_strdup_printf(ngettext("Selected %d recipient ", "Selected %d recipients ", selected),
                             selected);

        /* TRANSLATOR: This string will become
         * "Selected %d recipients (%d not fully valid)" */
        s2 = g_strdup_printf(ngettext("(%d not fully valid)", "(%d not fully valid)", invalid),
                             invalid);

        /* TRANSLATOR: "%s%s" are "Selected %d recipients (%d not fully valid)"
         * Swap order with "%2$s%1$s" if needed */
        msg = g_strdup_printf(_("%s%s"), s1, s2);  
        
        g_free (s1);
        g_free (s2);     
    }
        
	gnome_appbar_set_status (GNOME_APPBAR (glade_xml_get_widget (swidget->xml, "status")), msg);
    g_free(msg);

	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, OK), selected > 0);
	
	g_list_free (list);
}

/* Called when mode dropdown changes */
static void
mode_changed (GtkWidget *widget, SeahorseKeyStore* skstore)
{
    gint active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
    if (active >= 0)
        g_object_set (skstore, "mode", active, NULL);
}

/* Called when filter text box changes */
static void
filter_changed (GtkWidget *widget, SeahorseKeyStore* skstore)
{
    const gchar* text = gtk_entry_get_text (GTK_ENTRY (widget));
    g_object_set (skstore, "filter", text, NULL);
}

/* Called when properties on the SeahorseKeyStore object change */
static void
update_filters (GObject* object, GParamSpec* arg, SeahorseWidget* swidget)
{
    guint mode;
    gchar* filter;
    GtkWidget* w;
    
   	/* Refresh combo box */
    g_object_get (object, "mode", &mode, "filter", &filter, NULL);
    w = glade_xml_get_widget (swidget->xml, "mode");
    gtk_combo_box_set_active (GTK_COMBO_BOX (w), mode);
    
    /* Refresh the text filter */
    w = glade_xml_get_widget (swidget->xml, "filter");
    gtk_entry_set_text (GTK_ENTRY (w), filter ? filter : "");

    g_free (filter);                                                
}

GList*
seahorse_recipients_get (SeahorseContext *sctx, SeahorseKeyPair **signer)
{
	SeahorseWidget *swidget;
    SeahorseOperation *operation;
    SeahorseDefaultKeyControl *sdkc;
	GtkTreeSelection *selection;
	GtkTreeView *view;
	GtkWidget *widget;
	gint response;
	gboolean done = FALSE;
    GList *keys = NULL;
    gchar *id;
    SeahorseKeyStore* skstore;
    SeahorseKeySource *sksrc;
	
	swidget = seahorse_widget_new ("recipients", sctx);
	g_return_val_if_fail (swidget != NULL, NULL);
	
	view = GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, VIEW));
	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (selection, "changed",
		G_CALLBACK (selection_changed), swidget);
        
    sksrc = seahorse_context_get_key_source (sctx);
    g_return_val_if_fail (sksrc != NULL, NULL);

    /* Hook progress bar in */
    operation = seahorse_key_source_get_operation (sksrc);
    g_return_val_if_fail (operation != NULL, NULL);

    /* If always using the default key for signing, then hide this section */
    if (!signer || (*signer = seahorse_context_get_default_key (sctx)) != NULL) {
        widget = glade_xml_get_widget (swidget->xml, "sign_box");
        gtk_widget_hide (widget);

    /* Signing section */
    } else {
        widget = glade_xml_get_widget (swidget->xml, "sign_key_place");
        sdkc = seahorse_default_key_control_new (sksrc, _("None (Don't sign)"));
        gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (sdkc));
        gtk_widget_show_all (widget);    

        /* Select the last key used */
        id = seahorse_gconf_get_string (LASTSIGNER_KEY);
        seahorse_default_key_control_select_id (sdkc, id);
        g_free (id); 
    }
    
    widget = glade_xml_get_widget (swidget->xml, "status");
    seahorse_progress_appbar_set_operation (widget, operation);
        
	skstore = seahorse_recipients_store_new (sksrc, view);
   
    glade_xml_signal_connect_data (swidget->xml, "on_mode_changed", 
                              G_CALLBACK (mode_changed), skstore);
    glade_xml_signal_connect_data (swidget->xml, "on_filter_changed",
                              G_CALLBACK (filter_changed), skstore);

    g_signal_connect (skstore, "notify", G_CALLBACK (update_filters), swidget);
    update_filters (G_OBJECT (skstore), NULL, swidget);

	widget = seahorse_widget_get_top (swidget);
    seahorse_widget_show (swidget);
    
	while (!done) {
		response = gtk_dialog_run (GTK_DIALOG (widget));
		switch (response) {
			case GTK_RESPONSE_HELP:
				break;
			case GTK_RESPONSE_OK:
				keys = seahorse_key_store_get_selected_keys (view);
			default:
				done = TRUE;
				break;
		}
	}

    if (keys && signer) {
        if (!*signer) 
            *signer = seahorse_default_key_control_active (sdkc);

        /* Save this as the last key signed with */
        seahorse_gconf_set_string (LASTSIGNER_KEY, *signer == NULL ? 
                        "" : seahorse_key_pair_get_id (*signer));
    }
    
	seahorse_widget_destroy (swidget);
	return keys;
}
