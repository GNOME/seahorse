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

#include "seahorse-key-manager.h"
#include <seahorse-types.h>
#include <seahorse-key.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <string.h>
#include <seahorse-widget.h>
#include <seahorse-progress.h>
#include <seahorse-key-manager-store.h>
#include <seahorse-set.h>
#include <seahorse-context.h>
#include <gdk/gdk.h>
#include <seahorse-util.h>
#include <seahorse-source.h>
#include <gio/gio.h>
#include <seahorse-windows.h>
#include <seahorse-gconf.h>
#include <seahorse-preferences.h>
#include <gconf/gconf-client.h>
#include <gconf/gconf.h>
#include <common/seahorse-registry.h>
#include <config.h>
#include "seahorse-generate-select.h"


#define SEAHORSE_KEY_MANAGER_TYPE_TARGETS (seahorse_key_manager_targets_get_type ())

#define SEAHORSE_KEY_MANAGER_TYPE_TABS (seahorse_key_manager_tabs_get_type ())
typedef struct _SeahorseKeyManagerTabInfo SeahorseKeyManagerTabInfo;

typedef enum  {
	SEAHORSE_KEY_MANAGER_TARGETS_PLAIN,
	SEAHORSE_KEY_MANAGER_TARGETS_URIS
} SeahorseKeyManagerTargets;

typedef enum  {
	SEAHORSE_KEY_MANAGER_TABS_PUBLIC = 0,
	SEAHORSE_KEY_MANAGER_TABS_TRUSTED,
	SEAHORSE_KEY_MANAGER_TABS_PRIVATE,
	SEAHORSE_KEY_MANAGER_TABS_PASSWORD,
	SEAHORSE_KEY_MANAGER_TABS_NUM_TABS
} SeahorseKeyManagerTabs;

struct _SeahorseKeyManagerTabInfo {
	guint id;
	gint page;
	GtkTreeView* view;
	SeahorseSet* objects;
	GtkWidget* widget;
	SeahorseKeyManagerStore* store;
};



struct _SeahorseKeyManagerPrivate {
	GtkNotebook* _notebook;
	GtkActionGroup* _view_actions;
	GtkEntry* _filter_entry;
	GQuark _track_selected_id;
	guint _track_selected_tab;
	gboolean _loaded_gnome_keyring;
	SeahorseKeyManagerTabInfo* _tabs;
	gint _tabs_length1;
};

#define SEAHORSE_KEY_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_TYPE_KEY_MANAGER, SeahorseKeyManagerPrivate))
enum  {
	SEAHORSE_KEY_MANAGER_DUMMY_PROPERTY,
	SEAHORSE_KEY_MANAGER_SELECTED
};
GType seahorse_key_manager_targets_get_type (void);
GType seahorse_key_manager_tabs_get_type (void);
static SeahorseKeyManager* seahorse_key_manager_new (const char* ident);
static GList* seahorse_key_manager_real_get_selected_objects (SeahorseView* base);
static void seahorse_key_manager_real_set_selected_objects (SeahorseView* base, GList* objects);
static SeahorseObject* seahorse_key_manager_real_get_selected_object_and_uid (SeahorseView* base, guint* uid);
static SeahorseKeyManagerTabInfo* seahorse_key_manager_get_tab_for_object (SeahorseKeyManager* self, SeahorseObject* obj);
static GtkTreeView* seahorse_key_manager_get_current_view (SeahorseKeyManager* self);
static guint seahorse_key_manager_get_tab_id (SeahorseKeyManager* self, SeahorseKeyManagerTabInfo* tab);
static SeahorseKeyManagerTabInfo* seahorse_key_manager_get_tab_info (SeahorseKeyManager* self, gint page);
static void seahorse_key_manager_set_tab_current (SeahorseKeyManager* self, SeahorseKeyManagerTabInfo* tab);
static void _seahorse_key_manager_on_view_selection_changed_gtk_tree_selection_changed (GtkTreeSelection* _sender, gpointer self);
static void _seahorse_key_manager_on_row_activated_gtk_tree_view_row_activated (GtkTreeView* _sender, GtkTreePath* path, GtkTreeViewColumn* column, gpointer self);
static gboolean _seahorse_key_manager_on_key_list_button_pressed_gtk_widget_button_press_event (GtkTreeView* _sender, GdkEventButton* event, gpointer self);
static gboolean _seahorse_key_manager_on_key_list_popup_menu_gtk_widget_popup_menu (GtkTreeView* _sender, gpointer self);
static void seahorse_key_manager_initialize_tab (SeahorseKeyManager* self, const char* tabwidget, guint tabid, const char* viewwidget, SeahorseObjectPredicate* pred);
static gboolean seahorse_key_manager_on_first_timer (SeahorseKeyManager* self);
static void seahorse_key_manager_on_filter_changed (SeahorseKeyManager* self, GtkEntry* entry);
static gboolean _seahorse_key_manager_fire_selection_changed_gsource_func (gpointer self);
static void seahorse_key_manager_on_view_selection_changed (SeahorseKeyManager* self, GtkTreeSelection* selection);
static void seahorse_key_manager_on_row_activated (SeahorseKeyManager* self, GtkTreeView* view, GtkTreePath* path, GtkTreeViewColumn* column);
static gboolean seahorse_key_manager_on_key_list_button_pressed (SeahorseKeyManager* self, GtkWidget* widget, GdkEventButton* event);
static gboolean seahorse_key_manager_on_key_list_popup_menu (SeahorseKeyManager* self, GtkTreeView* view);
static void seahorse_key_manager_on_key_generate (SeahorseKeyManager* self, GtkAction* action);
static void seahorse_key_manager_on_new_button_clicked (SeahorseKeyManager* self, GtkWidget* widget);
static void seahorse_key_manager_imported_keys (SeahorseKeyManager* self, SeahorseOperation* op);
static void _seahorse_key_manager_imported_keys_seahorse_done_func (SeahorseOperation* op, gpointer self);
static void seahorse_key_manager_import_files (SeahorseKeyManager* self, char** uris, int uris_length1);
static void seahorse_key_manager_import_prompt (SeahorseKeyManager* self);
static void seahorse_key_manager_on_key_import_file (SeahorseKeyManager* self, GtkAction* action);
static void seahorse_key_manager_on_import_button_clicked (SeahorseKeyManager* self, GtkWidget* widget);
static void seahorse_key_manager_import_text (SeahorseKeyManager* self, const char* text);
static void seahorse_key_manager_on_target_drag_data_received (SeahorseKeyManager* self, GtkWindow* window, GdkDragContext* context, gint x, gint y, GtkSelectionData* selection_data, guint info, guint time_);
static void seahorse_key_manager_clipboard_received (SeahorseKeyManager* self, GtkClipboard* board, const char* text);
static void _seahorse_key_manager_clipboard_received_gtk_clipboard_text_received_func (GtkClipboard* clipboard, const char* text, gpointer self);
static void seahorse_key_manager_on_key_import_clipboard (SeahorseKeyManager* self, GtkAction* action);
static void seahorse_key_manager_on_remote_find (SeahorseKeyManager* self, GtkAction* action);
static void seahorse_key_manager_on_remote_sync (SeahorseKeyManager* self, GtkAction* action);
static void seahorse_key_manager_on_app_quit (SeahorseKeyManager* self, GtkAction* action);
static gboolean seahorse_key_manager_on_delete_event (SeahorseKeyManager* self, GtkWidget* widget, GdkEvent* event);
static void seahorse_key_manager_on_view_type_activate (SeahorseKeyManager* self, GtkToggleAction* action);
static void seahorse_key_manager_on_view_expires_activate (SeahorseKeyManager* self, GtkToggleAction* action);
static void seahorse_key_manager_on_view_validity_activate (SeahorseKeyManager* self, GtkToggleAction* action);
static void seahorse_key_manager_on_view_trust_activate (SeahorseKeyManager* self, GtkToggleAction* action);
static void seahorse_key_manager_on_gconf_notify (SeahorseKeyManager* self, GConfClient* client, guint cnxn_id, GConfEntry* entry);
static gboolean seahorse_key_manager_fire_selection_changed (SeahorseKeyManager* self);
static void seahorse_key_manager_on_tab_changed (SeahorseKeyManager* self, GtkNotebook* notebook, void* unused, guint page_num);
static void seahorse_key_manager_load_gnome_keyring_items (SeahorseKeyManager* self);
static void _seahorse_key_manager_on_app_quit_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_key_manager_on_key_generate_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_key_manager_on_key_import_file_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_key_manager_on_key_import_clipboard_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_key_manager_on_remote_find_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_key_manager_on_remote_sync_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_key_manager_on_view_type_activate_gtk_action_activate (GtkToggleAction* _sender, gpointer self);
static void _seahorse_key_manager_on_view_expires_activate_gtk_action_activate (GtkToggleAction* _sender, gpointer self);
static void _seahorse_key_manager_on_view_trust_activate_gtk_action_activate (GtkToggleAction* _sender, gpointer self);
static void _seahorse_key_manager_on_view_validity_activate_gtk_action_activate (GtkToggleAction* _sender, gpointer self);
static void _seahorse_key_manager_on_gconf_notify_gconf_client_notify_func (GConfClient* client, guint cnxn_id, GConfEntry* entry, gpointer self);
static gboolean _seahorse_key_manager_on_delete_event_gtk_widget_delete_event (GtkWidget* _sender, GdkEvent* event, gpointer self);
static void _seahorse_key_manager_on_import_button_clicked_gtk_button_clicked (GtkButton* _sender, gpointer self);
static void _seahorse_key_manager_on_new_button_clicked_gtk_button_clicked (GtkButton* _sender, gpointer self);
static void _seahorse_key_manager_on_tab_changed_gtk_notebook_switch_page (GtkNotebook* _sender, void* page, guint page_num, gpointer self);
static void _seahorse_key_manager_on_filter_changed_gtk_editable_changed (GtkEntry* _sender, gpointer self);
static void _seahorse_key_manager_on_target_drag_data_received_gtk_widget_drag_data_received (GtkWindow* _sender, GdkDragContext* context, gint x, gint y, GtkSelectionData* selection_data, guint info, guint time_, gpointer self);
static gboolean _seahorse_key_manager_on_first_timer_gsource_func (gpointer self);
static GObject * seahorse_key_manager_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties);
static gpointer seahorse_key_manager_parent_class = NULL;
static SeahorseViewIface* seahorse_key_manager_seahorse_view_parent_iface = NULL;
static void seahorse_key_manager_dispose (GObject * obj);
static void _vala_array_free (gpointer array, gint array_length, GDestroyNotify destroy_func);
static int _vala_strcmp0 (const char * str1, const char * str2);

