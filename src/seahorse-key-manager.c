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

#include <config.h>
#include <gnome.h>
#include <gconf/gconf-client.h>
#include <eel/eel-gconf-extensions.h>

#include "seahorse-key-manager.h"
#include "seahorse-widget.h"
#include "seahorse-generate.h"
#include "seahorse-preferences.h"
#include "seahorse-import.h"
#include "seahorse-text-editor.h"
#include "seahorse-util.h"
#include "seahorse-validity.h"
#include "seahorse-key-manager-store.h"
#include "seahorse-ops-key.h"
#include "seahorse-key-dialogs.h"
#include "seahorse-file-dialogs.h"
#include "seahorse-key-pair.h"

#define KEY_LIST "key_list"

#define STATUSBAR_VISIBLE KEY_UI "/statusbar_visible"
#define TOOLBAR_VISIBLE KEY_UI "/toolbar_visible"

#define GNOME_INTERFACE "/desktop/gnome/interface"
#define GNOME_TOOLBAR_STYLE GNOME_INTERFACE "/toolbar_style"

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
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (
		glade_xml_get_widget (swidget->xml, KEY_LIST)));
	if (skey)
		seahorse_export_new (swidget->sctx, skey);
}

static void
sign_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (
		glade_xml_get_widget (swidget->xml, KEY_LIST)));
	if (skey)
		seahorse_sign_new (swidget->sctx, skey, 0);
}

/* Loads delete dialog if a key is selected */
static void
delete_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (
		glade_xml_get_widget (swidget->xml, KEY_LIST)));
	if (skey)
		seahorse_delete_new (swidget->sctx, skey, 0);
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

static void
add_revoker_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (
		glade_xml_get_widget (swidget->xml, KEY_LIST)));
	if (skey)
		seahorse_add_revoker_new (swidget->sctx, skey);
}

static void
encrypt_file_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	seahorse_encrypt_file_new (swidget->sctx);
}

static void
sign_file_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	seahorse_sign_file_new (swidget->sctx);
}

static void
encrypt_sign_file_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	seahorse_encrypt_sign_file_new (swidget->sctx);
}

static void
decrypt_file_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	seahorse_decrypt_file_new (swidget->sctx);
}

static void
verify_file_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	seahorse_verify_file_new (swidget->sctx);
}

static void
decrypt_verify_file_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	seahorse_decrypt_verify_file_new (swidget->sctx);
}

/* Loads text editor */
static void
text_editor_activate (GtkMenuItem *widget, SeahorseWidget *swidget)
{
	seahorse_text_editor_show (swidget->sctx);
}

/* Loads preferences dialog */
static void
preferences_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_preferences_show (swidget->sctx);
}

static void
view_bar (GtkCheckMenuItem *item, const gchar *key)
{
	eel_gconf_set_boolean (key, gtk_check_menu_item_get_active (item));
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
		"Jean Schurger <yshark@schurger.org>",
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
		secret = SEAHORSE_IS_KEY_PAIR (skey);
	}
	
	/* do all key operation items */
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "properties_button"), sensitive);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "export_button"), sensitive);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "sign_button"), sensitive);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "delete_button"), sensitive);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "properties"), sensitive);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "export"), sensitive);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "sign"), sensitive);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "delete"), sensitive);
	/* ops requiring a secret key */
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "change_passphrase"), secret);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "add_uid"), secret);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "add_subkey"), secret);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "add_revoker"), secret);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_change_passphrase"), secret);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_add_uid"), secret);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_add_subkey"), secret);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_add_revoker"), secret);
}

static void
show_context_menu (SeahorseWidget *swidget, guint button, guint32 time)
{
	GtkWidget *menu;
	
	menu = glade_xml_get_widget (swidget->xml, "context_menu");
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, time);
	gtk_widget_show (menu);
}

static gboolean
key_list_button_pressed (GtkWidget *widget, GdkEventButton *event, SeahorseWidget *swidget)
{
	if (event->button == 3)
		show_context_menu (swidget, event->button, event->time);
	
	return FALSE;
}

