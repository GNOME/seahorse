/*
 * Seahorse
 *
 * Copyright (C) 2002 Jacob Perkins
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
#include <gpgme.h>

#include "seahorse-key-manager.h"
#include "seahorse-widget.h"
#include "seahorse-key.h"
#include "seahorse-generate.h"
#include "seahorse-export.h"
#include "seahorse-delete.h"
#include "seahorse-preferences.h"
#include "seahorse-import.h"
#include "seahorse-text-editor.h"
#include "seahorse-key-properties.h"
#include "seahorse-util.h"
#include "seahorse-validity.h"
#include "seahorse-key-manager-store.h"
#include "seahorse-file-manager.h"

#define KEY_MANAGER "key-manager"
#define KEY_LIST "key_list"

/* Quits seahorse */
static void
quit (GtkWidget *widget, SeahorseWidget *swidget)
{	
	seahorse_context_destroy (swidget->sctx);
	gtk_exit (0);
}

/* Quits seahorse */
static gboolean
delete_event (GtkWidget *widget, GdkEvent *event, SeahorseWidget *swidget)
{
	quit (widget, swidget);
	return TRUE;
}

/* Loads generate dialog */
static void
generate_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_generate_show (swidget->sctx);
}

/* Loads export dialog if a key is selected */
static void
export_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, KEY_LIST)));
	if (skey)
		seahorse_export_new (swidget->sctx, skey);
}

/* Loads import dialog */
static void
import_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_import_show (swidget->sctx);
}

/* Loads text editor */
static void
text_editor_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_text_editor_show (swidget->sctx);
}

/* Loads file manager */
static void
file_manager_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_file_manager_show (swidget->sctx);
}

/* Loads key properties if a key is selected */
static void
properties_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, KEY_LIST)));
	if (skey)
		seahorse_key_properties_new (swidget->sctx, skey);
}

/* Loads delete dialog if a key is selected */
static void
delete_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, KEY_LIST)));
	if (skey)
		seahorse_delete_new (GTK_WINDOW (glade_xml_get_widget (swidget->xml, KEY_MANAGER)),
			swidget->sctx, skey);
}

/* Loads preferences dialog */
static void
preferences_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_preferences_show (swidget->sctx);
}

/* Shows about dialog */
static void
about_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	static GtkWidget *about = NULL;
	GdkPixbuf *pixbuf = NULL;

	gchar *authors[] = {
		"Jacob Perkins <jap1@users.sourceforge.net>",
		"Jose Carlos Garcia Sogo <jsogo@users.sourceforge.net>",
		"Jean Schurger <jk24@users.sourceforge.net>",
		NULL
	};

	gchar *documenters[] = {
		"Jacob Perkins <jap1@users.sourceforge.net>",
		NULL
	};

	gchar *translator_credits = _("translator_credits");

	if (about != NULL) {
		gtk_window_present (GTK_WINDOW (about));
		return;
	}

	pixbuf = gdk_pixbuf_new_from_file (SEAHORSE_GLADEDIR "seahorse.png", NULL);
	if (pixbuf != NULL) {
		GdkPixbuf *temp_pixbuf = NULL;
		
		temp_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
			gdk_pixbuf_get_width (pixbuf),
			gdk_pixbuf_get_height (pixbuf),
			GDK_INTERP_HYPER);
		g_object_unref (pixbuf);

		pixbuf = temp_pixbuf;
	}

	about = gnome_about_new (_("seahorse"), VERSION,
		"Copyright \xc2\xa9 2002, 2003 Seahorse Project",
		"http://seahorse.sourceforge.net",
		(const char **)authors, (const char **)documenters,
		strcmp (translator_credits, "translator_credits") != 0 ?
			translator_credits : NULL,
		pixbuf);
	gtk_window_set_transient_for (GTK_WINDOW (about), GTK_WINDOW (
		glade_xml_get_widget (swidget->xml, swidget->name)));
	gtk_window_set_destroy_with_parent (GTK_WINDOW (about), TRUE);

	if (pixbuf != NULL)
		g_object_unref (pixbuf);

	g_signal_connect (GTK_OBJECT (about), "destroy",
		G_CALLBACK (gtk_widget_destroyed), &about);
	gtk_widget_show (about);
}

/* Loads key properties of activated key */
static void
row_activated (GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *arg2, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_key_from_path (GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, KEY_LIST)), path);
	if (skey != NULL)
		seahorse_key_properties_new (swidget->sctx, skey);
}

void
seahorse_key_manager_show (SeahorseContext *sctx)
{
	SeahorseWidget *swidget;
	
	swidget = seahorse_widget_new_component (KEY_MANAGER, sctx);
	gtk_object_sink (GTK_OBJECT (sctx));
	
	glade_xml_signal_connect_data (swidget->xml, "quit", G_CALLBACK (quit), swidget);
	glade_xml_signal_connect_data (swidget->xml, "quit_event", G_CALLBACK (delete_event), swidget);
	glade_xml_signal_connect_data (swidget->xml, "generate_activate", G_CALLBACK (generate_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "export_activate", G_CALLBACK (export_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "import_activate", G_CALLBACK (import_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "text_editor_activate", G_CALLBACK (text_editor_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "file_manager_activate", G_CALLBACK (file_manager_activate), swidget);		
	glade_xml_signal_connect_data (swidget->xml, "properties_activate", G_CALLBACK (properties_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "delete_activate", G_CALLBACK (delete_activate), swidget);		
	glade_xml_signal_connect_data (swidget->xml, "preferences_activate", G_CALLBACK (preferences_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "row_activated", G_CALLBACK (row_activated), swidget);
	glade_xml_signal_connect_data (swidget->xml, "about_activate", G_CALLBACK (about_activate), swidget);
	
	seahorse_key_manager_store_new (sctx, GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, KEY_LIST)));
}