static const SeahorseObjectPredicate SEAHORSE_KEY_MANAGER_PRED_PUBLIC = {((GQuark) (0)), ((GQuark) (0)), SEAHORSE_LOCATION_LOCAL, SEAHORSE_USAGE_PUBLIC_KEY, ((guint) (0)), ((guint) (SKEY_FLAG_TRUSTED | SKEY_FLAG_IS_VALID)), NULL};
static const SeahorseObjectPredicate SEAHORSE_KEY_MANAGER_PRED_TRUSTED = {((GQuark) (0)), ((GQuark) (0)), SEAHORSE_LOCATION_LOCAL, SEAHORSE_USAGE_PUBLIC_KEY, ((guint) (SKEY_FLAG_TRUSTED | SKEY_FLAG_IS_VALID)), ((guint) (0)), NULL};
static const SeahorseObjectPredicate SEAHORSE_KEY_MANAGER_PRED_PRIVATE = {((GQuark) (0)), ((GQuark) (0)), SEAHORSE_LOCATION_LOCAL, SEAHORSE_USAGE_PRIVATE_KEY, ((guint) (0)), ((guint) (0)), NULL};
static const SeahorseObjectPredicate SEAHORSE_KEY_MANAGER_PRED_PASSWORD = {((GQuark) (0)), ((GQuark) (0)), SEAHORSE_LOCATION_LOCAL, SEAHORSE_USAGE_CREDENTIALS, ((guint) (0)), ((guint) (0)), NULL};
static const GtkActionEntry SEAHORSE_KEY_MANAGER_GENERAL_ENTRIES[] = {{"remote-menu", NULL, N_ ("_Remote")}, {"app-quit", GTK_STOCK_QUIT, N_ ("_Quit"), "<control>Q", N_ ("Close this program"), ((GCallback) (NULL))}, {"key-generate", GTK_STOCK_NEW, N_ ("_Create New Key..."), "<control>N", N_ ("Create a new personal key"), ((GCallback) (NULL))}, {"key-import-file", GTK_STOCK_OPEN, N_ ("_Import..."), "<control>I", N_ ("Import keys into your key ring from a file"), ((GCallback) (NULL))}, {"key-import-clipboard", GTK_STOCK_PASTE, N_ ("Paste _Keys"), "<control>V", N_ ("Import keys from the clipboard"), ((GCallback) (NULL))}};
static const GtkActionEntry SEAHORSE_KEY_MANAGER_SERVER_ENTRIES[] = {{"remote-find", GTK_STOCK_FIND, N_ ("_Find Remote Keys..."), "", N_ ("Search for keys on a key server"), ((GCallback) (NULL))}, {"remote-sync", GTK_STOCK_REFRESH, N_ ("_Sync and Publish Keys..."), "", N_ ("Publish and/or synchronize your keys with those online."), ((GCallback) (NULL))}};
static const GtkToggleActionEntry SEAHORSE_KEY_MANAGER_VIEW_ENTRIES[] = {{"view-type", NULL, N_ ("T_ypes"), NULL, N_ ("Show type column"), NULL, FALSE}, {"view-expires", NULL, N_ ("_Expiry"), NULL, N_ ("Show expiry column"), NULL, FALSE}, {"view-trust", NULL, N_ ("_Trust"), NULL, N_ ("Show owner trust column"), NULL, FALSE}, {"view-validity", NULL, N_ ("_Validity"), NULL, N_ ("Show validity column"), NULL, FALSE}};



GType seahorse_key_manager_targets_get_type (void) {
	static GType seahorse_key_manager_targets_type_id = 0;
	if (G_UNLIKELY (seahorse_key_manager_targets_type_id == 0)) {
		static const GEnumValue values[] = {{SEAHORSE_KEY_MANAGER_TARGETS_PLAIN, "SEAHORSE_KEY_MANAGER_TARGETS_PLAIN", "plain"}, {SEAHORSE_KEY_MANAGER_TARGETS_URIS, "SEAHORSE_KEY_MANAGER_TARGETS_URIS", "uris"}, {0, NULL, NULL}};
		seahorse_key_manager_targets_type_id = g_enum_register_static ("SeahorseKeyManagerTargets", values);
	}
	return seahorse_key_manager_targets_type_id;
}



GType seahorse_key_manager_tabs_get_type (void) {
	static GType seahorse_key_manager_tabs_type_id = 0;
	if (G_UNLIKELY (seahorse_key_manager_tabs_type_id == 0)) {
		static const GEnumValue values[] = {{SEAHORSE_KEY_MANAGER_TABS_PUBLIC, "SEAHORSE_KEY_MANAGER_TABS_PUBLIC", "public"}, {SEAHORSE_KEY_MANAGER_TABS_TRUSTED, "SEAHORSE_KEY_MANAGER_TABS_TRUSTED", "trusted"}, {SEAHORSE_KEY_MANAGER_TABS_PRIVATE, "SEAHORSE_KEY_MANAGER_TABS_PRIVATE", "private"}, {SEAHORSE_KEY_MANAGER_TABS_PASSWORD, "SEAHORSE_KEY_MANAGER_TABS_PASSWORD", "password"}, {SEAHORSE_KEY_MANAGER_TABS_NUM_TABS, "SEAHORSE_KEY_MANAGER_TABS_NUM_TABS", "num-tabs"}, {0, NULL, NULL}};
		seahorse_key_manager_tabs_type_id = g_enum_register_static ("SeahorseKeyManagerTabs", values);
	}
	return seahorse_key_manager_tabs_type_id;
}


static SeahorseKeyManager* seahorse_key_manager_new (const char* ident) {
	GParameter * __params;
	GParameter * __params_it;
	SeahorseKeyManager * self;
	g_return_val_if_fail (ident != NULL, NULL);
	__params = g_new0 (GParameter, 1);
	__params_it = __params;
	__params_it->name = "name";
	g_value_init (&__params_it->value, G_TYPE_STRING);
	g_value_set_string (&__params_it->value, ident);
	__params_it++;
	self = g_object_newv (SEAHORSE_TYPE_KEY_MANAGER, __params_it - __params, __params);
	while (__params_it > __params) {
		--__params_it;
		g_value_unset (&__params_it->value);
	}
	g_free (__params);
	return self;
}


GtkWindow* seahorse_key_manager_show (SeahorseOperation* op) {
	SeahorseKeyManager* man;
	GtkWindow* _tmp0;
	g_return_val_if_fail (SEAHORSE_IS_OPERATION (op), NULL);
	man = g_object_ref_sink (seahorse_key_manager_new ("key-manager"));
	g_object_ref (G_OBJECT (man));
	/* Destorys itself with destroy */
	seahorse_progress_status_set_operation (SEAHORSE_WIDGET (man), op);
	_tmp0 = NULL;
	return (_tmp0 = GTK_WINDOW (seahorse_widget_get_toplevel (SEAHORSE_WIDGET (man))), (man == NULL ? NULL : (man = (g_object_unref (man), NULL))), _tmp0);
}


static GList* seahorse_key_manager_real_get_selected_objects (SeahorseView* base) {
	SeahorseKeyManager * self;
	SeahorseKeyManagerTabInfo* tab;
	self = SEAHORSE_KEY_MANAGER (base);
	tab = seahorse_key_manager_get_tab_info (self, -1);
	if (tab == NULL) {
		return NULL;
	}
	return seahorse_key_manager_store_get_selected_keys ((*tab).view);
}