static gboolean
key_list_popup_menu (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_selected_key (GTK_TREE_VIEW (widget));
	if (skey)
		show_context_menu (swidget, 0, gtk_get_current_event_time ());
}

static void
set_toolbar_style (GtkToolbar *toolbar, const gchar *style)
{
	if (g_str_equal (style, TOOLBAR_BOTH))
		gtk_toolbar_set_style (toolbar, GTK_TOOLBAR_BOTH);
	else if (g_str_equal (style, TOOLBAR_BOTH_HORIZ))
		gtk_toolbar_set_style (toolbar, GTK_TOOLBAR_BOTH_HORIZ);
	else if (g_str_equal (style, TOOLBAR_TEXT))
		gtk_toolbar_set_style (toolbar, GTK_TOOLBAR_TEXT);
	else if (g_str_equal (style, TOOLBAR_ICONS))
		gtk_toolbar_set_style (toolbar, GTK_TOOLBAR_ICONS);
}

static void
gconf_notification (GConfClient *gclient, guint id, GConfEntry *entry, SeahorseWidget *swidget)
{
	const gchar *key;
	GConfValue *value;
	GtkWidget *widget;
	
	key = gconf_entry_get_key (entry);
	value = gconf_entry_get_value (entry);
	
	if (g_str_equal (key, STATUSBAR_VISIBLE)) {
		widget = glade_xml_get_widget (swidget->xml, "status");
		
		if (gconf_value_get_bool (value))
			gtk_widget_show (widget);
		else
			gtk_widget_hide (widget);
	}
	else if (g_str_equal (key, TOOLBAR_VISIBLE)) {
		widget = glade_xml_get_widget (swidget->xml, "tool_dock");
		
		if (gconf_value_get_bool (value))
			gtk_widget_show (widget);
		else
			gtk_widget_hide (widget);
	}
	else if (g_str_equal (key, GNOME_TOOLBAR_STYLE) &&
	g_str_equal (TOOLBAR_DEFAULT, eel_gconf_get_string (KEY_TOOLBAR_STYLE))) {
		set_toolbar_style (GTK_TOOLBAR (glade_xml_get_widget (swidget->xml, "toolbar")),
			gconf_value_get_string (value));
	}
	else if (g_str_equal (key, KEY_TOOLBAR_STYLE)) {
		widget = glade_xml_get_widget (swidget->xml, "toolbar");
		
		/* if changed to default, use system settings */
		if (g_str_equal (gconf_value_get_string (value), TOOLBAR_DEFAULT)) {
			set_toolbar_style (GTK_TOOLBAR (widget),
				eel_gconf_get_string (GNOME_TOOLBAR_STYLE));
		}
		else
			set_toolbar_style (GTK_TOOLBAR (widget), gconf_value_get_string (value));
	}
}

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
	
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, KEY_LIST), sensitive);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key"), sensitive);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "edit"), sensitive);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "tools"), sensitive);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "tool_dock"), sensitive);
	
	while (g_main_context_pending (NULL))
		g_main_context_iteration (NULL, TRUE);
}

