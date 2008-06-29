/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
 * 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *  
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include "seahorse-viewer.h"
#include <glib/gi18n-lib.h>
#include <config.h>
#include <seahorse-preferences.h>
#include <gio/gio.h>
#include <seahorse-commands.h>
#include <seahorse-util.h>
#include <seahorse-operation.h>
#include <gdk/gdk.h>
#include <seahorse-key-source.h>
#include <seahorse-progress.h>
#include <common/seahorse-registry.h>




struct _SeahorseViewerPrivate {
	GtkUIManager* _ui_manager;
	GtkActionGroup* _key_actions;
	GHashTable* _commands;
};

#define SEAHORSE_VIEWER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_TYPE_VIEWER, SeahorseViewerPrivate))
enum  {
	SEAHORSE_VIEWER_DUMMY_PROPERTY,
	SEAHORSE_VIEWER_SELECTED_KEY,
	SEAHORSE_VIEWER_CURRENT_KEYSET,
	SEAHORSE_VIEWER_WINDOW
};
static gboolean seahorse_viewer__about_initialized;
static void _seahorse_viewer_on_app_preferences_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_viewer_on_app_about_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_viewer_on_help_show_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_viewer_on_key_properties_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_viewer_on_key_export_file_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_viewer_on_key_export_clipboard_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_viewer_on_key_delete_gtk_action_activate (GtkAction* _sender, gpointer self);
static void seahorse_viewer_include_basic_actions (SeahorseViewer* self);
static GList* seahorse_viewer_real_get_selected_keys (SeahorseViewer* self);
static void seahorse_viewer_real_set_selected_keys (SeahorseViewer* self, GList* keys);
static SeahorseKey* seahorse_viewer_real_get_selected_key_and_uid (SeahorseViewer* self, guint* uid);
static void seahorse_viewer_on_ui_add_widget (SeahorseViewer* self, GtkUIManager* ui, GtkWidget* widget);
static void seahorse_viewer_on_app_preferences (SeahorseViewer* self, GtkAction* action);
static void _seahorse_viewer_on_about_link_clicked_gtk_about_dialog_activate_link_func (GtkAboutDialog* about, const char* link_, gpointer self);
static void seahorse_viewer_on_app_about (SeahorseViewer* self, GtkAction* action);
static void seahorse_viewer_on_about_link_clicked (GtkAboutDialog* about, const char* url);
static void seahorse_viewer_on_help_show (SeahorseViewer* self, GtkAction* action);
static void seahorse_viewer_on_key_properties (SeahorseViewer* self, GtkAction* action);
static gint seahorse_viewer_compare_by_ktype (SeahorseViewer* self, SeahorseKey* one, SeahorseKey* two);
static void seahorse_viewer_delete_key_batch (SeahorseViewer* self, GList* keys);
static void seahorse_viewer_on_key_delete (SeahorseViewer* self, GtkAction* action);
static void seahorse_viewer_on_copy_complete (SeahorseViewer* self, SeahorseOperation* op);
static void* _g_realloc_grealloc_func (void* data, gulong size);
static void _g_free_gdestroy_notify (void* data);
static void _seahorse_viewer_on_copy_complete_seahorse_done_func (SeahorseOperation* op, gpointer self);
static void seahorse_viewer_on_key_export_clipboard (SeahorseViewer* self, GtkAction* action);
static void seahorse_viewer_on_export_done (SeahorseViewer* self, SeahorseOperation* op);
static void _seahorse_viewer_on_export_done_seahorse_done_func (SeahorseOperation* op, gpointer self);
static void seahorse_viewer_on_key_export_file (SeahorseViewer* self, GtkAction* action);
static void seahorse_viewer_on_selection_changed (SeahorseViewer* self, SeahorseView* view);
static void _seahorse_viewer_on_ui_add_widget_gtk_ui_manager_add_widget (GtkUIManager* _sender, GtkWidget* widget, gpointer self);
static void _seahorse_viewer_on_selection_changed_seahorse_view_selection_changed (SeahorseViewer* _sender, gpointer self);
static GObject * seahorse_viewer_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties);
static gpointer seahorse_viewer_parent_class = NULL;
static SeahorseViewIface* seahorse_viewer_seahorse_view_parent_iface = NULL;
static void seahorse_viewer_dispose (GObject * obj);
static void _vala_array_free (gpointer array, gint array_length, GDestroyNotify destroy_func);

static const GtkActionEntry SEAHORSE_VIEWER_UI_ENTRIES[] = {{"key-menu", NULL, N_ ("_Key")}, {"edit-menu", NULL, N_ ("_Edit")}, {"view-menu", NULL, N_ ("_View")}, {"help-menu", NULL, N_ ("_Help")}, {"app-preferences", GTK_STOCK_PREFERENCES, N_ ("Prefere_nces"), NULL, N_ ("Change preferences for this program"), ((GCallback) (NULL))}, {"app-about", "gnome-stock-about", N_ ("_About"), NULL, N_ ("About this program"), ((GCallback) (NULL))}, {"help-show", GTK_STOCK_HELP, N_ ("_Contents"), "F1", N_ ("Show Seahorse help"), ((GCallback) (NULL))}};
static const GtkActionEntry SEAHORSE_VIEWER_KEY_ENTRIES[] = {{"key-properties", GTK_STOCK_PROPERTIES, N_ ("P_roperties"), NULL, N_ ("Show key properties"), ((GCallback) (NULL))}, {"key-export-file", GTK_STOCK_SAVE_AS, N_ ("E_xport Public Key..."), NULL, N_ ("Export public part of key to a file"), ((GCallback) (NULL))}, {"key-export-clipboard", GTK_STOCK_COPY, N_ ("_Copy Public Key"), "<control>C", N_ ("Copy public part of selected keys to the clipboard"), ((GCallback) (NULL))}, {"key-delete", GTK_STOCK_DELETE, N_ ("_Delete Key"), NULL, N_ ("Delete selected keys"), ((GCallback) (NULL))}};