static void seahorse_key_manager_real_set_selected_objects (SeahorseView* base, GList* objects) {
	SeahorseKeyManager * self;
	GList** _tmp1;
	gint tab_lists_length1;
	gint _tmp0;
	GList** tab_lists;
	guint highest_matched;
	SeahorseKeyManagerTabInfo* highest_tab;
	self = SEAHORSE_KEY_MANAGER (base);
	g_return_if_fail (objects != NULL);
	_tmp1 = NULL;
	tab_lists = (_tmp1 = g_new0 (GList*, (_tmp0 = ((gint) (SEAHORSE_KEY_MANAGER_TABS_NUM_TABS))) + 1), tab_lists_length1 = _tmp0, _tmp1);
	/* Break objects into what's on each tab */
	{
		GList* obj_collection;
		GList* obj_it;
		obj_collection = objects;
		for (obj_it = obj_collection; obj_it != NULL; obj_it = obj_it->next) {
			SeahorseObject* _tmp2;
			SeahorseObject* obj;
			_tmp2 = NULL;
			obj = (_tmp2 = ((SeahorseObject*) (obj_it->data)), (_tmp2 == NULL ? NULL : g_object_ref (_tmp2)));
			{
				SeahorseKeyManagerTabInfo* tab;
				tab = seahorse_key_manager_get_tab_for_object (self, obj);
				if (tab == NULL) {
					(obj == NULL ? NULL : (obj = (g_object_unref (obj), NULL)));
					continue;
				}
				g_assert ((*tab).id < ((gint) (SEAHORSE_KEY_MANAGER_TABS_NUM_TABS)));
				tab_lists[(*tab).id] = g_list_prepend (tab_lists[(*tab).id], obj);
				(obj == NULL ? NULL : (obj = (g_object_unref (obj), NULL)));
			}
		}
	}
	highest_matched = ((guint) (0));
	highest_tab = NULL;
	{
		gint i;
		i = 0;
		for (; i < ((gint) (SEAHORSE_KEY_MANAGER_TABS_NUM_TABS)); (i = i + 1)) {
			GList* list;
			SeahorseKeyManagerTabInfo* tab;
			guint num;
			list = tab_lists[i];
			tab = &self->priv->_tabs[i];
			/* Save away the tab that had the most objects */
			num = g_list_length (list);
			if (num > highest_matched) {
				highest_matched = num;
				highest_tab = tab;
			}
			/* Select the objects on that tab */
			seahorse_key_manager_store_set_selected_keys ((*tab).view, list);
		}
	}
	/* Change to the tab with the most objects */
	if (highest_tab != NULL) {
		seahorse_key_manager_set_tab_current (self, highest_tab);
	}
	tab_lists = (_vala_array_free (tab_lists, tab_lists_length1, ((GDestroyNotify) (g_list_free))), NULL);
}


static SeahorseObject* seahorse_key_manager_real_get_selected_object_and_uid (SeahorseView* base, guint* uid) {
	SeahorseKeyManager * self;
	SeahorseKeyManagerTabInfo* tab;
	self = SEAHORSE_KEY_MANAGER (base);
	tab = seahorse_key_manager_get_tab_info (self, -1);
	if (tab == NULL) {
		return NULL;
	}
	return SEAHORSE_OBJECT (seahorse_key_manager_store_get_selected_key ((*tab).view, &(*uid)));
}


static SeahorseKeyManagerTabInfo* seahorse_key_manager_get_tab_for_object (SeahorseKeyManager* self, SeahorseObject* obj) {
	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), NULL);
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (obj), NULL);
	{
		gint i;
		i = 0;
		for (; i < ((gint) (SEAHORSE_KEY_MANAGER_TABS_NUM_TABS)); (i = i + 1)) {
			SeahorseKeyManagerTabInfo* tab;
			tab = &self->priv->_tabs[i];
			if (seahorse_set_has_object ((*tab).objects, obj)) {
				return tab;
			}
		}
	}
	return NULL;
}


static GtkTreeView* seahorse_key_manager_get_current_view (SeahorseKeyManager* self) {
	SeahorseKeyManagerTabInfo* tab;
	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), NULL);
	tab = seahorse_key_manager_get_tab_info (self, -1);
	if (tab == NULL) {
		return NULL;
	}
	return (*tab).view;
}


static guint seahorse_key_manager_get_tab_id (SeahorseKeyManager* self, SeahorseKeyManagerTabInfo* tab) {
	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), 0U);
	if (tab == NULL) {
		return ((guint) (0));
	}
	return (*tab).id;
}


static SeahorseKeyManagerTabInfo* seahorse_key_manager_get_tab_info (SeahorseKeyManager* self, gint page) {
	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), NULL);
	if (page < 0) {
		page = gtk_notebook_get_current_page (self->priv->_notebook);
	}
	if (page < 0) {
		return NULL;
	}
	{
		gint i;
		i = 0;
		for (; i < ((gint) (SEAHORSE_KEY_MANAGER_TABS_NUM_TABS)); (i = i + 1)) {
			SeahorseKeyManagerTabInfo* tab;
			tab = &self->priv->_tabs[i];
			if ((*tab).page == page) {
				return tab;
			}
		}
	}
	return NULL;
}


static void seahorse_key_manager_set_tab_current (SeahorseKeyManager* self, SeahorseKeyManagerTabInfo* tab) {
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	gtk_notebook_set_current_page (self->priv->_notebook, (*tab).page);
	g_signal_emit_by_name (G_OBJECT (SEAHORSE_VIEW (self)), "selection-changed");
}


static void _seahorse_key_manager_on_view_selection_changed_gtk_tree_selection_changed (GtkTreeSelection* _sender, gpointer self) {
	seahorse_key_manager_on_view_selection_changed (self, _sender);
}


static void _seahorse_key_manager_on_row_activated_gtk_tree_view_row_activated (GtkTreeView* _sender, GtkTreePath* path, GtkTreeViewColumn* column, gpointer self) {
	seahorse_key_manager_on_row_activated (self, _sender, path, column);
}


static gboolean _seahorse_key_manager_on_key_list_button_pressed_gtk_widget_button_press_event (GtkTreeView* _sender, GdkEventButton* event, gpointer self) {
	return seahorse_key_manager_on_key_list_button_pressed (self, _sender, event);
}


static gboolean _seahorse_key_manager_on_key_list_popup_menu_gtk_widget_popup_menu (GtkTreeView* _sender, gpointer self) {
	return seahorse_key_manager_on_key_list_popup_menu (self, _sender);
}


static void seahorse_key_manager_initialize_tab (SeahorseKeyManager* self, const char* tabwidget, guint tabid, const char* viewwidget, SeahorseObjectPredicate* pred) {
	SeahorseSet* objects;
	GtkTreeView* _tmp0;
	GtkTreeView* view;
	SeahorseKeyManagerStore* _tmp1;
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (tabwidget != NULL);
	g_return_if_fail (viewwidget != NULL);
	g_assert (tabid < ((gint) (SEAHORSE_KEY_MANAGER_TABS_NUM_TABS)));
	self->priv->_tabs[tabid].id = tabid;
	self->priv->_tabs[tabid].widget = seahorse_widget_get_widget (SEAHORSE_WIDGET (self), tabwidget);
	g_return_if_fail (self->priv->_tabs[tabid].widget != NULL);
	self->priv->_tabs[tabid].page = gtk_notebook_page_num (self->priv->_notebook, self->priv->_tabs[tabid].widget);
	g_return_if_fail (self->priv->_tabs[tabid].page >= 0);
	objects = seahorse_set_new_full (&(*pred));
	self->priv->_tabs[tabid].objects = objects;
	/* init key list & selection settings */
	_tmp0 = NULL;
	view = (_tmp0 = GTK_TREE_VIEW (seahorse_widget_get_widget (SEAHORSE_WIDGET (self), viewwidget)), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	self->priv->_tabs[tabid].view = view;
	g_return_if_fail (view != NULL);
	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (view), GTK_SELECTION_MULTIPLE);
	g_signal_connect_object (gtk_tree_view_get_selection (view), "changed", ((GCallback) (_seahorse_key_manager_on_view_selection_changed_gtk_tree_selection_changed)), self, 0);
	g_signal_connect_object (view, "row-activated", ((GCallback) (_seahorse_key_manager_on_row_activated_gtk_tree_view_row_activated)), self, 0);
	g_signal_connect_object (GTK_WIDGET (view), "button-press-event", ((GCallback) (_seahorse_key_manager_on_key_list_button_pressed_gtk_widget_button_press_event)), self, 0);
	g_signal_connect_object (GTK_WIDGET (view), "popup-menu", ((GCallback) (_seahorse_key_manager_on_key_list_popup_menu_gtk_widget_popup_menu)), self, 0);
	gtk_widget_realize (GTK_WIDGET (view));
	/* Add new key store and associate it */
	_tmp1 = NULL;
	self->priv->_tabs[tabid].store = (_tmp1 = seahorse_key_manager_store_new (objects, view), (self->priv->_tabs[tabid].store == NULL ? NULL : (self->priv->_tabs[tabid].store = (g_object_unref (self->priv->_tabs[tabid].store), NULL))), _tmp1);
	(objects == NULL ? NULL : (objects = (g_object_unref (objects), NULL)));
	(view == NULL ? NULL : (view = (g_object_unref (view), NULL)));
}


static gboolean seahorse_key_manager_on_first_timer (SeahorseKeyManager* self) {
	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), FALSE);
	/* Although not all the keys have completed we'll know whether we have 
	 * any or not at this point */
	if (seahorse_context_get_count (seahorse_context_for_app ()) == 0) {
		GtkWidget* _tmp0;
		GtkWidget* widget;
		_tmp0 = NULL;
		widget = (_tmp0 = seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "first-time-box"), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
		gtk_widget_show (widget);
		(widget == NULL ? NULL : (widget = (g_object_unref (widget), NULL)));
	}
	return FALSE;
}


static void seahorse_key_manager_on_filter_changed (SeahorseKeyManager* self, GtkEntry* entry) {
	const char* _tmp0;
	char* text;
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_ENTRY (entry));
	_tmp0 = NULL;
	text = (_tmp0 = gtk_entry_get_text (entry), (_tmp0 == NULL ? NULL : g_strdup (_tmp0)));
	{
		gint i;
		i = 0;
		for (; i < self->priv->_tabs_length1; (i = i + 1)) {
			g_object_set (G_OBJECT (self->priv->_tabs[i].store), "filter", text, NULL, NULL);
		}
	}
	text = (g_free (text), NULL);
}


static gboolean _seahorse_key_manager_fire_selection_changed_gsource_func (gpointer self) {
	return seahorse_key_manager_fire_selection_changed (self);
}