void
seahorse_key_manager_show (SeahorseContext *sctx)
{
	SeahorseWidget *swidget;
	GtkTreeView *view;
	GtkTreeSelection *selection;
	GtkWidget *widget;
	gboolean visible;
	
	swidget = seahorse_widget_new ("key-manager", sctx);
	gtk_object_sink (GTK_OBJECT (sctx));
	
	/* construct key context menu */
	glade_xml_construct (swidget->xml, SEAHORSE_GLADEDIR "seahorse-key-manager.glade2",
		"context_menu", NULL);
	
	/* quit signals */
	glade_xml_signal_connect_data (swidget->xml, "quit",
		G_CALLBACK (quit), swidget);
	glade_xml_signal_connect_data (swidget->xml, "quit_event",
		G_CALLBACK (delete_event), swidget);
	
	g_signal_connect_after (swidget->sctx, "progress", G_CALLBACK (show_progress), swidget);
	
	/* init gclient */
	eel_gconf_notification_add (KEY_UI, (GConfClientNotifyFunc) gconf_notification, swidget);
	eel_gconf_monitor_add (KEY_UI);
	eel_gconf_notification_add (GNOME_INTERFACE, (GConfClientNotifyFunc) gconf_notification, swidget);
	eel_gconf_monitor_add (GNOME_INTERFACE);
	
	/* init toolbar */
	glade_xml_signal_connect_data (swidget->xml, "toolbar_activate",
		G_CALLBACK (view_bar), TOOLBAR_VISIBLE);
	set_toolbar_style (GTK_TOOLBAR (glade_xml_get_widget (swidget->xml, "toolbar")),
		eel_gconf_get_string (KEY_TOOLBAR_STYLE));
	visible = eel_gconf_get_boolean (TOOLBAR_VISIBLE);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (
		glade_xml_get_widget (swidget->xml, "view_toolbar")), visible);
	if (!visible)
		gtk_widget_hide (glade_xml_get_widget (swidget->xml, "tool_dock"));
	
	/* init key list & selection settings */
	view = GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, KEY_LIST));
	selection = gtk_tree_view_get_selection (view);
	g_signal_connect (selection, "changed",
		G_CALLBACK (selection_changed), swidget);
	seahorse_key_manager_store_new (sctx, view);
	selection_changed (selection, swidget);
	
	/* init status bars after so that not hidden during startup */
	widget = glade_xml_get_widget (swidget->xml, "view_statusbar");
	visible = eel_gconf_get_boolean (STATUSBAR_VISIBLE);
	glade_xml_signal_connect_data (swidget->xml, "statusbar_activate",
		G_CALLBACK (view_bar), STATUSBAR_VISIBLE);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), visible);
	if (!visible)
		gtk_widget_hide (glade_xml_get_widget (swidget->xml, "status"));
	
	/* key menu signals */
	glade_xml_signal_connect_data (swidget->xml, "generate_activate",
		G_CALLBACK (generate_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "import_activate",
		G_CALLBACK (import_activate), swidget);	
	/* tool menu signals */
	glade_xml_signal_connect_data (swidget->xml, "encrypt_file_activate",
		G_CALLBACK (encrypt_file_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "sign_file_activate",
		G_CALLBACK (sign_file_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "encrypt_sign_file_activate",
		G_CALLBACK (encrypt_sign_file_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "decrypt_file_activate",
		G_CALLBACK (decrypt_file_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "verify_file_activate",
		G_CALLBACK (verify_file_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "decrypt_verify_file_activate",
		G_CALLBACK (decrypt_verify_file_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "text_editor_activate",
		G_CALLBACK (text_editor_activate), swidget);	
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
	glade_xml_signal_connect_data (swidget->xml, "key_list_popup_menu",
		G_CALLBACK (key_list_popup_menu), swidget);
	/* selected key signals */
	glade_xml_signal_connect_data (swidget->xml, "properties_activate",
		G_CALLBACK (properties_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "export_activate",
		G_CALLBACK (export_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "sign_activate",
		G_CALLBACK (sign_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "delete_activate",
		G_CALLBACK (delete_activate), swidget);
	/* selected key with secret signals */
	glade_xml_signal_connect_data (swidget->xml, "change_passphrase_activate",
		G_CALLBACK (change_passphrase_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "add_uid_activate",
		G_CALLBACK (add_uid_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "add_subkey_activate",
		G_CALLBACK (add_subkey_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "add_revoker_activate",
		G_CALLBACK (add_revoker_activate), swidget);
	
	//features not available
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "add_photo"), FALSE);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_add_photo"), FALSE);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "backup"), FALSE);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_backup"), FALSE);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "gen_revoke"), FALSE);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "key_gen_revoke"), FALSE);
}
