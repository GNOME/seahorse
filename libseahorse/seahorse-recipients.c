/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#include "seahorse-libdialogs.h"
#include "seahorse-widget.h"
#include "seahorse-validity.h"
#include "seahorse-recipients-store.h"

#define VIEW "keys"
#define OK "ok"

static void
show_progress (SeahorseContext *sctx, const gchar *op, gdouble fract, SeahorseWidget *swidget)
{
	GnomeAppBar *status;
	GtkProgressBar *progress;
	gboolean sensitive;

	sensitive = (fract == -1);
	
	status = GNOME_APPBAR (glade_xml_get_widget (swidget->xml, "status"));
	gnome_appbar_set_status (status, op);
	progress = gnome_appbar_get_progress (status);
	/* do progress */
	if (fract <= 1 && fract > 0)
		gtk_progress_bar_set_fraction (progress, fract);
	else if (fract != -1) {
		gtk_progress_bar_set_pulse_step (progress, 0.05);
		gtk_progress_bar_pulse (progress);
	}
	/* if fract == -1, cleanup progress */
	else
		gtk_progress_bar_set_fraction (progress, 0);
	
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, OK), sensitive);
	
	while (g_main_context_pending (NULL))
		g_main_context_iteration (NULL, TRUE);
}

static void
selection_changed (GtkTreeSelection *selection, SeahorseWidget *swidget)
{
	GList *list = NULL, *keys = NULL;
	gint selected = 0, invalid = 0;
	gchar* msg;
   
	list = seahorse_key_store_get_selected_keys (GTK_TREE_VIEW (
		glade_xml_get_widget (swidget->xml, VIEW)));
	selected = g_list_length (list);
	
	for (keys = list; keys != NULL; keys = g_list_next (keys)) {
		if (seahorse_key_get_validity (keys->data) < SEAHORSE_VALIDITY_FULL)
			invalid++;
	}
 
    if(invalid == 0)
    {
        msg = g_strdup_printf(
            ngettext(_("Selected %d keys"), _("Selected %d keys"), selected), selected);
    }
    
    else 
    {
        msg = g_strdup_printf(
            ngettext(_("Selected %d not fully valid key"), _("Selected %d keys (%d not fully valid)"), selected), selected, invalid);
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

gpgme_key_t *
seahorse_recipients_get (SeahorseContext *sctx)
{
	SeahorseWidget *swidget;
	GtkTreeSelection *selection;
	GtkTreeView *view;
	GtkWidget *widget;
	gint response;
	gboolean done = FALSE;
	gpgme_key_t * recips = NULL;
    SeahorseKeyStore* skstore;
	
	swidget = seahorse_widget_new ("recipients", sctx);
	g_return_val_if_fail (swidget != NULL, NULL);
	
	g_signal_connect (swidget->sctx, "progress", G_CALLBACK (show_progress), swidget);
	
	view = GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, VIEW));
	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (selection, "changed",
		G_CALLBACK (selection_changed), swidget);
        
	skstore = seahorse_recipients_store_new (sctx, view);
   
    glade_xml_signal_connect_data (swidget->xml, "on_mode_changed", 
                              G_CALLBACK (mode_changed), skstore);
    glade_xml_signal_connect_data (swidget->xml, "on_filter_changed",
                              G_CALLBACK (filter_changed), skstore);

    g_signal_connect (skstore, "notify", G_CALLBACK (update_filters), swidget);
    update_filters (G_OBJECT (skstore), NULL, swidget);

    /* Start loading keys here */
    seahorse_context_get_keys (sctx);
                                        
	widget = glade_xml_get_widget (swidget->xml, swidget->name);
	while (!done) {
		response = gtk_dialog_run (GTK_DIALOG (widget));
		switch (response) {
			case GTK_RESPONSE_HELP:
				break;
			case GTK_RESPONSE_OK:
				recips = seahorse_key_store_get_selected_recips (view);
			default:
				done = TRUE;
				break;
		}
	}
	
    g_signal_handlers_disconnect_by_func (swidget->sctx, G_CALLBACK (show_progress), swidget);	
	seahorse_widget_destroy (swidget);
	return recips;
}
