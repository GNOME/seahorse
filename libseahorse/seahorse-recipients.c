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
	
	list = seahorse_key_store_get_selected_keys (GTK_TREE_VIEW (
		glade_xml_get_widget (swidget->xml, VIEW)));
	selected = g_list_length (list);
	
	for (keys = list; keys != NULL; keys = g_list_next (keys)) {
		if (seahorse_key_get_validity (keys->data) < SEAHORSE_VALIDITY_FULL)
			invalid++;
	}
	
	gnome_appbar_set_status (GNOME_APPBAR (glade_xml_get_widget (swidget->xml, "status")),
		g_strdup_printf (_("Selected %d not fully valid keys and %d fully valid keys"),
			invalid, selected - invalid));
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, OK), selected > 0);
	
	g_list_free (list);
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
	
	swidget = seahorse_widget_new ("recipients", sctx);
	g_return_val_if_fail (swidget != NULL, NULL);
	
	g_signal_connect (swidget->sctx, "progress", G_CALLBACK (show_progress), swidget);
	
	view = GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, VIEW));
	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (selection, "changed",
		G_CALLBACK (selection_changed), swidget);
	seahorse_recipients_store_new (sctx, view);
	
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
	
	seahorse_widget_destroy (swidget);
	return recips;
}