static void _seahorse_viewer_on_app_preferences_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_viewer_on_app_preferences (self, _sender);
}


static void _seahorse_viewer_on_app_about_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_viewer_on_app_about (self, _sender);
}


static void _seahorse_viewer_on_help_show_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_viewer_on_help_show (self, _sender);
}


static void _seahorse_viewer_on_key_properties_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_viewer_on_key_properties (self, _sender);
}


static void _seahorse_viewer_on_key_export_file_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_viewer_on_key_export_file (self, _sender);
}


static void _seahorse_viewer_on_key_export_clipboard_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_viewer_on_key_export_clipboard (self, _sender);
}


static void _seahorse_viewer_on_key_delete_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_viewer_on_key_delete (self, _sender);
}


static void seahorse_viewer_include_basic_actions (SeahorseViewer* self) {
	GtkActionGroup* actions;
	GtkActionGroup* _tmp0;
	gboolean _tmp1;
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	/*
	 * We hook callbacks up here for now because of a compiler warning. See:
	 * http://bugzilla.gnome.org/show_bug.cgi?id=539483
	 */
	actions = gtk_action_group_new ("main");
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, SEAHORSE_VIEWER_UI_ENTRIES, G_N_ELEMENTS (SEAHORSE_VIEWER_UI_ENTRIES), self);
	g_signal_connect_object (gtk_action_group_get_action (actions, "app-preferences"), "activate", ((GCallback) (_seahorse_viewer_on_app_preferences_gtk_action_activate)), self, 0);
	g_signal_connect_object (gtk_action_group_get_action (actions, "app-about"), "activate", ((GCallback) (_seahorse_viewer_on_app_about_gtk_action_activate)), self, 0);
	g_signal_connect_object (gtk_action_group_get_action (actions, "help-show"), "activate", ((GCallback) (_seahorse_viewer_on_help_show_gtk_action_activate)), self, 0);
	seahorse_viewer_include_actions (self, actions);
	_tmp0 = NULL;
	self->priv->_key_actions = (_tmp0 = gtk_action_group_new ("key"), (self->priv->_key_actions == NULL ? NULL : (self->priv->_key_actions = (g_object_unref (self->priv->_key_actions), NULL))), _tmp0);
	gtk_action_group_set_translation_domain (self->priv->_key_actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (self->priv->_key_actions, SEAHORSE_VIEWER_KEY_ENTRIES, G_N_ELEMENTS (SEAHORSE_VIEWER_KEY_ENTRIES), self);
	g_signal_connect_object (gtk_action_group_get_action (self->priv->_key_actions, "key-properties"), "activate", ((GCallback) (_seahorse_viewer_on_key_properties_gtk_action_activate)), self, 0);
	g_signal_connect_object (gtk_action_group_get_action (self->priv->_key_actions, "key-export-file"), "activate", ((GCallback) (_seahorse_viewer_on_key_export_file_gtk_action_activate)), self, 0);
	g_signal_connect_object (gtk_action_group_get_action (self->priv->_key_actions, "key-export-clipboard"), "activate", ((GCallback) (_seahorse_viewer_on_key_export_clipboard_gtk_action_activate)), self, 0);
	g_signal_connect_object (gtk_action_group_get_action (self->priv->_key_actions, "key-delete"), "activate", ((GCallback) (_seahorse_viewer_on_key_delete_gtk_action_activate)), self, 0);
	/* Mark the properties toolbar button as important */
	g_object_set (gtk_action_group_get_action (self->priv->_key_actions, "key-properties"), "is-important", TRUE, NULL);
	seahorse_viewer_include_actions (self, self->priv->_key_actions);
	(actions == NULL ? NULL : (actions = (g_object_unref (actions), NULL)));
}


void seahorse_viewer_ensure_updated (SeahorseViewer* self) {
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	gtk_ui_manager_ensure_update (self->priv->_ui_manager);
}


void seahorse_viewer_include_actions (SeahorseViewer* self, GtkActionGroup* actions) {
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_ACTION_GROUP (actions));
	gtk_ui_manager_insert_action_group (self->priv->_ui_manager, actions, -1);
}


static GList* seahorse_viewer_real_get_selected_keys (SeahorseViewer* self) {
	g_return_val_if_fail (SEAHORSE_IS_VIEWER (self), NULL);
	return NULL;
}


GList* seahorse_viewer_get_selected_keys (SeahorseViewer* self) {
	return SEAHORSE_VIEWER_GET_CLASS (self)->get_selected_keys (self);
}


static void seahorse_viewer_real_set_selected_keys (SeahorseViewer* self, GList* keys) {
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (keys != NULL);
}


void seahorse_viewer_set_selected_keys (SeahorseViewer* self, GList* keys) {
	SEAHORSE_VIEWER_GET_CLASS (self)->set_selected_keys (self, keys);
}


static SeahorseKey* seahorse_viewer_real_get_selected_key_and_uid (SeahorseViewer* self, guint* uid) {
	g_return_val_if_fail (SEAHORSE_IS_VIEWER (self), NULL);
	/* Must be overridden */
	return NULL;
}


SeahorseKey* seahorse_viewer_get_selected_key_and_uid (SeahorseViewer* self, guint* uid) {
	return SEAHORSE_VIEWER_GET_CLASS (self)->get_selected_key_and_uid (self, uid);
}


