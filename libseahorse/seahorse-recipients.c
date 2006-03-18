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
#include "seahorse-pgp-key.h"
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

static void
filter_activated (GtkEntry *entry, GtkWidget *widget)
{
    gtk_widget_grab_focus (widget);
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

static void
keyset_changed (SeahorseKeyset *skset, SeahorseWidget *swidget)
{
    GtkWidget *widget = glade_xml_get_widget (swidget->xml, "sign_box");
    g_return_if_fail (widget != NULL);
    
    if (seahorse_keyset_get_count (skset) == 0)
        gtk_widget_hide (widget);
    else
        gtk_widget_show (widget);
}

static void
widget_destroyed (SeahorseWidget *swidget, SeahorseKeyset *skset)
{
    g_signal_handlers_disconnect_by_func (skset, keyset_changed, swidget);
}

GList*
seahorse_recipients_get (SeahorsePGPKey **signer)
{
    SeahorseWidget *swidget;
    GtkTreeSelection *selection;
    GtkTreeView *view;
    GtkWidget *widget;
    GtkWidget *combo;
    gint response;
    gboolean done = FALSE;
    GList *keys = NULL;
    gchar *id;
    SeahorseKey *skey;
    SeahorseKeyStore *skstore;
    SeahorseKeyset * skset;
    
    swidget = seahorse_widget_new_allow_multiple ("recipients");
    g_return_val_if_fail (swidget != NULL, NULL);
    
    view = GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, VIEW));
    selection = gtk_tree_view_get_selection (view);
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    g_signal_connect (selection, "changed",
                G_CALLBACK (selection_changed), swidget);

    /* If always using the default key for signing, then hide this section */
    if (!signer) {
        
        widget = glade_xml_get_widget (swidget->xml, "sign_box");
        gtk_widget_hide (widget);

    /* Signing section */
    } else {
        
        skset = seahorse_keyset_pgp_signers_new ();
        combo = glade_xml_get_widget (swidget->xml, "signer-select");
        g_return_val_if_fail (combo != NULL, NULL);
        seahorse_combo_keys_attach (GTK_OPTION_MENU (combo), skset, _("None (Don't sign)"));
        
        /* Control visibility of box when keys or no keys exist */
        g_signal_connect (skset, "set-changed", G_CALLBACK (keyset_changed), swidget);
        g_signal_connect (swidget, "destroy", G_CALLBACK (widget_destroyed), skset);
        keyset_changed (skset, swidget);
        
        g_object_unref (skset);
        
        /* Select the last key used */
        id = seahorse_gconf_get_string (LASTSIGNER_KEY);
        seahorse_combo_keys_set_active_id (GTK_OPTION_MENU (combo), id);
        g_free (id); 
    }
    
    skset = seahorse_keyset_new (SKEY_PGP, 0, 0, 0, 0);
    skstore = seahorse_recipients_store_new (skset, view);
   
    glade_xml_signal_connect_data (swidget->xml, "on_mode_changed", 
                              G_CALLBACK (mode_changed), skstore);
    glade_xml_signal_connect_data (swidget->xml, "on_filter_changed",
                              G_CALLBACK (filter_changed), skstore);
    glade_xml_signal_connect_data (swidget->xml, "on_filter_activate",
                              G_CALLBACK (filter_activated), view);

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
        
        *signer = SEAHORSE_PGP_KEY (seahorse_combo_keys_get_active (GTK_OPTION_MENU (combo)));
        g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (*signer), keys);

        /* Save this as the last key signed with */
        seahorse_gconf_set_string (LASTSIGNER_KEY, *signer == NULL ? 
                        "" : seahorse_key_get_keyid (SEAHORSE_KEY (*signer)));
    }
    
    seahorse_widget_destroy (swidget);
    return keys;
}