static void seahorse_key_manager_on_view_selection_changed (SeahorseKeyManager* self, GtkTreeSelection* selection) {
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_idle_add (_seahorse_key_manager_fire_selection_changed_gsource_func, self);
}


static void seahorse_key_manager_on_row_activated (SeahorseKeyManager* self, GtkTreeView* view, GtkTreePath* path, GtkTreeViewColumn* column) {
	SeahorseKey* _tmp0;
	SeahorseKey* key;
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_TREE_VIEW (view));
	g_return_if_fail (path != NULL);
	g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (column));
	_tmp0 = NULL;
	key = (_tmp0 = seahorse_key_manager_store_get_key_from_path (view, path, NULL), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	if (key != NULL) {
		seahorse_viewer_show_properties (SEAHORSE_VIEWER (self), SEAHORSE_OBJECT (key));
	}
	(key == NULL ? NULL : (key = (g_object_unref (key), NULL)));
}


static gboolean seahorse_key_manager_on_key_list_button_pressed (SeahorseKeyManager* self, GtkWidget* widget, GdkEventButton* event) {
	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	if ((*event).button == 3) {
		seahorse_viewer_show_context_menu (SEAHORSE_VIEWER (self), (*event).button, (*event).time);
	}
	return FALSE;
}


static gboolean seahorse_key_manager_on_key_list_popup_menu (SeahorseKeyManager* self, GtkTreeView* view) {
	SeahorseObject* _tmp0;
	SeahorseObject* obj;
	gboolean _tmp2;
	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), FALSE);
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), FALSE);
	_tmp0 = NULL;
	obj = (_tmp0 = seahorse_viewer_get_selected (SEAHORSE_VIEWER (self)), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	if (obj != NULL) {
		gboolean _tmp1;
		seahorse_viewer_show_context_menu (SEAHORSE_VIEWER (self), ((guint) (0)), gtk_get_current_event_time ());
		return (_tmp1 = TRUE, (obj == NULL ? NULL : (obj = (g_object_unref (obj), NULL))), _tmp1);
	}
	return (_tmp2 = FALSE, (obj == NULL ? NULL : (obj = (g_object_unref (obj), NULL))), _tmp2);
}


static void seahorse_key_manager_on_key_generate (SeahorseKeyManager* self, GtkAction* action) {
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	seahorse_generate_select_show (seahorse_view_get_window (SEAHORSE_VIEW (self)));
}


static void seahorse_key_manager_on_new_button_clicked (SeahorseKeyManager* self, GtkWidget* widget) {
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_WIDGET (widget));
	seahorse_generate_select_show (seahorse_view_get_window (SEAHORSE_VIEW (self)));
}


static void seahorse_key_manager_imported_keys (SeahorseKeyManager* self, SeahorseOperation* op) {
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (SEAHORSE_IS_OPERATION (op));
	if (!seahorse_operation_is_successful (op)) {
		seahorse_operation_display_error (op, _ ("Couldn't import keys"), GTK_WIDGET (seahorse_view_get_window (SEAHORSE_VIEW (self))));
		return;
	}
	seahorse_viewer_set_status (SEAHORSE_VIEWER (self), _ ("Imported keys"));
}


static void _seahorse_key_manager_imported_keys_seahorse_done_func (SeahorseOperation* op, gpointer self) {
	seahorse_key_manager_imported_keys (self, op);
}


static void seahorse_key_manager_import_files (SeahorseKeyManager* self, char** uris, int uris_length1) {
	GError * inner_error;
	SeahorseMultiOperation* mop;
	GString* errmsg;
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	inner_error = NULL;
	mop = g_object_new (SEAHORSE_TYPE_MULTI_OPERATION, NULL);
	errmsg = g_string_new ("");
	{
		char** uri_collection;
		int uri_collection_length1;
		int uri_it;
		uri_collection = uris;
		uri_collection_length1 = uris_length1;
		for (uri_it = 0; (uris_length1 != -1 && uri_it < uris_length1) || (uris_length1 == -1 && uri_collection[uri_it] != NULL); uri_it = uri_it + 1) {
			const char* _tmp1;
			char* uri;
			_tmp1 = NULL;
			uri = (_tmp1 = uri_collection[uri_it], (_tmp1 == NULL ? NULL : g_strdup (_tmp1)));
			{
				GQuark ktype;
				SeahorseSource* _tmp0;
				SeahorseSource* sksrc;
				if (g_utf8_strlen (uri, -1) == 0) {
					uri = (g_free (uri), NULL);
					continue;
				}
				/* Figure out where to import to */
				ktype = seahorse_util_detect_file_type (uri);
				if (ktype == 0) {
					g_string_append_printf (errmsg, "%s: Invalid file format\n", uri);
					uri = (g_free (uri), NULL);
					continue;
				}
				/* All our supported key types have a local source */
				_tmp0 = NULL;
				sksrc = (_tmp0 = seahorse_context_find_source (seahorse_context_for_app (), ktype, SEAHORSE_LOCATION_LOCAL), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
				g_return_if_fail (sksrc != NULL);
				{
					GFile* file;
					GFileInputStream* input;
					SeahorseOperation* op;
					file = g_file_new_for_uri (uri);
					input = g_file_read (file, NULL, &inner_error);
					if (inner_error != NULL) {
						goto __catch4_g_error;
					}
					op = seahorse_source_import (sksrc, G_INPUT_STREAM (input));
					seahorse_multi_operation_take (mop, op);
					(file == NULL ? NULL : (file = (g_object_unref (file), NULL)));
					(input == NULL ? NULL : (input = (g_object_unref (input), NULL)));
					(op == NULL ? NULL : (op = (g_object_unref (op), NULL)));
				}
				goto __finally4;
				__catch4_g_error:
				{
					GError * ex;
					ex = inner_error;
					inner_error = NULL;
					{
						g_string_append_printf (errmsg, "%s: %s", uri, ex->message);
						(ex == NULL ? NULL : (ex = (g_error_free (ex), NULL)));
						uri = (g_free (uri), NULL);
						(sksrc == NULL ? NULL : (sksrc = (g_object_unref (sksrc), NULL)));
						continue;
					}
				}
				__finally4:
				;
				uri = (g_free (uri), NULL);
				(sksrc == NULL ? NULL : (sksrc = (g_object_unref (sksrc), NULL)));
			}
		}
	}
	if (seahorse_operation_is_running (SEAHORSE_OPERATION (mop))) {
		seahorse_progress_show (SEAHORSE_OPERATION (mop), _ ("Importing keys"), TRUE);
		seahorse_operation_watch (SEAHORSE_OPERATION (mop), _seahorse_key_manager_imported_keys_seahorse_done_func, self, NULL, NULL);
	}
	if (errmsg->len == 0) {
		seahorse_util_show_error (GTK_WIDGET (seahorse_view_get_window (SEAHORSE_VIEW (self))), _ ("Couldn't import keys"), errmsg->str);
	}
	(mop == NULL ? NULL : (mop = (g_object_unref (mop), NULL)));
	(errmsg == NULL ? NULL : (errmsg = (g_string_free (errmsg, TRUE), NULL)));
}


static void seahorse_key_manager_import_prompt (SeahorseKeyManager* self) {
	GtkDialog* _tmp0;
	GtkDialog* dialog;
	char* uri;
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	_tmp0 = NULL;
	dialog = (_tmp0 = seahorse_util_chooser_open_new (_ ("Import Key"), seahorse_view_get_window (SEAHORSE_VIEW (self))), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	seahorse_util_chooser_show_key_files (dialog);
	uri = seahorse_util_chooser_open_prompt (dialog);
	if (uri != NULL) {
		char** _tmp3;
		gint uris_length1;
		char** _tmp2;
		const char* _tmp1;
		char** uris;
		_tmp3 = NULL;
		_tmp2 = NULL;
		_tmp1 = NULL;
		uris = (_tmp3 = (_tmp2 = g_new0 (char*, 1 + 1), _tmp2[0] = (_tmp1 = uri, (_tmp1 == NULL ? NULL : g_strdup (_tmp1))), _tmp2), uris_length1 = 1, _tmp3);
		seahorse_key_manager_import_files (self, uris, uris_length1);
		uris = (_vala_array_free (uris, uris_length1, ((GDestroyNotify) (g_free))), NULL);
	}
	(dialog == NULL ? NULL : (dialog = (g_object_unref (dialog), NULL)));
	uri = (g_free (uri), NULL);
}


static void seahorse_key_manager_on_key_import_file (SeahorseKeyManager* self, GtkAction* action) {
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	seahorse_key_manager_import_prompt (self);
}


static void seahorse_key_manager_on_import_button_clicked (SeahorseKeyManager* self, GtkWidget* widget) {
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_WIDGET (widget));
	seahorse_key_manager_import_prompt (self);
}


static void seahorse_key_manager_import_text (SeahorseKeyManager* self, const char* text) {
	glong len;
	GQuark ktype;
	SeahorseSource* _tmp0;
	SeahorseSource* sksrc;
	GMemoryInputStream* input;
	SeahorseOperation* op;
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (text != NULL);
	len = g_utf8_strlen (text, -1);
	ktype = seahorse_util_detect_data_type (text, len);
	if (ktype == 0) {
		seahorse_util_show_error (GTK_WIDGET (seahorse_view_get_window (SEAHORSE_VIEW (self))), _ ("Couldn't import keys"), _ ("Unrecognized key type, or invalid data format"));
		return;
	}
	/* All our supported key types have a local key source */
	_tmp0 = NULL;
	sksrc = (_tmp0 = seahorse_context_find_source (seahorse_context_for_app (), ktype, SEAHORSE_LOCATION_LOCAL), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	g_return_if_fail (sksrc != NULL);
	/* 
	 * BUG: We cast to get around this bug:
	 * http://bugzilla.gnome.org/show_bug.cgi?id=540662
	 */
	input = G_MEMORY_INPUT_STREAM (g_memory_input_stream_new_from_data (text, len, g_free));
	op = seahorse_source_import (sksrc, G_INPUT_STREAM (input));
	seahorse_progress_show (op, _ ("Importing Keys"), TRUE);
	seahorse_operation_watch (op, _seahorse_key_manager_imported_keys_seahorse_done_func, self, NULL, NULL);
	(sksrc == NULL ? NULL : (sksrc = (g_object_unref (sksrc), NULL)));
	(input == NULL ? NULL : (input = (g_object_unref (input), NULL)));
	(op == NULL ? NULL : (op = (g_object_unref (op), NULL)));
}


static void seahorse_key_manager_on_target_drag_data_received (SeahorseKeyManager* self, GtkWindow* window, GdkDragContext* context, gint x, gint y, GtkSelectionData* selection_data, guint info, guint time_) {
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_WINDOW (window));
	g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
	g_return_if_fail (selection_data != NULL);
	if (info == SEAHORSE_KEY_MANAGER_TARGETS_PLAIN) {
		char* _tmp0;
		_tmp0 = NULL;
		seahorse_key_manager_import_text (self, (_tmp0 = ((char*) (gtk_selection_data_get_text (selection_data)))));
		_tmp0 = (g_free (_tmp0), NULL);
	} else {
		if (info == SEAHORSE_KEY_MANAGER_TARGETS_URIS) {
			char** _tmp1;
			gint uris_length1;
			char** uris;
			_tmp1 = NULL;
			uris = (_tmp1 = gtk_selection_data_get_uris (selection_data), uris_length1 = -1, _tmp1);
			{
				gint i;
				i = 0;
				for (; i < uris_length1; (i = i + 1)) {
					char* _tmp3;
					const char* _tmp2;
					_tmp3 = NULL;
					_tmp2 = NULL;
					uris[i] = (_tmp3 = (_tmp2 = g_strstrip (uris[i]), (_tmp2 == NULL ? NULL : g_strdup (_tmp2))), (uris[i] = (g_free (uris[i]), NULL)), _tmp3);
				}
			}
			seahorse_key_manager_import_files (self, uris, uris_length1);
			uris = (_vala_array_free (uris, uris_length1, ((GDestroyNotify) (g_free))), NULL);
		}
	}
}


static void seahorse_key_manager_clipboard_received (SeahorseKeyManager* self, GtkClipboard* board, const char* text) {
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_CLIPBOARD (board));
	g_return_if_fail (text != NULL);
	if (text != NULL && g_utf8_strlen (text, -1) > 0) {
		seahorse_key_manager_import_text (self, text);
	}
}