static void seahorse_viewer_on_ui_add_widget (SeahorseViewer* self, GtkUIManager* ui, GtkWidget* widget) {
	char* name;
	GtkWidget* _tmp2;
	GtkWidget* holder;
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_UI_MANAGER (ui));
	g_return_if_fail (GTK_IS_WIDGET (widget));
	name = NULL;
	if (G_TYPE_FROM_INSTANCE (G_OBJECT (widget)) == GTK_TYPE_MENU_BAR) {
		char* _tmp0;
		_tmp0 = NULL;
		name = (_tmp0 = g_strdup ("menu-placeholder"), (name = (g_free (name), NULL)), _tmp0);
	} else {
		if (G_TYPE_FROM_INSTANCE (G_OBJECT (widget)) == GTK_TYPE_TOOLBAR) {
			char* _tmp1;
			_tmp1 = NULL;
			name = (_tmp1 = g_strdup ("toolbar-placeholder"), (name = (g_free (name), NULL)), _tmp1);
		} else {
			name = (g_free (name), NULL);
			return;
		}
	}
	_tmp2 = NULL;
	holder = (_tmp2 = seahorse_widget_get_widget (SEAHORSE_WIDGET (self), name), (_tmp2 == NULL ? NULL : g_object_ref (_tmp2)));
	if (holder != NULL) {
		gtk_container_add ((GTK_CONTAINER (holder)), widget);
	} else {
		g_warning ("seahorse-viewer.vala:187: no place holder found for: %s", name);
	}
	name = (g_free (name), NULL);
	(holder == NULL ? NULL : (holder = (g_object_unref (holder), NULL)));
}


static void seahorse_viewer_on_app_preferences (SeahorseViewer* self, GtkAction* action) {
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	seahorse_preferences_show (seahorse_view_get_window (SEAHORSE_VIEW (self)), NULL);
}


static void _seahorse_viewer_on_about_link_clicked_gtk_about_dialog_activate_link_func (GtkAboutDialog* about, const char* link_, gpointer self) {
	seahorse_viewer_on_about_link_clicked (about, link_);
}


static void seahorse_viewer_on_app_about (SeahorseViewer* self, GtkAction* action) {
	char** _tmp2;
	gint authors_length1;
	char** _tmp1;
	const char* _tmp0;
	char** authors;
	char** _tmp4;
	gint documenters_length1;
	char** _tmp3;
	char** documenters;
	char** _tmp6;
	gint artists_length1;
	char** _tmp5;
	char** artists;
	GtkAboutDialog* about;
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	_tmp2 = NULL;
	_tmp1 = NULL;
	_tmp0 = NULL;
	authors = (_tmp2 = (_tmp1 = g_new0 (char*, 9 + 1), _tmp1[0] = g_strdup ("Jacob Perkins <jap1@users.sourceforge.net>"), _tmp1[1] = g_strdup ("Jose Carlos Garcia Sogo <jsogo@users.sourceforge.net>"), _tmp1[2] = g_strdup ("Jean Schurger <yshark@schurger.org>"), _tmp1[3] = g_strdup ("Stef Walter <stef@memberwebs.com>"), _tmp1[4] = g_strdup ("Adam Schreiber <sadam@clemson.edu>"), _tmp1[5] = g_strdup (""), _tmp1[6] = (_tmp0 = _ ("Contributions:"), (_tmp0 == NULL ? NULL : g_strdup (_tmp0))), _tmp1[7] = g_strdup ("Albrecht Dreß <albrecht.dress@arcor.de>"), _tmp1[8] = g_strdup ("Jim Pharis <binbrain@gmail.com>"), _tmp1), authors_length1 = 9, _tmp2);
	_tmp4 = NULL;
	_tmp3 = NULL;
	documenters = (_tmp4 = (_tmp3 = g_new0 (char*, 3 + 1), _tmp3[0] = g_strdup ("Jacob Perkins <jap1@users.sourceforge.net>"), _tmp3[1] = g_strdup ("Adam Schreiber <sadam@clemson.edu>"), _tmp3[2] = g_strdup ("Milo Casagrande <milo_casagrande@yahoo.it>"), _tmp3), documenters_length1 = 3, _tmp4);
	_tmp6 = NULL;
	_tmp5 = NULL;
	artists = (_tmp6 = (_tmp5 = g_new0 (char*, 2 + 1), _tmp5[0] = g_strdup ("Jacob Perkins <jap1@users.sourceforge.net>"), _tmp5[1] = g_strdup ("Stef Walter <stef@memberwebs.com>"), _tmp5), artists_length1 = 2, _tmp6);
	if (!seahorse_viewer__about_initialized) {
		seahorse_viewer__about_initialized = TRUE;
		gtk_about_dialog_set_url_hook (_seahorse_viewer_on_about_link_clicked_gtk_about_dialog_activate_link_func, NULL, NULL);
	}
	about = g_object_ref_sink (gtk_about_dialog_new ());
	gtk_about_dialog_set_artists (about, artists);
	gtk_about_dialog_set_authors (about, authors);
	gtk_about_dialog_set_documenters (about, documenters);
	gtk_about_dialog_set_version (about, VERSION);
	gtk_about_dialog_set_comments (about, _ ("Encryption Key Manager"));
	gtk_about_dialog_set_copyright (about, "Copyright \xc2\xa9 2002 - 2008 Seahorse Project");
	gtk_about_dialog_set_translator_credits (about, _ ("translator-credits"));
	gtk_about_dialog_set_logo_icon_name (about, "seahorse");
	gtk_about_dialog_set_website (about, "http://www.gnome.org/projects/seahorse");
	gtk_about_dialog_set_website_label (about, _ ("Seahorse Project Homepage"));
	gtk_dialog_run (GTK_DIALOG (about));
	authors = (_vala_array_free (authors, authors_length1, ((GDestroyNotify) (g_free))), NULL);
	documenters = (_vala_array_free (documenters, documenters_length1, ((GDestroyNotify) (g_free))), NULL);
	artists = (_vala_array_free (artists, artists_length1, ((GDestroyNotify) (g_free))), NULL);
	(about == NULL ? NULL : (about = (g_object_unref (about), NULL)));
}


