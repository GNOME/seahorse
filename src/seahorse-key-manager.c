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
#include "seahorse-ops-key.h"
#include "seahorse-add-uid.h"
#include "seahorse-add-subkey.h"

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

/* Loads import dialog */
static void
import_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_import_show (swidget->sctx);
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

/* Loads export dialog if a key is selected */
static void
export_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, KEY_LIST)));
	if (skey)
		seahorse_export_new (swidget->sctx, skey);
}

/* Loads delete dialog if a key is selected */
static void
delete_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (
		glade_xml_get_widget (swidget->xml, KEY_LIST)));
	if (skey)
		seahorse_delete_key_new (GTK_WINDOW (glade_xml_get_widget (
			swidget->xml, KEY_MANAGER)), swidget->sctx, skey);
}

static void
change_passphrase_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (
		glade_xml_get_widget (swidget->xml, KEY_LIST)));
	if (skey)
		seahorse_ops_key_change_pass (swidget->sctx, skey);
}

static void
add_uid_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (
		glade_xml_get_widget (swidget->xml, KEY_LIST)));
	if (skey)
		seahorse_add_uid_new (swidget->sctx, skey);
}

static void
add_subkey_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (
		glade_xml_get_widget (swidget->xml, KEY_LIST)));
	if (skey)
		seahorse_add_subkey_new (swidget->sctx, skey);
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

	pixbuf = gdk_pixbuf_new_from_file (PIXMAPSDIR "seahorse.png", NULL);
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

static void
selection_changed (GtkTreeSelection *selection, SeahorseWidget *swidget)
{
	gboolean sensitive = FALSE,
		 secret = FALSE;
	SeahorseKey *skey = NULL;
	
	sensitive = gtk_tree_selection_get_selected (selection, NULL, NULL);
	if (sensitive) {
		skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (
			glade_xml_get_widget (swidget->xml, KEY_LIST)));
		secret = seahorse_context_key_has_secret (swidget->sctx, skey);
	}
	
	/* do all key operation items */
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "properties_button"), sensitive);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "export_button"), sensitive);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "delete_button"), sensitive);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "properties"), sensitive);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "export"), sensitive);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "delete"), sensitive);
	/* ops requiring a secret key */
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "change_passphrase"), secret);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "add_uid"), secret);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "add_subkey"), secret);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_change_passphrase"), secret);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_add_uid"), secret);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_add_subkey"), secret);
}

static gboolean
key_list_button_pressed (GtkWidget *widget, GdkEventButton *event, SeahorseWidget *swidget)
{
	GtkWidget *menu;
	
	if (event->button == 3) {
		menu = glade_xml_get_widget (swidget->xml, "context_menu");
		gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, event->button, event->time);
		gtk_widget_show (menu);
	}
	return FALSE;
}

void
seahorse_key_manager_show (SeahorseContext *sctx)
{
	SeahorseWidget *swidget;
	GtkTreeView *view;
	GtkTreeSelection *selection;
	
	swidget = seahorse_widget_new_component (KEY_MANAGER, sctx);
	gtk_object_sink (GTK_OBJECT (sctx));
	
	/* construct key context menu */
	glade_xml_construct (swidget->xml, SEAHORSE_GLADEDIR "seahorse-key-manager.glade2",
		"context_menu", NULL);
	/* quit signals */
	glade_xml_signal_connect_data (swidget->xml, "quit",
		G_CALLBACK (quit), swidget);
	glade_xml_signal_connect_data (swidget->xml, "quit_event",
		G_CALLBACK (delete_event), swidget);
	/* key menu signals */
	glade_xml_signal_connect_data (swidget->xml, "generate_activate",
		G_CALLBACK (generate_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "import_activate",
		G_CALLBACK (import_activate), swidget);	
	/* selected key signals */
	glade_xml_signal_connect_data (swidget->xml, "properties_activate",
		G_CALLBACK (properties_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "export_activate",
		G_CALLBACK (export_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "delete_activate",
		G_CALLBACK (delete_activate), swidget);
	/* selected key with secret signals */
	glade_xml_signal_connect_data (swidget->xml, "change_passphrase_activate",
		G_CALLBACK (change_passphrase_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "add_uid_activate",
		G_CALLBACK (add_uid_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "add_subkey_activate",
		G_CALLBACK (add_subkey_activate), swidget);
	/* tool menu signals */	
	glade_xml_signal_connect_data (swidget->xml, "text_editor_activate",
		G_CALLBACK (text_editor_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "file_manager_activate",
		G_CALLBACK (file_manager_activate), swidget);	
	/* other signals */	
	glade_xml_signal_connect_data (swidget->xml, "preferences_activate",
		G_CALLBACK (preferences_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "about_activate",
		G_CALLBACK (about_activate), swidget);
	/* tree view signals */	
	glade_xml_signal_connect_data (swidget->xml, "row_activated",
		G_CALLBACK (row_activated), swidget);
	glade_xml_signal_connect_data (swidget->xml, "key_list_button_pressed",
		G_CALLBACK (key_list_button_pressed), swidget);
	/* do initial selected key settings */
	view = GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, KEY_LIST));
	seahorse_key_manager_store_new (sctx, view);
	selection = gtk_tree_view_get_selection (view);
	g_signal_connect_after (selection, "changed",
		G_CALLBACK (selection_changed), swidget);
	selection_changed (selection, swidget);
	
	//features not available
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "sign_button"), FALSE);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "sign"), FALSE);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "add_revoker"), FALSE);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "add_photo"), FALSE);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_sign"), FALSE);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_add_revoker"), FALSE);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_add_photo"), FALSE);
}