static void _seahorse_key_manager_clipboard_received_gtk_clipboard_text_received_func (GtkClipboard* clipboard, const char* text, gpointer self) {
	seahorse_key_manager_clipboard_received (self, clipboard, text);
}


static void seahorse_key_manager_on_key_import_clipboard (SeahorseKeyManager* self, GtkAction* action) {
	GdkAtom atom;
	GtkClipboard* _tmp0;
	GtkClipboard* board;
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	atom = gdk_atom_intern ("CLIPBOARD", FALSE);
	_tmp0 = NULL;
	board = (_tmp0 = gtk_clipboard_get (atom), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	gtk_clipboard_request_text (board, _seahorse_key_manager_clipboard_received_gtk_clipboard_text_received_func, self);
	(board == NULL ? NULL : (board = (g_object_unref (board), NULL)));
}


static void seahorse_key_manager_on_remote_find (SeahorseKeyManager* self, GtkAction* action) {
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	seahorse_keyserver_search_show (seahorse_view_get_window (SEAHORSE_VIEW (self)));
}


static void seahorse_key_manager_on_remote_sync (SeahorseKeyManager* self, GtkAction* action) {
	GList* objects;
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	objects = seahorse_viewer_get_selected_objects (SEAHORSE_VIEWER (self));
	if (objects == NULL) {
		GList* _tmp0;
		_tmp0 = NULL;
		objects = (_tmp0 = seahorse_context_find_objects (seahorse_context_for_app (), ((GQuark) (0)), 0, SEAHORSE_LOCATION_LOCAL), (objects == NULL ? NULL : (objects = (g_list_free (objects), NULL))), _tmp0);
	}
	seahorse_keyserver_sync_show (objects, seahorse_view_get_window (SEAHORSE_VIEW (self)));
	(objects == NULL ? NULL : (objects = (g_list_free (objects), NULL)));
}


static void seahorse_key_manager_on_app_quit (SeahorseKeyManager* self, GtkAction* action) {
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (action == NULL || GTK_IS_ACTION (action));
	seahorse_context_destroy (seahorse_context_for_app ());
}


/* When this window closes we quit seahorse */
static gboolean seahorse_key_manager_on_delete_event (SeahorseKeyManager* self, GtkWidget* widget, GdkEvent* event) {
	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	seahorse_key_manager_on_app_quit (self, NULL);
	return TRUE;
}


static void seahorse_key_manager_on_view_type_activate (SeahorseKeyManager* self, GtkToggleAction* action) {
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));
	seahorse_gconf_set_boolean (SHOW_TYPE_KEY, gtk_toggle_action_get_active (action));
}


static void seahorse_key_manager_on_view_expires_activate (SeahorseKeyManager* self, GtkToggleAction* action) {
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));
	seahorse_gconf_set_boolean (SHOW_EXPIRES_KEY, gtk_toggle_action_get_active (action));
}


static void seahorse_key_manager_on_view_validity_activate (SeahorseKeyManager* self, GtkToggleAction* action) {
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));
	seahorse_gconf_set_boolean (SHOW_VALIDITY_KEY, gtk_toggle_action_get_active (action));
}


static void seahorse_key_manager_on_view_trust_activate (SeahorseKeyManager* self, GtkToggleAction* action) {
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));
	seahorse_gconf_set_boolean (SHOW_TRUST_KEY, gtk_toggle_action_get_active (action));
}


static void seahorse_key_manager_on_gconf_notify (SeahorseKeyManager* self, GConfClient* client, guint cnxn_id, GConfEntry* entry) {
	GtkToggleAction* action;
	const char* _tmp0;
	char* key;
	char* name;
	GtkToggleAction* _tmp6;
	GtkToggleAction* _tmp5;
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GCONF_IS_CLIENT (client));
	g_return_if_fail (entry != NULL);
	action = NULL;
	_tmp0 = NULL;
	key = (_tmp0 = entry->key, (_tmp0 == NULL ? NULL : g_strdup (_tmp0)));
	name = NULL;
	if (_vala_strcmp0 (key, SHOW_TRUST_KEY) == 0) {
		char* _tmp1;
		_tmp1 = NULL;
		name = (_tmp1 = g_strdup ("view-trust"), (name = (g_free (name), NULL)), _tmp1);
	} else {
		if (_vala_strcmp0 (key, SHOW_TYPE_KEY) == 0) {
			char* _tmp2;
			_tmp2 = NULL;
			name = (_tmp2 = g_strdup ("view-type"), (name = (g_free (name), NULL)), _tmp2);
		} else {
			if (_vala_strcmp0 (key, SHOW_EXPIRES_KEY) == 0) {
				char* _tmp3;
				_tmp3 = NULL;
				name = (_tmp3 = g_strdup ("view-expires"), (name = (g_free (name), NULL)), _tmp3);
			} else {
				if (_vala_strcmp0 (key, SHOW_VALIDITY_KEY) == 0) {
					char* _tmp4;
					_tmp4 = NULL;
					name = (_tmp4 = g_strdup ("view-validity"), (name = (g_free (name), NULL)), _tmp4);
				} else {
					(action == NULL ? NULL : (action = (g_object_unref (action), NULL)));
					key = (g_free (key), NULL);
					name = (g_free (name), NULL);
					return;
				}
			}
		}
	}
	_tmp6 = NULL;
	_tmp5 = NULL;
	action = (_tmp6 = (_tmp5 = GTK_TOGGLE_ACTION (gtk_action_group_get_action (self->priv->_view_actions, name)), (_tmp5 == NULL ? NULL : g_object_ref (_tmp5))), (action == NULL ? NULL : (action = (g_object_unref (action), NULL))), _tmp6);
	g_return_if_fail (action != NULL);
	gtk_toggle_action_set_active (action, gconf_value_get_bool (entry->value));
	(action == NULL ? NULL : (action = (g_object_unref (action), NULL)));
	key = (g_free (key), NULL);
	name = (g_free (name), NULL);
}