static void seahorse_viewer_on_about_link_clicked (GtkAboutDialog* about, const char* url) {
	GError * inner_error;
	g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));
	g_return_if_fail (url != NULL);
	inner_error = NULL;
	{
		GAppLaunchContext* _tmp0;
		_tmp0 = NULL;
		g_app_info_launch_default_for_uri (url, (_tmp0 = g_app_launch_context_new ()), &inner_error);
		if (inner_error != NULL) {
			goto __catch0_g_error;
		}
		(_tmp0 == NULL ? NULL : (_tmp0 = (g_object_unref (_tmp0), NULL)));
	}
	goto __finally0;
	__catch0_g_error:
	{
		GError * ex;
		ex = inner_error;
		inner_error = NULL;
		{
			g_warning ("seahorse-viewer.vala:243: couldn't launch url: %s: %s", url, ex->message);
			(ex == NULL ? NULL : (ex = (g_error_free (ex), NULL)));
		}
	}
	__finally0:
	;
}


static void seahorse_viewer_on_help_show (SeahorseViewer* self, GtkAction* action) {
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	seahorse_widget_show_help (SEAHORSE_WIDGET (self));
}


void seahorse_viewer_show_context_menu (SeahorseViewer* self, guint button, guint time) {
	GtkMenu* _tmp0;
	GtkMenu* menu;
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	_tmp0 = NULL;
	menu = (_tmp0 = GTK_MENU (gtk_ui_manager_get_widget (self->priv->_ui_manager, "/KeyPopup")), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	g_return_if_fail (menu != NULL && G_TYPE_FROM_INSTANCE (G_OBJECT (menu)) == GTK_TYPE_MENU);
	gtk_menu_popup (menu, NULL, NULL, NULL, NULL, button, time);
	gtk_widget_show (GTK_WIDGET (menu));
	(menu == NULL ? NULL : (menu = (g_object_unref (menu), NULL)));
}


void seahorse_viewer_show_properties (SeahorseViewer* self, SeahorseKey* key) {
	SeahorseCommands* _tmp0;
	SeahorseCommands* commands;
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (SEAHORSE_IS_KEY (key));
	_tmp0 = NULL;
	commands = (_tmp0 = ((SeahorseCommands*) (g_hash_table_lookup (self->priv->_commands, GINT_TO_POINTER (seahorse_key_get_ktype (key))))), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	if (commands != NULL) {
		seahorse_commands_show_properties (commands, key);
	}
	(commands == NULL ? NULL : (commands = (g_object_unref (commands), NULL)));
}


static void seahorse_viewer_on_key_properties (SeahorseViewer* self, GtkAction* action) {
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	if (seahorse_viewer_get_selected_key (self) != NULL) {
		seahorse_viewer_show_properties (self, seahorse_viewer_get_selected_key (self));
	}
}


static gint seahorse_viewer_compare_by_ktype (SeahorseViewer* self, SeahorseKey* one, SeahorseKey* two) {
	GQuark kone;
	GQuark ktwo;
	g_return_val_if_fail (SEAHORSE_IS_VIEWER (self), 0);
	g_return_val_if_fail (SEAHORSE_IS_KEY (one), 0);
	g_return_val_if_fail (SEAHORSE_IS_KEY (two), 0);
	kone = seahorse_key_get_ktype (one);
	ktwo = seahorse_key_get_ktype (two);
	if (kone < ktwo) {
		return -1;
	}
	if (kone > ktwo) {
		return 1;
	}
	return 0;
}


static void seahorse_viewer_delete_key_batch (SeahorseViewer* self, GList* keys) {
	GError * inner_error;
	SeahorseCommands* _tmp0;
	SeahorseCommands* commands;
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (keys != NULL);
	inner_error = NULL;
	g_assert (keys != NULL);
	_tmp0 = NULL;
	commands = (_tmp0 = ((SeahorseCommands*) (g_hash_table_lookup (self->priv->_commands, GINT_TO_POINTER (seahorse_key_get_ktype (((SeahorseKey*) (((SeahorseKey*) (keys->data))))))))), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	{
		if (commands != NULL) {
			seahorse_commands_delete_keys (commands, keys, &inner_error);
			if (inner_error != NULL) {
				goto __catch1_g_error;
			}
		}
	}
	goto __finally1;
	__catch1_g_error:
	{
		GError * ex;
		ex = inner_error;
		inner_error = NULL;
		{
			seahorse_util_handle_error (ex, _ ("Couldn't delete keys."), seahorse_view_get_window (SEAHORSE_VIEW (self)), NULL);
			(ex == NULL ? NULL : (ex = (g_error_free (ex), NULL)));
		}
	}
	__finally1:
	;
	(commands == NULL ? NULL : (commands = (g_object_unref (commands), NULL)));
}


static void seahorse_viewer_on_key_delete (SeahorseViewer* self, GtkAction* action) {
	GList* keys;
	GList* batch;
	GList* _tmp0;
	guint num;
	GQuark ktype;
	GList* _tmp4;
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	keys = NULL;
	batch = NULL;
	/* Get the selected keys and sort them by ktype */
	_tmp0 = NULL;
	keys = (_tmp0 = seahorse_viewer_get_selected_keys (self), (keys == NULL ? NULL : (keys = (g_list_free (keys), NULL))), _tmp0);
	keys = g_list_sort (keys, ((GCompareFunc) (seahorse_viewer_compare_by_ktype)));
	num = g_list_length (keys);
	if (num == 0) {
		(keys == NULL ? NULL : (keys = (g_list_free (keys), NULL)));
		(batch == NULL ? NULL : (batch = (g_list_free (batch), NULL)));
		return;
	}
	/* Check for private keys */
	{
		GList* key_collection;
		GList* key_it;
		key_collection = keys;
		for (key_it = key_collection; key_it != NULL; key_it = key_it->next) {
			SeahorseKey* key;
			key = ((SeahorseKey*) (key_it->data));
			{
				if (seahorse_key_get_etype (key) == SKEY_PRIVATE) {
					char* prompt;
					prompt = NULL;
					if (num == 1) {
						char* _tmp1;
						_tmp1 = NULL;
						prompt = (_tmp1 = g_strdup_printf (_ ("%s is a private key. Are you sure you want to proceed?"), seahorse_key_get_display_name (((SeahorseKey*) (((SeahorseKey*) (keys->data)))))), (prompt = (g_free (prompt), NULL)), _tmp1);
					} else {
						char* _tmp3;
						const char* _tmp2;
						_tmp3 = NULL;
						_tmp2 = NULL;
						prompt = (_tmp3 = (_tmp2 = _ ("One or more of the deleted keys are private keys. Are you sure you want to proceed?"), (_tmp2 == NULL ? NULL : g_strdup (_tmp2))), (prompt = (g_free (prompt), NULL)), _tmp3);
					}
					if (!seahorse_util_prompt_delete (prompt)) {
						prompt = (g_free (prompt), NULL);
						(keys == NULL ? NULL : (keys = (g_list_free (keys), NULL)));
						(batch == NULL ? NULL : (batch = (g_list_free (batch), NULL)));
						return;
					}
					prompt = (g_free (prompt), NULL);
				}
			}
		}
	}
	ktype = ((GQuark) (0));
	_tmp4 = NULL;
	batch = (_tmp4 = NULL, (batch == NULL ? NULL : (batch = (g_list_free (batch), NULL))), _tmp4);
	{
		GList* key_collection;
		GList* key_it;
		key_collection = keys;
		for (key_it = key_collection; key_it != NULL; key_it = key_it->next) {
			SeahorseKey* _tmp6;
			SeahorseKey* key;
			_tmp6 = NULL;
			key = (_tmp6 = ((SeahorseKey*) (key_it->data)), (_tmp6 == NULL ? NULL : g_object_ref (_tmp6)));
			{
				/* Process that batch */
				if (ktype != seahorse_key_get_ktype (key) && batch != NULL) {
					GList* _tmp5;
					seahorse_viewer_delete_key_batch (self, batch);
					_tmp5 = NULL;
					batch = (_tmp5 = NULL, (batch == NULL ? NULL : (batch = (g_list_free (batch), NULL))), _tmp5);
				}
				/* Add to the batch */
				batch = g_list_prepend (batch, key);
				(key == NULL ? NULL : (key = (g_object_unref (key), NULL)));
			}
		}
	}
	/* Process last batch */
	if (batch != NULL) {
		seahorse_viewer_delete_key_batch (self, batch);
	}
	(keys == NULL ? NULL : (keys = (g_list_free (keys), NULL)));
	(batch == NULL ? NULL : (batch = (g_list_free (batch), NULL)));
}


static void seahorse_viewer_on_copy_complete (SeahorseViewer* self, SeahorseOperation* op) {
	GObject* _tmp0;
	GObject* result;
	GMemoryOutputStream* _tmp1;
	GMemoryOutputStream* output;
	const char* text;
	guint size;
	GdkAtom atom;
	GtkClipboard* _tmp2;
	GtkClipboard* board;
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (SEAHORSE_IS_OPERATION (op));
	if (!seahorse_operation_is_successful (op)) {
		seahorse_operation_display_error (op, _ ("Couldn't retrieve data from key server"), GTK_WIDGET (seahorse_view_get_window (SEAHORSE_VIEW (self))));
		return;
	}
	_tmp0 = NULL;
	result = (_tmp0 = G_OBJECT (seahorse_operation_get_result (op)), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	g_return_if_fail (result != NULL && G_TYPE_FROM_INSTANCE (result) != G_TYPE_MEMORY_OUTPUT_STREAM);
	_tmp1 = NULL;
	output = (_tmp1 = G_MEMORY_OUTPUT_STREAM (result), (_tmp1 == NULL ? NULL : g_object_ref (_tmp1)));
	text = ((const char*) (g_memory_output_stream_get_data (output)));
	g_return_if_fail (text != NULL);
	size = seahorse_util_memory_output_length (output);
	g_return_if_fail (size >= 0);
	atom = gdk_atom_intern ("CLIPBOARD", FALSE);
	_tmp2 = NULL;
	board = (_tmp2 = gtk_clipboard_get (atom), (_tmp2 == NULL ? NULL : g_object_ref (_tmp2)));
	gtk_clipboard_set_text (board, text, ((gint) (size)));
	seahorse_viewer_set_status (self, _ ("Copied keys"));
	(result == NULL ? NULL : (result = (g_object_unref (result), NULL)));
	(output == NULL ? NULL : (output = (g_object_unref (output), NULL)));
	(board == NULL ? NULL : (board = (g_object_unref (board), NULL)));
}


static void* _g_realloc_grealloc_func (void* data, gulong size) {
	return g_realloc (data, size);
}


static void _g_free_gdestroy_notify (void* data) {
	g_free (data);
}


static void _seahorse_viewer_on_copy_complete_seahorse_done_func (SeahorseOperation* op, gpointer self) {
	seahorse_viewer_on_copy_complete (self, op);
}


static void seahorse_viewer_on_key_export_clipboard (SeahorseViewer* self, GtkAction* action) {
	GList* keys;
	GOutputStream* output;
	SeahorseOperation* op;
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	keys = seahorse_viewer_get_selected_keys (self);
	if (keys == NULL) {
		(keys == NULL ? NULL : (keys = (g_list_free (keys), NULL)));
		return;
	}
	output = G_OUTPUT_STREAM (g_memory_output_stream_new (NULL, ((gulong) (0)), _g_realloc_grealloc_func, _g_free_gdestroy_notify));
	op = seahorse_key_source_export_keys (keys, output);
	seahorse_progress_show (op, _ ("Retrieving keys"), TRUE);
	seahorse_operation_watch (op, _seahorse_viewer_on_copy_complete_seahorse_done_func, self, NULL, NULL);
	(keys == NULL ? NULL : (keys = (g_list_free (keys), NULL)));
	(output == NULL ? NULL : (output = (g_object_unref (output), NULL)));
	(op == NULL ? NULL : (op = (g_object_unref (op), NULL)));
}


static void seahorse_viewer_on_export_done (SeahorseViewer* self, SeahorseOperation* op) {
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (SEAHORSE_IS_OPERATION (op));
	if (!seahorse_operation_is_successful (op)) {
		seahorse_operation_display_error (op, _ ("Couldn't export keys"), GTK_WIDGET (seahorse_view_get_window (SEAHORSE_VIEW (self))));
	}
}


static void _seahorse_viewer_on_export_done_seahorse_done_func (SeahorseOperation* op, gpointer self) {
	seahorse_viewer_on_export_done (self, op);
}


static void seahorse_viewer_on_key_export_file (SeahorseViewer* self, GtkAction* action) {
	GError * inner_error;
	GList* keys;
	GtkDialog* _tmp0;
	GtkDialog* dialog;
	char* uri;
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	inner_error = NULL;
	keys = seahorse_viewer_get_selected_keys (self);
	if (keys == NULL) {
		(keys == NULL ? NULL : (keys = (g_list_free (keys), NULL)));
		return;
	}
	_tmp0 = NULL;
	dialog = (_tmp0 = seahorse_util_chooser_save_new (_ ("Export public key"), seahorse_view_get_window (SEAHORSE_VIEW (self))), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	seahorse_util_chooser_show_key_files (dialog);
	seahorse_util_chooser_set_filename (dialog, keys);
	uri = seahorse_util_chooser_save_prompt (dialog);
	if (uri != NULL) {
		{
			GFile* file;
			GOutputStream* output;
			SeahorseOperation* op;
			file = g_file_new_for_uri (uri);
			output = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE, 0, NULL, &inner_error));
			if (inner_error != NULL) {
				goto __catch2_g_error;
			}
			op = seahorse_key_source_export_keys (keys, output);
			seahorse_progress_show (op, _ ("Exporting keys"), TRUE);
			seahorse_operation_watch (op, _seahorse_viewer_on_export_done_seahorse_done_func, self, NULL, NULL);
			(file == NULL ? NULL : (file = (g_object_unref (file), NULL)));
			(output == NULL ? NULL : (output = (g_object_unref (output), NULL)));
			(op == NULL ? NULL : (op = (g_object_unref (op), NULL)));
		}
		goto __finally2;
		__catch2_g_error:
		{
			GError * ex;
			ex = inner_error;
			inner_error = NULL;
			{
				char* _tmp1;
				_tmp1 = NULL;
				seahorse_util_handle_error (ex, _ ("Couldn't export key to \"%s\""), (_tmp1 = seahorse_util_uri_get_last (uri)), NULL);
				_tmp1 = (g_free (_tmp1), NULL);
				(ex == NULL ? NULL : (ex = (g_error_free (ex), NULL)));
				(keys == NULL ? NULL : (keys = (g_list_free (keys), NULL)));
				(dialog == NULL ? NULL : (dialog = (g_object_unref (dialog), NULL)));
				uri = (g_free (uri), NULL);
				return;
			}
		}
		__finally2:
		;
	}
	(keys == NULL ? NULL : (keys = (g_list_free (keys), NULL)));
	(dialog == NULL ? NULL : (dialog = (g_object_unref (dialog), NULL)));
	uri = (g_free (uri), NULL);
}


static void seahorse_viewer_on_selection_changed (SeahorseViewer* self, SeahorseView* view) {
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (SEAHORSE_IS_VIEW (view));
	gtk_action_group_set_sensitive (self->priv->_key_actions, seahorse_view_get_selected_key (view) != NULL);
}


void seahorse_viewer_set_status (SeahorseViewer* self, const char* text) {
	GtkWidget* _tmp0;
	GtkWidget* widget;
	GtkStatusbar* _tmp1;
	GtkStatusbar* status;
	guint id;
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (text != NULL);
	_tmp0 = NULL;
	widget = (_tmp0 = seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "status"), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	g_return_if_fail (widget != NULL || G_TYPE_FROM_INSTANCE (G_OBJECT (widget)) != GTK_TYPE_STATUSBAR);
	_tmp1 = NULL;
	status = (_tmp1 = GTK_STATUSBAR (widget), (_tmp1 == NULL ? NULL : g_object_ref (_tmp1)));
	id = gtk_statusbar_get_context_id (status, "key-manager");
	gtk_statusbar_pop (status, id);
	gtk_statusbar_push (status, id, text);
	(widget == NULL ? NULL : (widget = (g_object_unref (widget), NULL)));
	(status == NULL ? NULL : (status = (g_object_unref (status), NULL)));
}


void seahorse_viewer_set_numbered_status (SeahorseViewer* self, const char* text, gint num) {
	char* message;
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (text != NULL);
	message = g_strdup_printf (text, num);
	seahorse_viewer_set_status (self, message);
	message = (g_free (message), NULL);
}


SeahorseKey* seahorse_viewer_get_selected_key (SeahorseViewer* self) {
	SeahorseKey* value;
	g_object_get (G_OBJECT (self), "selected-key", &value, NULL);
	if (value != NULL) {
		g_object_unref (value);
	}
	return value;
}


static SeahorseKey* seahorse_viewer_real_get_selected_key (SeahorseViewer* self) {
	g_return_val_if_fail (SEAHORSE_IS_VIEWER (self), NULL);
	/* Must be overridden */
	return NULL;
}


void seahorse_viewer_set_selected_key (SeahorseViewer* self, SeahorseKey* value) {
	g_object_set (G_OBJECT (self), "selected-key", value, NULL);
}


static void seahorse_viewer_real_set_selected_key (SeahorseViewer* self, SeahorseKey* value) {
	GList* keys;
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	keys = NULL;
	keys = g_list_prepend (keys, value);
	seahorse_viewer_set_selected_keys (self, keys);
	(keys == NULL ? NULL : (keys = (g_list_free (keys), NULL)));
}


SeahorseKeyset* seahorse_viewer_get_current_keyset (SeahorseViewer* self) {
	SeahorseKeyset* value;
	g_object_get (G_OBJECT (self), "current-keyset", &value, NULL);
	if (value != NULL) {
		g_object_unref (value);
	}
	return value;
}


static SeahorseKeyset* seahorse_viewer_real_get_current_keyset (SeahorseViewer* self) {
	g_return_val_if_fail (SEAHORSE_IS_VIEWER (self), NULL);
	/* Must be overridden */
	return NULL;
}


static GtkWindow* seahorse_viewer_real_get_window (SeahorseViewer* self) {
	g_return_val_if_fail (SEAHORSE_IS_VIEWER (self), NULL);
	return GTK_WINDOW (seahorse_widget_get_toplevel (SEAHORSE_WIDGET (self)));
}


static void _seahorse_viewer_on_ui_add_widget_gtk_ui_manager_add_widget (GtkUIManager* _sender, GtkWidget* widget, gpointer self) {
	seahorse_viewer_on_ui_add_widget (self, _sender, widget);
}


static void _seahorse_viewer_on_selection_changed_seahorse_view_selection_changed (SeahorseViewer* _sender, gpointer self) {
	seahorse_viewer_on_selection_changed (self, _sender);
}


static GObject * seahorse_viewer_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties) {
	GObject * obj;
	SeahorseViewerClass * klass;
	GObjectClass * parent_class;
	SeahorseViewer * self;
	GError * inner_error;
	klass = SEAHORSE_VIEWER_CLASS (g_type_class_peek (SEAHORSE_TYPE_VIEWER));
	parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
	obj = parent_class->constructor (type, n_construct_properties, construct_properties);
	self = SEAHORSE_VIEWER (obj);
	inner_error = NULL;
	{
		GtkUIManager* _tmp0;
		GtkWidget* _tmp1;
		GtkWidget* win;
		GHashTable* _tmp2;
		GList* types;
		_tmp0 = NULL;
		self->priv->_ui_manager = (_tmp0 = gtk_ui_manager_new (), (self->priv->_ui_manager == NULL ? NULL : (self->priv->_ui_manager = (g_object_unref (self->priv->_ui_manager), NULL))), _tmp0);
		/* The widgts get added in an idle loop later */
		g_signal_connect_object (self->priv->_ui_manager, "add-widget", ((GCallback) (_seahorse_viewer_on_ui_add_widget_gtk_ui_manager_add_widget)), self, 0);
		{
			char* path;
			path = g_strdup_printf ("%sseahorse-%s.ui", SEAHORSE_GLADEDIR, seahorse_widget_get_name (SEAHORSE_WIDGET (self)));
			gtk_ui_manager_add_ui_from_file (self->priv->_ui_manager, path, &inner_error);
			if (inner_error != NULL) {
				goto __catch3_g_error;
			}
			path = (g_free (path), NULL);
		}
		goto __finally3;
		__catch3_g_error:
		{
			GError * ex;
			ex = inner_error;
			inner_error = NULL;
			{
				g_warning ("seahorse-viewer.vala:70: couldn't load ui description for '%s': %s", seahorse_widget_get_name (SEAHORSE_WIDGET (self)), ex->message);
				(ex == NULL ? NULL : (ex = (g_error_free (ex), NULL)));
			}
		}
		__finally3:
		;
		_tmp1 = NULL;
		win = (_tmp1 = seahorse_widget_get_toplevel (SEAHORSE_WIDGET (self)), (_tmp1 == NULL ? NULL : g_object_ref (_tmp1)));
		if (G_TYPE_FROM_INSTANCE (G_OBJECT (win)) == GTK_TYPE_WINDOW) {
			gtk_window_add_accel_group ((GTK_WINDOW (win)), gtk_ui_manager_get_accel_group (self->priv->_ui_manager));
		}
		seahorse_viewer_include_basic_actions (self);
		g_signal_connect_object (SEAHORSE_VIEW (self), "selection-changed", ((GCallback) (_seahorse_viewer_on_selection_changed_seahorse_view_selection_changed)), self, 0);
		/* Setup the commands */
		_tmp2 = NULL;
		self->priv->_commands = (_tmp2 = g_hash_table_new (g_direct_hash, g_direct_equal), (self->priv->_commands == NULL ? NULL : (self->priv->_commands = (g_hash_table_unref (self->priv->_commands), NULL))), _tmp2);
		types = seahorse_registry_find_types (seahorse_registry_get (), "commands", NULL, NULL);
		{
			GList* typ_collection;
			GList* typ_it;
			typ_collection = types;
			for (typ_it = typ_collection; typ_it != NULL; typ_it = typ_it->next) {
				GType typ;
				typ = GPOINTER_TO_INT (typ_it->data);
				{
					SeahorseCommands* commands;
					SeahorseCommands* _tmp3;
					GtkActionGroup* _tmp4;
					GtkActionGroup* actions;
					const char* _tmp5;
					char* uidef;
					/* Add each commands to our hash table */
					commands = SEAHORSE_COMMANDS (g_object_new (typ, "view", self, NULL, NULL));
					_tmp3 = NULL;
					g_hash_table_insert (self->priv->_commands, GINT_TO_POINTER (seahorse_commands_get_ktype (commands)), (_tmp3 = commands, (_tmp3 == NULL ? NULL : g_object_ref (_tmp3))));
					/* Add the UI for each commands */
					_tmp4 = NULL;
					actions = (_tmp4 = seahorse_commands_get_command_actions (commands), (_tmp4 == NULL ? NULL : g_object_ref (_tmp4)));
					if (actions != NULL) {
						seahorse_viewer_include_actions (self, actions);
					}
					_tmp5 = NULL;
					uidef = (_tmp5 = seahorse_commands_get_ui_definition (commands), (_tmp5 == NULL ? NULL : g_strdup (_tmp5)));
					if (uidef != NULL && g_utf8_strlen (uidef, -1) > 0) {
						gtk_ui_manager_add_ui_from_string (self->priv->_ui_manager, uidef, ((glong) (-1)), &inner_error);
						if (inner_error != NULL) {
							g_critical ("file %s: line %d: uncaught error: %s", __FILE__, __LINE__, inner_error->message);
							g_clear_error (&inner_error);
						}
					}
					(commands == NULL ? NULL : (commands = (g_object_unref (commands), NULL)));
					(actions == NULL ? NULL : (actions = (g_object_unref (actions), NULL)));
					uidef = (g_free (uidef), NULL);
				}
			}
		}
		(win == NULL ? NULL : (win = (g_object_unref (win), NULL)));
		(types == NULL ? NULL : (types = (g_list_free (types), NULL)));
	}
	return obj;
}


static void seahorse_viewer_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SeahorseViewer * self;
	self = SEAHORSE_VIEWER (object);
	switch (property_id) {
		case SEAHORSE_VIEWER_SELECTED_KEY:
		g_value_set_object (value, seahorse_viewer_real_get_selected_key (self));
		break;
		case SEAHORSE_VIEWER_CURRENT_KEYSET:
		g_value_set_object (value, seahorse_viewer_real_get_current_keyset (self));
		break;
		case SEAHORSE_VIEWER_WINDOW:
		g_value_set_object (value, seahorse_viewer_real_get_window (self));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_viewer_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
	SeahorseViewer * self;
	self = SEAHORSE_VIEWER (object);
	switch (property_id) {
		case SEAHORSE_VIEWER_SELECTED_KEY:
		seahorse_viewer_real_set_selected_key (self, g_value_get_object (value));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_viewer_class_init (SeahorseViewerClass * klass) {
	seahorse_viewer_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseViewerPrivate));
	G_OBJECT_CLASS (klass)->get_property = seahorse_viewer_get_property;
	G_OBJECT_CLASS (klass)->set_property = seahorse_viewer_set_property;
	G_OBJECT_CLASS (klass)->constructor = seahorse_viewer_constructor;
	G_OBJECT_CLASS (klass)->dispose = seahorse_viewer_dispose;
	SEAHORSE_VIEWER_CLASS (klass)->get_selected_keys = seahorse_viewer_real_get_selected_keys;
	SEAHORSE_VIEWER_CLASS (klass)->set_selected_keys = seahorse_viewer_real_set_selected_keys;
	SEAHORSE_VIEWER_CLASS (klass)->get_selected_key_and_uid = seahorse_viewer_real_get_selected_key_and_uid;
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_VIEWER_SELECTED_KEY, "selected-key");
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_VIEWER_CURRENT_KEYSET, "current-keyset");
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_VIEWER_WINDOW, "window");
}


static void seahorse_viewer_seahorse_view_interface_init (SeahorseViewIface * iface) {
	seahorse_viewer_seahorse_view_parent_iface = g_type_interface_peek_parent (iface);
	iface->get_selected_keys = seahorse_viewer_get_selected_keys;
	iface->set_selected_keys = seahorse_viewer_set_selected_keys;
	iface->get_selected_key_and_uid = seahorse_viewer_get_selected_key_and_uid;
}


static void seahorse_viewer_instance_init (SeahorseViewer * self) {
	self->priv = SEAHORSE_VIEWER_GET_PRIVATE (self);
}


static void seahorse_viewer_dispose (GObject * obj) {
	SeahorseViewer * self;
	self = SEAHORSE_VIEWER (obj);
	(self->priv->_ui_manager == NULL ? NULL : (self->priv->_ui_manager = (g_object_unref (self->priv->_ui_manager), NULL)));
	(self->priv->_key_actions == NULL ? NULL : (self->priv->_key_actions = (g_object_unref (self->priv->_key_actions), NULL)));
	(self->priv->_commands == NULL ? NULL : (self->priv->_commands = (g_hash_table_unref (self->priv->_commands), NULL)));
	G_OBJECT_CLASS (seahorse_viewer_parent_class)->dispose (obj);
}