static gboolean seahorse_key_manager_fire_selection_changed (SeahorseKeyManager* self) {
	gboolean selected;
	gint rows;
	GtkTreeView* _tmp0;
	GtkTreeView* view;
	gboolean dotracking;
	guint tabid;
	GQuark keyid;
	gboolean _tmp3;
	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), FALSE);
	selected = FALSE;
	rows = 0;
	_tmp0 = NULL;
	view = (_tmp0 = seahorse_key_manager_get_current_view (self), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	if (view != NULL) {
		GtkTreeSelection* _tmp1;
		GtkTreeSelection* selection;
		_tmp1 = NULL;
		selection = (_tmp1 = gtk_tree_view_get_selection (view), (_tmp1 == NULL ? NULL : g_object_ref (_tmp1)));
		rows = gtk_tree_selection_count_selected_rows (selection);
		selected = rows > 0;
		(selection == NULL ? NULL : (selection = (g_object_unref (selection), NULL)));
	}
	dotracking = TRUE;
	/* See which tab we're on, if different from previous, no tracking */
	tabid = seahorse_key_manager_get_tab_id (self, seahorse_key_manager_get_tab_info (self, -1));
	if (tabid != self->priv->_track_selected_tab) {
		dotracking = FALSE;
		self->priv->_track_selected_tab = tabid;
	}
	/* Retrieve currently tracked, and reset tracking */
	keyid = self->priv->_track_selected_id;
	self->priv->_track_selected_id = ((GQuark) (0));
	/* no selection, see if selected key moved to another tab */
	if (dotracking && rows == 0 && keyid != 0) {
		SeahorseObject* _tmp2;
		SeahorseObject* obj;
		/* Find it */
		_tmp2 = NULL;
		obj = (_tmp2 = seahorse_context_find_object (seahorse_context_for_app (), keyid, SEAHORSE_LOCATION_LOCAL), (_tmp2 == NULL ? NULL : g_object_ref (_tmp2)));
		if (obj != NULL) {
			SeahorseKeyManagerTabInfo* tab;
			/* If it's still around, then select it */
			tab = seahorse_key_manager_get_tab_for_object (self, obj);
			if (tab != NULL && tab != seahorse_key_manager_get_tab_info (self, -1)) {
				/* Make sure we don't end up in a loop  */
				g_assert (self->priv->_track_selected_id == 0);
				seahorse_viewer_set_selected (SEAHORSE_VIEWER (self), obj);
			}
		}
		(obj == NULL ? NULL : (obj = (g_object_unref (obj), NULL)));
	}
	if (selected) {
		GList* objects;
		seahorse_viewer_set_numbered_status (SEAHORSE_VIEWER (self), ngettext ("Selected %d key", "Selected %d keys", rows), rows);
		objects = seahorse_viewer_get_selected_objects (SEAHORSE_VIEWER (self));
		/* If one key is selected then mark it down for following across tabs */
		if (((SeahorseObject*) (objects->data)) != NULL) {
			self->priv->_track_selected_id = seahorse_object_get_id (((SeahorseObject*) (((SeahorseObject*) (objects->data)))));
		}
		(objects == NULL ? NULL : (objects = (g_list_free (objects), NULL)));
	}
	/* Fire the signal */
	g_signal_emit_by_name (G_OBJECT (SEAHORSE_VIEW (self)), "selection-changed");
	/* This is called as a one-time idle handler, return FALSE so we don't get run again */
	return (_tmp3 = FALSE, (view == NULL ? NULL : (view = (g_object_unref (view), NULL))), _tmp3);
}


static void seahorse_key_manager_on_tab_changed (SeahorseKeyManager* self, GtkNotebook* notebook, void* unused, guint page_num) {
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
	gtk_entry_set_text (self->priv->_filter_entry, "");
	/* Don't track the selected key when tab is changed on purpose */
	self->priv->_track_selected_id = ((GQuark) (0));
	seahorse_key_manager_fire_selection_changed (self);
	/* 
	 * Because gnome-keyring can throw prompts etc... we delay loading 
	 * of the gnome key ring items until we first access them. 
	 */
	if (seahorse_key_manager_get_tab_id (self, seahorse_key_manager_get_tab_info (self, ((gint) (page_num)))) == SEAHORSE_KEY_MANAGER_TABS_PASSWORD) {
		seahorse_key_manager_load_gnome_keyring_items (self);
	}
}


static void seahorse_key_manager_load_gnome_keyring_items (SeahorseKeyManager* self) {
	GType type;
	SeahorseSource* sksrc;
	SeahorseOperation* op;
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	if (self->priv->_loaded_gnome_keyring) {
		return;
	}
	type = seahorse_registry_find_type (seahorse_registry_get (), "gnome-keyring", "local", "key-source", NULL, NULL);
	g_return_if_fail (type != 0);
	sksrc = SEAHORSE_SOURCE (g_object_new (type, NULL, NULL));
	seahorse_context_add_source (seahorse_context_for_app (), sksrc);
	op = seahorse_source_load (sksrc, ((GQuark) (0)));
	/* Monitor loading progress */
	seahorse_progress_status_set_operation (SEAHORSE_WIDGET (self), op);
	(sksrc == NULL ? NULL : (sksrc = (g_object_unref (sksrc), NULL)));
	(op == NULL ? NULL : (op = (g_object_unref (op), NULL)));
}


static SeahorseObject* seahorse_key_manager_real_get_selected (SeahorseKeyManager* self) {
	SeahorseKeyManagerTabInfo* tab;
	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), NULL);
	tab = seahorse_key_manager_get_tab_info (self, -1);
	if (tab == NULL) {
		return NULL;
	}
	return SEAHORSE_OBJECT (seahorse_key_manager_store_get_selected_key ((*tab).view, NULL));
}


static void seahorse_key_manager_real_set_selected (SeahorseKeyManager* self, SeahorseObject* value) {
	GList* objects;
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	objects = NULL;
	objects = g_list_prepend (objects, value);
	seahorse_viewer_set_selected_objects (SEAHORSE_VIEWER (self), objects);
	(objects == NULL ? NULL : (objects = (g_list_free (objects), NULL)));
}


static void _seahorse_key_manager_on_app_quit_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_key_manager_on_app_quit (self, _sender);
}


static void _seahorse_key_manager_on_key_generate_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_key_manager_on_key_generate (self, _sender);
}


static void _seahorse_key_manager_on_key_import_file_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_key_manager_on_key_import_file (self, _sender);
}


static void _seahorse_key_manager_on_key_import_clipboard_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_key_manager_on_key_import_clipboard (self, _sender);
}


static void _seahorse_key_manager_on_remote_find_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_key_manager_on_remote_find (self, _sender);
}


static void _seahorse_key_manager_on_remote_sync_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_key_manager_on_remote_sync (self, _sender);
}


static void _seahorse_key_manager_on_view_type_activate_gtk_action_activate (GtkToggleAction* _sender, gpointer self) {
	seahorse_key_manager_on_view_type_activate (self, _sender);
}


static void _seahorse_key_manager_on_view_expires_activate_gtk_action_activate (GtkToggleAction* _sender, gpointer self) {
	seahorse_key_manager_on_view_expires_activate (self, _sender);
}


static void _seahorse_key_manager_on_view_trust_activate_gtk_action_activate (GtkToggleAction* _sender, gpointer self) {
	seahorse_key_manager_on_view_trust_activate (self, _sender);
}


static void _seahorse_key_manager_on_view_validity_activate_gtk_action_activate (GtkToggleAction* _sender, gpointer self) {
	seahorse_key_manager_on_view_validity_activate (self, _sender);
}


static void _seahorse_key_manager_on_gconf_notify_gconf_client_notify_func (GConfClient* client, guint cnxn_id, GConfEntry* entry, gpointer self) {
	seahorse_key_manager_on_gconf_notify (self, client, cnxn_id, entry);
}


static gboolean _seahorse_key_manager_on_delete_event_gtk_widget_delete_event (GtkWidget* _sender, GdkEvent* event, gpointer self) {
	return seahorse_key_manager_on_delete_event (self, _sender, event);
}


static void _seahorse_key_manager_on_import_button_clicked_gtk_button_clicked (GtkButton* _sender, gpointer self) {
	seahorse_key_manager_on_import_button_clicked (self, _sender);
}


static void _seahorse_key_manager_on_new_button_clicked_gtk_button_clicked (GtkButton* _sender, gpointer self) {
	seahorse_key_manager_on_new_button_clicked (self, _sender);
}


static void _seahorse_key_manager_on_tab_changed_gtk_notebook_switch_page (GtkNotebook* _sender, void* page, guint page_num, gpointer self) {
	seahorse_key_manager_on_tab_changed (self, _sender, page, page_num);
}


static void _seahorse_key_manager_on_filter_changed_gtk_editable_changed (GtkEntry* _sender, gpointer self) {
	seahorse_key_manager_on_filter_changed (self, _sender);
}


static void _seahorse_key_manager_on_target_drag_data_received_gtk_widget_drag_data_received (GtkWindow* _sender, GdkDragContext* context, gint x, gint y, GtkSelectionData* selection_data, guint info, guint time_, gpointer self) {
	seahorse_key_manager_on_target_drag_data_received (self, _sender, context, x, y, selection_data, info, time_);
}


static gboolean _seahorse_key_manager_on_first_timer_gsource_func (gpointer self) {
	return seahorse_key_manager_on_first_timer (self);
}