GType seahorse_viewer_get_type (void) {
	static GType seahorse_viewer_type_id = 0;
	if (G_UNLIKELY (seahorse_viewer_type_id == 0)) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorseViewerClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_viewer_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorseViewer), 0, (GInstanceInitFunc) seahorse_viewer_instance_init };
		static const GInterfaceInfo seahorse_view_info = { (GInterfaceInitFunc) seahorse_viewer_seahorse_view_interface_init, (GInterfaceFinalizeFunc) NULL, NULL};
		seahorse_viewer_type_id = g_type_register_static (SEAHORSE_TYPE_WIDGET, "SeahorseViewer", &g_define_type_info, G_TYPE_FLAG_ABSTRACT);
		g_type_add_interface_static (seahorse_viewer_type_id, SEAHORSE_TYPE_VIEW, &seahorse_view_info);
	}
	return seahorse_viewer_type_id;
}


static void _vala_array_free (gpointer array, gint array_length, GDestroyNotify destroy_func) {
	if (array != NULL && destroy_func != NULL) {
		int i;
		if (array_length >= 0)
		for (i = 0; i < array_length; i = i + 1) {
			if (((gpointer*) (array))[i] != NULL)
			destroy_func (((gpointer*) (array))[i]);
		}
		else
		for (i = 0; ((gpointer*) (array))[i] != NULL; i = i + 1) {
			destroy_func (((gpointer*) (array))[i]);
		}
	}
	g_free (array);
}