static GObject * seahorse_key_manager_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties) {
	GObject * obj;
	SeahorseKeyManagerClass * klass;
	GObjectClass * parent_class;
	SeahorseKeyManager * self;
	klass = SEAHORSE_KEY_MANAGER_CLASS (g_type_class_peek (SEAHORSE_TYPE_KEY_MANAGER));
	parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
	obj = parent_class->constructor (type, n_construct_properties, construct_properties);
	self = SEAHORSE_KEY_MANAGER (obj);
	{
		SeahorseKeyManagerTabInfo* _tmp1;
		gint _tmp0;
		GtkNotebook* _tmp3;
		GtkNotebook* _tmp2;
		GtkActionGroup* actions;
		GtkActionGroup* _tmp4;
		GtkActionGroup* _tmp5;
		GtkToggleAction* _tmp6;
		GtkToggleAction* action;
		GtkToggleAction* _tmp8;
		GtkToggleAction* _tmp7;
		GtkToggleAction* _tmp10;
		GtkToggleAction* _tmp9;
		GtkToggleAction* _tmp12;
		GtkToggleAction* _tmp11;
		GtkWidget* _tmp13;
		GtkWidget* widget;
		GtkWidget* _tmp19;
		GtkWidget* _tmp18;
		GtkTargetEntry* _tmp21;
		gint entries_length1;
		GtkTargetEntry* _tmp20;
		GtkTargetEntry* entries;
		GtkTargetList* targets;
		self->priv->_loaded_gnome_keyring = FALSE;
		_tmp1 = NULL;
		self->priv->_tabs = (_tmp1 = g_new0 (SeahorseKeyManagerTabInfo, (_tmp0 = ((gint) (SEAHORSE_KEY_MANAGER_TABS_NUM_TABS)))), (self->priv->_tabs = (g_free (self->priv->_tabs), NULL)), self->priv->_tabs_length1 = _tmp0, _tmp1);
		_tmp3 = NULL;
		_tmp2 = NULL;
		self->priv->_notebook = (_tmp3 = (_tmp2 = GTK_NOTEBOOK (seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "notebook")), (_tmp2 == NULL ? NULL : g_object_ref (_tmp2))), (self->priv->_notebook == NULL ? NULL : (self->priv->_notebook = (g_object_unref (self->priv->_notebook), NULL))), _tmp3);
		gtk_window_set_title (seahorse_view_get_window (SEAHORSE_VIEW (self)), _ ("Passwords and Encryption Keys"));
		/* 
		 * We hook callbacks up here for now because of a compiler warning. See:
		 * http://bugzilla.gnome.org/show_bug.cgi?id=539483
		 */
		actions = gtk_action_group_new ("general");
		gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
		gtk_action_group_add_actions (actions, SEAHORSE_KEY_MANAGER_GENERAL_ENTRIES, G_N_ELEMENTS (SEAHORSE_KEY_MANAGER_GENERAL_ENTRIES), self);
		g_signal_connect_object (gtk_action_group_get_action (actions, "app-quit"), "activate", ((GCallback) (_seahorse_key_manager_on_app_quit_gtk_action_activate)), self, 0);
		g_signal_connect_object (gtk_action_group_get_action (actions, "key-generate"), "activate", ((GCallback) (_seahorse_key_manager_on_key_generate_gtk_action_activate)), self, 0);
		g_signal_connect_object (gtk_action_group_get_action (actions, "key-import-file"), "activate", ((GCallback) (_seahorse_key_manager_on_key_import_file_gtk_action_activate)), self, 0);
		g_signal_connect_object (gtk_action_group_get_action (actions, "key-import-clipboard"), "activate", ((GCallback) (_seahorse_key_manager_on_key_import_clipboard_gtk_action_activate)), self, 0);
		seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), actions);
		_tmp4 = NULL;
		actions = (_tmp4 = gtk_action_group_new ("keyserver"), (actions == NULL ? NULL : (actions = (g_object_unref (actions), NULL))), _tmp4);
		gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
		gtk_action_group_add_actions (actions, SEAHORSE_KEY_MANAGER_SERVER_ENTRIES, G_N_ELEMENTS (SEAHORSE_KEY_MANAGER_SERVER_ENTRIES), self);
		g_signal_connect_object (gtk_action_group_get_action (actions, "remote-find"), "activate", ((GCallback) (_seahorse_key_manager_on_remote_find_gtk_action_activate)), self, 0);
		g_signal_connect_object (gtk_action_group_get_action (actions, "remote-sync"), "activate", ((GCallback) (_seahorse_key_manager_on_remote_sync_gtk_action_activate)), self, 0);
		seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), actions);
		_tmp5 = NULL;
		self->priv->_view_actions = (_tmp5 = gtk_action_group_new ("view"), (self->priv->_view_actions == NULL ? NULL : (self->priv->_view_actions = (g_object_unref (self->priv->_view_actions), NULL))), _tmp5);
		gtk_action_group_set_translation_domain (self->priv->_view_actions, GETTEXT_PACKAGE);
		gtk_action_group_add_toggle_actions (self->priv->_view_actions, SEAHORSE_KEY_MANAGER_VIEW_ENTRIES, G_N_ELEMENTS (SEAHORSE_KEY_MANAGER_VIEW_ENTRIES), self);
		_tmp6 = NULL;
		action = (_tmp6 = GTK_TOGGLE_ACTION (gtk_action_group_get_action (self->priv->_view_actions, "view-type")), (_tmp6 == NULL ? NULL : g_object_ref (_tmp6)));
		gtk_toggle_action_set_active (action, seahorse_gconf_get_boolean (SHOW_TYPE_KEY));
		g_signal_connect_object (GTK_ACTION (action), "activate", ((GCallback) (_seahorse_key_manager_on_view_type_activate_gtk_action_activate)), self, 0);
		_tmp8 = NULL;
		_tmp7 = NULL;
		action = (_tmp8 = (_tmp7 = GTK_TOGGLE_ACTION (gtk_action_group_get_action (self->priv->_view_actions, "view-expires")), (_tmp7 == NULL ? NULL : g_object_ref (_tmp7))), (action == NULL ? NULL : (action = (g_object_unref (action), NULL))), _tmp8);
		gtk_toggle_action_set_active (action, seahorse_gconf_get_boolean (SHOW_EXPIRES_KEY));
		g_signal_connect_object (GTK_ACTION (action), "activate", ((GCallback) (_seahorse_key_manager_on_view_expires_activate_gtk_action_activate)), self, 0);
		_tmp10 = NULL;
		_tmp9 = NULL;
		action = (_tmp10 = (_tmp9 = GTK_TOGGLE_ACTION (gtk_action_group_get_action (self->priv->_view_actions, "view-trust")), (_tmp9 == NULL ? NULL : g_object_ref (_tmp9))), (action == NULL ? NULL : (action = (g_object_unref (action), NULL))), _tmp10);
		gtk_toggle_action_set_active (action, seahorse_gconf_get_boolean (SHOW_TRUST_KEY));
		g_signal_connect_object (GTK_ACTION (action), "activate", ((GCallback) (_seahorse_key_manager_on_view_trust_activate_gtk_action_activate)), self, 0);
		_tmp12 = NULL;
		_tmp11 = NULL;
		action = (_tmp12 = (_tmp11 = GTK_TOGGLE_ACTION (gtk_action_group_get_action (self->priv->_view_actions, "view-validity")), (_tmp11 == NULL ? NULL : g_object_ref (_tmp11))), (action == NULL ? NULL : (action = (g_object_unref (action), NULL))), _tmp12);
		gtk_toggle_action_set_active (action, seahorse_gconf_get_boolean (SHOW_VALIDITY_KEY));
		g_signal_connect_object (GTK_ACTION (action), "activate", ((GCallback) (_seahorse_key_manager_on_view_validity_activate_gtk_action_activate)), self, 0);
		seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), self->priv->_view_actions);
		/* Notify us when gconf stuff changes under this key */
		seahorse_gconf_notify_lazy (LISTING_SCHEMAS, _seahorse_key_manager_on_gconf_notify_gconf_client_notify_func, self, GTK_OBJECT (self));
		/* close event */
		g_signal_connect_object (seahorse_widget_get_toplevel (SEAHORSE_WIDGET (self)), "delete-event", ((GCallback) (_seahorse_key_manager_on_delete_event_gtk_widget_delete_event)), self, 0);
		/* first time signals */
		g_signal_connect_object ((GTK_BUTTON (seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "import-button"))), "clicked", ((GCallback) (_seahorse_key_manager_on_import_button_clicked_gtk_button_clicked)), self, 0);
		g_signal_connect_object ((GTK_BUTTON (seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "new-button"))), "clicked", ((GCallback) (_seahorse_key_manager_on_new_button_clicked_gtk_button_clicked)), self, 0);
		/* The notebook */
		g_signal_connect_object (self->priv->_notebook, "switch-page", ((GCallback) (_seahorse_key_manager_on_tab_changed_gtk_notebook_switch_page)), self, 0);
		/* Flush all updates */
		seahorse_viewer_ensure_updated (SEAHORSE_VIEWER (self));
		/* Find the toolbar */
		_tmp13 = NULL;
		widget = (_tmp13 = seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "toolbar-placeholder"), (_tmp13 == NULL ? NULL : g_object_ref (_tmp13)));
		if (widget != NULL) {
			GList* children;
			children = gtk_container_get_children ((GTK_CONTAINER (widget)));
			if (children != NULL && ((SeahorseWidget*) (children->data)) != NULL) {
				GtkToolbar* _tmp14;
				GtkToolbar* toolbar;
				/* The toolbar is the first (and only) element */
				_tmp14 = NULL;
				toolbar = (_tmp14 = GTK_TOOLBAR (((SeahorseWidget*) (((SeahorseWidget*) (children->data))))), (_tmp14 == NULL ? NULL : g_object_ref (_tmp14)));
				if (toolbar != NULL && G_TYPE_FROM_INSTANCE (G_OBJECT (toolbar)) == GTK_TYPE_TOOLBAR) {
					GtkSeparatorToolItem* sep;
					GtkBox* box;
					GtkLabel* _tmp15;
					GtkEntry* _tmp16;
					GtkLabel* _tmp17;
					GtkToolItem* item;
					/* Insert a separator to right align the filter */
					sep = g_object_ref_sink (gtk_separator_tool_item_new ());
					gtk_separator_tool_item_set_draw (sep, FALSE);
					gtk_tool_item_set_expand (GTK_TOOL_ITEM (sep), TRUE);
					gtk_widget_show_all (GTK_WIDGET (sep));
					gtk_toolbar_insert (toolbar, GTK_TOOL_ITEM (sep), -1);
					/* Insert a filter bar */
					box = GTK_BOX (g_object_ref_sink (gtk_hbox_new (FALSE, 0)));
					_tmp15 = NULL;
					gtk_box_pack_start (box, GTK_WIDGET ((_tmp15 = g_object_ref_sink (gtk_label_new (_ ("Filter:"))))), FALSE, TRUE, ((guint) (3)));
					(_tmp15 == NULL ? NULL : (_tmp15 = (g_object_unref (_tmp15), NULL)));
					_tmp16 = NULL;
					self->priv->_filter_entry = (_tmp16 = g_object_ref_sink (gtk_entry_new ()), (self->priv->_filter_entry == NULL ? NULL : (self->priv->_filter_entry = (g_object_unref (self->priv->_filter_entry), NULL))), _tmp16);
					gtk_box_pack_start (box, GTK_WIDGET (self->priv->_filter_entry), FALSE, TRUE, ((guint) (0)));
					_tmp17 = NULL;
					gtk_box_pack_start (box, GTK_WIDGET ((_tmp17 = g_object_ref_sink (gtk_label_new (NULL)))), FALSE, FALSE, ((guint) (0)));
					(_tmp17 == NULL ? NULL : (_tmp17 = (g_object_unref (_tmp17), NULL)));
					gtk_widget_show_all (GTK_WIDGET (box));
					item = g_object_ref_sink (gtk_tool_item_new ());
					gtk_container_add (GTK_CONTAINER (item), GTK_WIDGET (box));
					gtk_widget_show_all (GTK_WIDGET (item));
					gtk_toolbar_insert (toolbar, item, -1);
					(sep == NULL ? NULL : (sep = (g_object_unref (sep), NULL)));
					(box == NULL ? NULL : (box = (g_object_unref (box), NULL)));
					(item == NULL ? NULL : (item = (g_object_unref (item), NULL)));
				}
				(toolbar == NULL ? NULL : (toolbar = (g_object_unref (toolbar), NULL)));
			}
		}
		/* For the filtering */
		g_signal_connect_object (GTK_EDITABLE (self->priv->_filter_entry), "changed", ((GCallback) (_seahorse_key_manager_on_filter_changed_gtk_editable_changed)), self, 0);
		/* Initialize the tabs, and associate them up */
		seahorse_key_manager_initialize_tab (self, "pub-key-tab", ((guint) (SEAHORSE_KEY_MANAGER_TABS_PUBLIC)), "pub-key-list", &SEAHORSE_KEY_MANAGER_PRED_PUBLIC);
		seahorse_key_manager_initialize_tab (self, "trust-key-tab", ((guint) (SEAHORSE_KEY_MANAGER_TABS_TRUSTED)), "trust-key-list", &SEAHORSE_KEY_MANAGER_PRED_TRUSTED);
		seahorse_key_manager_initialize_tab (self, "sec-key-tab", ((guint) (SEAHORSE_KEY_MANAGER_TABS_PRIVATE)), "sec-key-list", &SEAHORSE_KEY_MANAGER_PRED_PRIVATE);
		seahorse_key_manager_initialize_tab (self, "password-tab", ((guint) (SEAHORSE_KEY_MANAGER_TABS_PASSWORD)), "password-list", &SEAHORSE_KEY_MANAGER_PRED_PASSWORD);
		/* Set focus to the current key list */
		_tmp19 = NULL;
		_tmp18 = NULL;
		widget = (_tmp19 = (_tmp18 = GTK_WIDGET (seahorse_key_manager_get_current_view (self)), (_tmp18 == NULL ? NULL : g_object_ref (_tmp18))), (widget == NULL ? NULL : (widget = (g_object_unref (widget), NULL))), _tmp19);
		gtk_widget_grab_focus (widget);
		/* To avoid flicker */
		seahorse_widget_show (SEAHORSE_WIDGET (SEAHORSE_VIEWER (self)));
		/* Setup drops */
		_tmp21 = NULL;
		_tmp20 = NULL;
		entries = (_tmp21 = (_tmp20 = g_new0 (GtkTargetEntry, 0), _tmp20), entries_length1 = 0, _tmp21);
		gtk_drag_dest_set (GTK_WIDGET (seahorse_view_get_window (SEAHORSE_VIEW (self))), GTK_DEST_DEFAULT_ALL, entries, entries_length1, GDK_ACTION_COPY);
		targets = gtk_target_list_new (entries, ((guint) (0)));
		gtk_target_list_add_uri_targets (targets, ((guint) (SEAHORSE_KEY_MANAGER_TARGETS_URIS)));
		gtk_target_list_add_text_targets (targets, ((guint) (SEAHORSE_KEY_MANAGER_TARGETS_PLAIN)));
		gtk_drag_dest_set_target_list (GTK_WIDGET (seahorse_view_get_window (SEAHORSE_VIEW (self))), targets);
		g_signal_connect_object (GTK_WIDGET (seahorse_view_get_window (SEAHORSE_VIEW (self))), "drag-data-received", ((GCallback) (_seahorse_key_manager_on_target_drag_data_received_gtk_widget_drag_data_received)), self, 0);
		/* To show first time dialog */
		g_timeout_add (((guint) (1000)), _seahorse_key_manager_on_first_timer_gsource_func, self);
		g_signal_emit_by_name (G_OBJECT (SEAHORSE_VIEW (self)), "selection-changed");
		(actions == NULL ? NULL : (actions = (g_object_unref (actions), NULL)));
		(action == NULL ? NULL : (action = (g_object_unref (action), NULL)));
		(widget == NULL ? NULL : (widget = (g_object_unref (widget), NULL)));
		entries = (g_free (entries), NULL);
		(targets == NULL ? NULL : (targets = (gtk_target_list_unref (targets), NULL)));
	}
	return obj;
}


static void seahorse_key_manager_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SeahorseKeyManager * self;
	self = SEAHORSE_KEY_MANAGER (object);
	switch (property_id) {
		case SEAHORSE_KEY_MANAGER_SELECTED:
		g_value_set_object (value, seahorse_key_manager_real_get_selected (self));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_key_manager_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
	SeahorseKeyManager * self;
	self = SEAHORSE_KEY_MANAGER (object);
	switch (property_id) {
		case SEAHORSE_KEY_MANAGER_SELECTED:
		seahorse_key_manager_real_set_selected (self, g_value_get_object (value));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_key_manager_class_init (SeahorseKeyManagerClass * klass) {
	seahorse_key_manager_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseKeyManagerPrivate));
	G_OBJECT_CLASS (klass)->get_property = seahorse_key_manager_get_property;
	G_OBJECT_CLASS (klass)->set_property = seahorse_key_manager_set_property;
	G_OBJECT_CLASS (klass)->constructor = seahorse_key_manager_constructor;
	G_OBJECT_CLASS (klass)->dispose = seahorse_key_manager_dispose;
	SEAHORSE_VIEWER_CLASS (klass)->get_selected_objects = seahorse_key_manager_real_get_selected_objects;
	SEAHORSE_VIEWER_CLASS (klass)->set_selected_objects = seahorse_key_manager_real_set_selected_objects;
	SEAHORSE_VIEWER_CLASS (klass)->get_selected_object_and_uid = seahorse_key_manager_real_get_selected_object_and_uid;
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_KEY_MANAGER_SELECTED, "selected");
}


static void seahorse_key_manager_seahorse_view_interface_init (SeahorseViewIface * iface) {
	seahorse_key_manager_seahorse_view_parent_iface = g_type_interface_peek_parent (iface);
	iface->get_selected_objects = seahorse_key_manager_real_get_selected_objects;
	iface->set_selected_objects = seahorse_key_manager_real_set_selected_objects;
	iface->get_selected_object_and_uid = seahorse_key_manager_real_get_selected_object_and_uid;
}


static void seahorse_key_manager_instance_init (SeahorseKeyManager * self) {
	self->priv = SEAHORSE_KEY_MANAGER_GET_PRIVATE (self);
}


static void seahorse_key_manager_dispose (GObject * obj) {
	SeahorseKeyManager * self;
	self = SEAHORSE_KEY_MANAGER (obj);
	(self->priv->_notebook == NULL ? NULL : (self->priv->_notebook = (g_object_unref (self->priv->_notebook), NULL)));
	(self->priv->_view_actions == NULL ? NULL : (self->priv->_view_actions = (g_object_unref (self->priv->_view_actions), NULL)));
	(self->priv->_filter_entry == NULL ? NULL : (self->priv->_filter_entry = (g_object_unref (self->priv->_filter_entry), NULL)));
	self->priv->_tabs = (g_free (self->priv->_tabs), NULL);
	G_OBJECT_CLASS (seahorse_key_manager_parent_class)->dispose (obj);
}


GType seahorse_key_manager_get_type (void) {
	static GType seahorse_key_manager_type_id = 0;
	if (G_UNLIKELY (seahorse_key_manager_type_id == 0)) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorseKeyManagerClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_key_manager_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorseKeyManager), 0, (GInstanceInitFunc) seahorse_key_manager_instance_init };
		static const GInterfaceInfo seahorse_view_info = { (GInterfaceInitFunc) seahorse_key_manager_seahorse_view_interface_init, (GInterfaceFinalizeFunc) NULL, NULL};
		seahorse_key_manager_type_id = g_type_register_static (SEAHORSE_TYPE_VIEWER, "SeahorseKeyManager", &g_define_type_info, 0);
		g_type_add_interface_static (seahorse_key_manager_type_id, SEAHORSE_TYPE_VIEW, &seahorse_view_info);
	}
	return seahorse_key_manager_type_id;
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


static int _vala_strcmp0 (const char * str1, const char * str2) {
	if (str1 == NULL) {
		return -(str1 != str2);
	}
	if (str2 == NULL) {
		return (str1 != str2);
	}
	return strcmp (str1, str2);
}




