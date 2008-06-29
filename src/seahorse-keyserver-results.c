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

#include "seahorse-keyserver-results.h"
#include <seahorse-key-manager-store.h>
#include <seahorse-keyset.h>
#include <glib/gi18n-lib.h>
#include <seahorse-widget.h>
#include <seahorse-progress.h>
#include <gdk/gdk.h>
#include <seahorse-context.h>
#include <seahorse-key-source.h>
#include <seahorse-windows.h>
#include <config.h>




struct _SeahorseKeyserverResultsPrivate {
	char* _search_string;
	GtkTreeView* _view;
	GtkActionGroup* _key_actions;
	SeahorseKeyManagerStore* _store;
	SeahorseKeyset* _keyset;
	SeahorseKeyPredicate _pred;
};

#define SEAHORSE_KEYSERVER_RESULTS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_TYPE_KEYSERVER_RESULTS, SeahorseKeyserverResultsPrivate))
enum  {
	SEAHORSE_KEYSERVER_RESULTS_DUMMY_PROPERTY,
	SEAHORSE_KEYSERVER_RESULTS_SEARCH,
	SEAHORSE_KEYSERVER_RESULTS_SELECTED_KEY
};
static SeahorseKeyserverResults* seahorse_keyserver_results_new (const char* search_text);
static GList* seahorse_keyserver_results_real_get_selected_keys (SeahorseView* base);
static void seahorse_keyserver_results_real_set_selected_keys (SeahorseView* base, GList* keys);
static SeahorseKey* seahorse_keyserver_results_real_get_selected_key_and_uid (SeahorseView* base, guint* uid);
static gboolean seahorse_keyserver_results_on_filter_keyset (SeahorseKeyserverResults* self, SeahorseKey* key);
static gboolean _seahorse_keyserver_results_fire_selection_changed_gsource_func (gpointer self);
static void seahorse_keyserver_results_on_view_selection_changed (SeahorseKeyserverResults* self, GtkTreeSelection* selection);
static void seahorse_keyserver_results_on_row_activated (SeahorseKeyserverResults* self, GtkTreeView* view, GtkTreePath* path, GtkTreeViewColumn* column);
static gboolean seahorse_keyserver_results_on_key_list_button_pressed (SeahorseKeyserverResults* self, GtkWidget* widget, GdkEventButton* event);
static gboolean seahorse_keyserver_results_on_key_list_popup_menu (SeahorseKeyserverResults* self, GtkTreeView* view);
static void seahorse_keyserver_results_on_view_expand_all (SeahorseKeyserverResults* self, GtkAction* action);
static void seahorse_keyserver_results_on_view_collapse_all (SeahorseKeyserverResults* self, GtkAction* action);
static void seahorse_keyserver_results_on_app_close (SeahorseKeyserverResults* self, GtkAction* action);
static void seahorse_keyserver_results_imported_keys (SeahorseKeyserverResults* self, SeahorseOperation* op);
static void _seahorse_keyserver_results_imported_keys_seahorse_done_func (SeahorseOperation* op, gpointer self);
static void seahorse_keyserver_results_on_key_import_keyring (SeahorseKeyserverResults* self, GtkAction* action);
static void seahorse_keyserver_results_on_remote_find (SeahorseKeyserverResults* self, GtkAction* action);
static gboolean seahorse_keyserver_results_on_delete_event (SeahorseKeyserverResults* self, GtkWidget* widget, GdkEvent* event);
static gboolean seahorse_keyserver_results_fire_selection_changed (SeahorseKeyserverResults* self);
static void seahorse_keyserver_results_set_search (SeahorseKeyserverResults* self, const char* value);
static void _seahorse_keyserver_results_on_app_close_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_keyserver_results_on_view_expand_all_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_keyserver_results_on_view_collapse_all_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_keyserver_results_on_remote_find_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_keyserver_results_on_key_import_keyring_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_keyserver_results_on_view_selection_changed_gtk_tree_selection_changed (GtkTreeSelection* _sender, gpointer self);
static void _seahorse_keyserver_results_on_row_activated_gtk_tree_view_row_activated (GtkTreeView* _sender, GtkTreePath* path, GtkTreeViewColumn* column, gpointer self);
static gboolean _seahorse_keyserver_results_on_key_list_button_pressed_gtk_widget_button_press_event (GtkTreeView* _sender, GdkEventButton* event, gpointer self);
static gboolean _seahorse_keyserver_results_on_key_list_popup_menu_gtk_widget_popup_menu (GtkTreeView* _sender, gpointer self);
static gboolean _seahorse_keyserver_results_on_filter_keyset_seahorse_key_predicate_func (SeahorseKey* key, gpointer self);
static GObject * seahorse_keyserver_results_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties);
static gpointer seahorse_keyserver_results_parent_class = NULL;
static SeahorseViewIface* seahorse_keyserver_results_seahorse_view_parent_iface = NULL;
static void seahorse_keyserver_results_dispose (GObject * obj);

static const GtkActionEntry SEAHORSE_KEYSERVER_RESULTS_GENERAL_ENTRIES[] = {{"remote-menu", NULL, N_ ("_Remote")}, {"app-close", GTK_STOCK_CLOSE, N_ ("_Close"), "<control>W", N_ ("Close this window"), ((GCallback) (NULL))}, {"view-expand-all", GTK_STOCK_ADD, N_ ("_Expand All"), NULL, N_ ("Expand all listings"), ((GCallback) (NULL))}, {"view-collapse-all", GTK_STOCK_REMOVE, N_ ("_Collapse All"), NULL, N_ ("Collapse all listings"), ((GCallback) (NULL))}};
static const GtkActionEntry SEAHORSE_KEYSERVER_RESULTS_SERVER_ENTRIES[] = {{"remote-find", GTK_STOCK_FIND, N_ ("_Find Remote Keys..."), "", N_ ("Search for keys on a key server"), ((GCallback) (NULL))}};
static const GtkActionEntry SEAHORSE_KEYSERVER_RESULTS_KEY_ENTRIES[] = {{"key-import-keyring", GTK_STOCK_ADD, N_ ("_Import"), "", N_ ("Import selected keys to local key ring"), ((GCallback) (NULL))}};


static SeahorseKeyserverResults* seahorse_keyserver_results_new (const char* search_text) {
	GParameter * __params;
	GParameter * __params_it;
	SeahorseKeyserverResults * self;
	g_return_val_if_fail (search_text != NULL, NULL);
	__params = g_new0 (GParameter, 2);
	__params_it = __params;
	__params_it->name = "name";
	g_value_init (&__params_it->value, G_TYPE_STRING);
	g_value_set_string (&__params_it->value, "keyserver-results");
	__params_it++;
	__params_it->name = "search";
	g_value_init (&__params_it->value, G_TYPE_STRING);
	g_value_set_string (&__params_it->value, search_text);
	__params_it++;
	self = g_object_newv (SEAHORSE_TYPE_KEYSERVER_RESULTS, __params_it - __params, __params);
	while (__params_it > __params) {
		--__params_it;
		g_value_unset (&__params_it->value);
	}
	g_free (__params);
	return self;
}


void seahorse_keyserver_results_show (SeahorseOperation* op, GtkWindow* parent, const char* search_text) {
	SeahorseKeyserverResults* res;
	g_return_if_fail (SEAHORSE_IS_OPERATION (op));
	g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));
	g_return_if_fail (search_text != NULL);
	res = g_object_ref_sink (seahorse_keyserver_results_new (search_text));
	g_object_ref (G_OBJECT (res));
	/* Destorys itself with destroy */
	if (parent != NULL) {
		GtkWindow* _tmp0;
		GtkWindow* window;
		_tmp0 = NULL;
		window = (_tmp0 = GTK_WINDOW (seahorse_widget_get_toplevel (SEAHORSE_WIDGET (res))), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
		gtk_window_set_transient_for (window, parent);
		(window == NULL ? NULL : (window = (g_object_unref (window), NULL)));
	}
	seahorse_progress_status_set_operation (SEAHORSE_WIDGET (res), op);
	(res == NULL ? NULL : (res = (g_object_unref (res), NULL)));
}


static GList* seahorse_keyserver_results_real_get_selected_keys (SeahorseView* base) {
	SeahorseKeyserverResults * self;
	self = SEAHORSE_KEYSERVER_RESULTS (base);
	return seahorse_key_manager_store_get_selected_keys (self->priv->_view);
}


static void seahorse_keyserver_results_real_set_selected_keys (SeahorseView* base, GList* keys) {
	SeahorseKeyserverResults * self;
	self = SEAHORSE_KEYSERVER_RESULTS (base);
	g_return_if_fail (keys != NULL);
	seahorse_key_manager_store_set_selected_keys (self->priv->_view, keys);
}


static SeahorseKey* seahorse_keyserver_results_real_get_selected_key_and_uid (SeahorseView* base, guint* uid) {
	SeahorseKeyserverResults * self;
	self = SEAHORSE_KEYSERVER_RESULTS (base);
	return seahorse_key_manager_store_get_selected_key (self->priv->_view, &(*uid));
}


static gboolean seahorse_keyserver_results_on_filter_keyset (SeahorseKeyserverResults* self, SeahorseKey* key) {
	const char* _tmp1;
	char* name;
	char* _tmp2;
	gboolean _tmp3;
	gboolean _tmp4;
	g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), FALSE);
	g_return_val_if_fail (SEAHORSE_IS_KEY (key), FALSE);
	if (g_utf8_strlen (self->priv->_search_string, -1) == 0) {
		return TRUE;
	}
	_tmp1 = NULL;
	name = (_tmp1 = seahorse_key_get_display_name (key), (_tmp1 == NULL ? NULL : g_strdup (_tmp1)));
	_tmp2 = NULL;
	return (_tmp4 = (_tmp3 = (strstr ((_tmp2 = g_utf8_casefold (name, -1)), self->priv->_search_string) != NULL), (_tmp2 = (g_free (_tmp2), NULL)), _tmp3), (name = (g_free (name), NULL)), _tmp4);
}


static gboolean _seahorse_keyserver_results_fire_selection_changed_gsource_func (gpointer self) {
	return seahorse_keyserver_results_fire_selection_changed (self);
}


static void seahorse_keyserver_results_on_view_selection_changed (SeahorseKeyserverResults* self, GtkTreeSelection* selection) {
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_idle_add (_seahorse_keyserver_results_fire_selection_changed_gsource_func, self);
}


static void seahorse_keyserver_results_on_row_activated (SeahorseKeyserverResults* self, GtkTreeView* view, GtkTreePath* path, GtkTreeViewColumn* column) {
	SeahorseKey* _tmp0;
	SeahorseKey* key;
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (GTK_IS_TREE_VIEW (view));
	g_return_if_fail (path != NULL);
	g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (column));
	_tmp0 = NULL;
	key = (_tmp0 = seahorse_key_manager_store_get_key_from_path (view, path, NULL), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	if (key != NULL) {
		seahorse_viewer_show_properties (SEAHORSE_VIEWER (self), key);
	}
	(key == NULL ? NULL : (key = (g_object_unref (key), NULL)));
}


static gboolean seahorse_keyserver_results_on_key_list_button_pressed (SeahorseKeyserverResults* self, GtkWidget* widget, GdkEventButton* event) {
	g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	if ((*event).button == 3) {
		seahorse_viewer_show_context_menu (SEAHORSE_VIEWER (self), (*event).button, (*event).time);
	}
	return FALSE;
}


static gboolean seahorse_keyserver_results_on_key_list_popup_menu (SeahorseKeyserverResults* self, GtkTreeView* view) {
	SeahorseKey* _tmp0;
	SeahorseKey* key;
	gboolean _tmp2;
	g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), FALSE);
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), FALSE);
	_tmp0 = NULL;
	key = (_tmp0 = seahorse_viewer_get_selected_key (SEAHORSE_VIEWER (self)), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	if (key == NULL) {
		gboolean _tmp1;
		return (_tmp1 = FALSE, (key == NULL ? NULL : (key = (g_object_unref (key), NULL))), _tmp1);
	}
	seahorse_viewer_show_context_menu (SEAHORSE_VIEWER (self), ((guint) (0)), gtk_get_current_event_time ());
	return (_tmp2 = TRUE, (key == NULL ? NULL : (key = (g_object_unref (key), NULL))), _tmp2);
}


static void seahorse_keyserver_results_on_view_expand_all (SeahorseKeyserverResults* self, GtkAction* action) {
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	gtk_tree_view_expand_all (self->priv->_view);
}


static void seahorse_keyserver_results_on_view_collapse_all (SeahorseKeyserverResults* self, GtkAction* action) {
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	gtk_tree_view_collapse_all (self->priv->_view);
}


static void seahorse_keyserver_results_on_app_close (SeahorseKeyserverResults* self, GtkAction* action) {
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (action == NULL || GTK_IS_ACTION (action));
	seahorse_widget_destroy (SEAHORSE_WIDGET (self));
}


static void seahorse_keyserver_results_imported_keys (SeahorseKeyserverResults* self, SeahorseOperation* op) {
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (SEAHORSE_IS_OPERATION (op));
	if (!seahorse_operation_is_successful (op)) {
		seahorse_operation_display_error (op, _ ("Couldn't import keys"), GTK_WIDGET (seahorse_view_get_window (SEAHORSE_VIEW (self))));
		return;
	}
	seahorse_viewer_set_status (SEAHORSE_VIEWER (self), _ ("Imported keys"));
}


static void _seahorse_keyserver_results_imported_keys_seahorse_done_func (SeahorseOperation* op, gpointer self) {
	seahorse_keyserver_results_imported_keys (self, op);
}


static void seahorse_keyserver_results_on_key_import_keyring (SeahorseKeyserverResults* self, GtkAction* action) {
	GList* keys;
	SeahorseOperation* op;
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	keys = seahorse_viewer_get_selected_keys (SEAHORSE_VIEWER (self));
	/* No keys, nothing to do */
	if (keys == NULL) {
		(keys == NULL ? NULL : (keys = (g_list_free (keys), NULL)));
		return;
	}
	op = seahorse_context_transfer_keys (seahorse_context_for_app (), keys, NULL);
	seahorse_progress_show (op, _ ("Importing keys from key servers"), TRUE);
	seahorse_operation_watch (op, _seahorse_keyserver_results_imported_keys_seahorse_done_func, self, NULL, NULL);
	(keys == NULL ? NULL : (keys = (g_list_free (keys), NULL)));
	(op == NULL ? NULL : (op = (g_object_unref (op), NULL)));
}


static void seahorse_keyserver_results_on_remote_find (SeahorseKeyserverResults* self, GtkAction* action) {
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	seahorse_keyserver_search_show (seahorse_view_get_window (SEAHORSE_VIEW (self)));
}


/* When this window closes we quit seahorse */
static gboolean seahorse_keyserver_results_on_delete_event (SeahorseKeyserverResults* self, GtkWidget* widget, GdkEvent* event) {
	g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	seahorse_keyserver_results_on_app_close (self, NULL);
	return TRUE;
}


static gboolean seahorse_keyserver_results_fire_selection_changed (SeahorseKeyserverResults* self) {
	gint rows;
	GtkTreeSelection* _tmp0;
	GtkTreeSelection* selection;
	gboolean _tmp1;
	g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), FALSE);
	rows = 0;
	_tmp0 = NULL;
	selection = (_tmp0 = gtk_tree_view_get_selection (self->priv->_view), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	rows = gtk_tree_selection_count_selected_rows (selection);
	seahorse_viewer_set_numbered_status (SEAHORSE_VIEWER (self), ngettext ("Selected %d key", "Selected %d keys", rows), rows);
	gtk_action_group_set_sensitive (self->priv->_key_actions, rows > 0);
	g_signal_emit_by_name (G_OBJECT (SEAHORSE_VIEW (self)), "selection-changed");
	return (_tmp1 = FALSE, (selection == NULL ? NULL : (selection = (g_object_unref (selection), NULL))), _tmp1);
}


const char* seahorse_keyserver_results_get_search (SeahorseKeyserverResults* self) {
	g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), NULL);
	return self->priv->_search_string;
}


static void seahorse_keyserver_results_set_search (SeahorseKeyserverResults* self, const char* value) {
	const char* _tmp1;
	char* str;
	char* _tmp2;
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	/* Many key servers ignore spaces at the beginning and end, so we do too */
	_tmp1 = NULL;
	str = (_tmp1 = value, (_tmp1 == NULL ? NULL : g_strdup (_tmp1)));
	_tmp2 = NULL;
	self->priv->_search_string = (_tmp2 = g_utf8_casefold (g_strstrip (str), -1), (self->priv->_search_string = (g_free (self->priv->_search_string), NULL)), _tmp2);
	str = (g_free (str), NULL);
}


static SeahorseKey* seahorse_keyserver_results_real_get_selected_key (SeahorseKeyserverResults* self) {
	g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), NULL);
	return seahorse_key_manager_store_get_selected_key (self->priv->_view, NULL);
}


static void seahorse_keyserver_results_real_set_selected_key (SeahorseKeyserverResults* self, SeahorseKey* value) {
	GList* keys;
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	keys = NULL;
	keys = g_list_prepend (keys, value);
	seahorse_viewer_set_selected_keys (SEAHORSE_VIEWER (self), keys);
	(keys == NULL ? NULL : (keys = (g_list_free (keys), NULL)));
}


static void _seahorse_keyserver_results_on_app_close_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_keyserver_results_on_app_close (self, _sender);
}


static void _seahorse_keyserver_results_on_view_expand_all_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_keyserver_results_on_view_expand_all (self, _sender);
}


static void _seahorse_keyserver_results_on_view_collapse_all_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_keyserver_results_on_view_collapse_all (self, _sender);
}


static void _seahorse_keyserver_results_on_remote_find_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_keyserver_results_on_remote_find (self, _sender);
}


static void _seahorse_keyserver_results_on_key_import_keyring_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_keyserver_results_on_key_import_keyring (self, _sender);
}


static void _seahorse_keyserver_results_on_view_selection_changed_gtk_tree_selection_changed (GtkTreeSelection* _sender, gpointer self) {
	seahorse_keyserver_results_on_view_selection_changed (self, _sender);
}


static void _seahorse_keyserver_results_on_row_activated_gtk_tree_view_row_activated (GtkTreeView* _sender, GtkTreePath* path, GtkTreeViewColumn* column, gpointer self) {
	seahorse_keyserver_results_on_row_activated (self, _sender, path, column);
}


static gboolean _seahorse_keyserver_results_on_key_list_button_pressed_gtk_widget_button_press_event (GtkTreeView* _sender, GdkEventButton* event, gpointer self) {
	return seahorse_keyserver_results_on_key_list_button_pressed (self, _sender, event);
}


static gboolean _seahorse_keyserver_results_on_key_list_popup_menu_gtk_widget_popup_menu (GtkTreeView* _sender, gpointer self) {
	return seahorse_keyserver_results_on_key_list_popup_menu (self, _sender);
}


static gboolean _seahorse_keyserver_results_on_filter_keyset_seahorse_key_predicate_func (SeahorseKey* key, gpointer self) {
	return seahorse_keyserver_results_on_filter_keyset (self, key);
}


static GObject * seahorse_keyserver_results_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties) {
	GObject * obj;
	SeahorseKeyserverResultsClass * klass;
	GObjectClass * parent_class;
	SeahorseKeyserverResults * self;
	klass = SEAHORSE_KEYSERVER_RESULTS_CLASS (g_type_class_peek (SEAHORSE_TYPE_KEYSERVER_RESULTS));
	parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
	obj = parent_class->constructor (type, n_construct_properties, construct_properties);
	self = SEAHORSE_KEYSERVER_RESULTS (obj);
	{
		char* title;
		GtkActionGroup* actions;
		GtkActionGroup* _tmp3;
		GtkActionGroup* _tmp4;
		gboolean _tmp5;
		GtkTreeView* _tmp7;
		GtkTreeView* _tmp6;
		SeahorseKeyPredicateFunc _tmp8;
		SeahorseKeyset* _tmp9;
		SeahorseKeyManagerStore* _tmp10;
		title = NULL;
		if (g_utf8_strlen (self->priv->_search_string, -1) == 0) {
			char* _tmp1;
			const char* _tmp0;
			_tmp1 = NULL;
			_tmp0 = NULL;
			title = (_tmp1 = (_tmp0 = _ ("Remote Keys"), (_tmp0 == NULL ? NULL : g_strdup (_tmp0))), (title = (g_free (title), NULL)), _tmp1);
		} else {
			char* _tmp2;
			_tmp2 = NULL;
			title = (_tmp2 = g_strdup_printf (_ ("Remote Keys Containing '%s'"), self->priv->_search_string), (title = (g_free (title), NULL)), _tmp2);
		}
		gtk_window_set_title (seahorse_view_get_window (SEAHORSE_VIEW (self)), title);
		/* 
		 * We hook callbacks up here for now because of a compiler warning. See:
		 * http://bugzilla.gnome.org/show_bug.cgi?id=539483
		 */
		actions = gtk_action_group_new ("general");
		gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
		gtk_action_group_add_actions (actions, SEAHORSE_KEYSERVER_RESULTS_GENERAL_ENTRIES, G_N_ELEMENTS (SEAHORSE_KEYSERVER_RESULTS_GENERAL_ENTRIES), self);
		g_signal_connect_object (gtk_action_group_get_action (actions, "app-close"), "activate", ((GCallback) (_seahorse_keyserver_results_on_app_close_gtk_action_activate)), self, 0);
		g_signal_connect_object (gtk_action_group_get_action (actions, "view-expand-all"), "activate", ((GCallback) (_seahorse_keyserver_results_on_view_expand_all_gtk_action_activate)), self, 0);
		g_signal_connect_object (gtk_action_group_get_action (actions, "view-collapse-all"), "activate", ((GCallback) (_seahorse_keyserver_results_on_view_collapse_all_gtk_action_activate)), self, 0);
		seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), actions);
		_tmp3 = NULL;
		actions = (_tmp3 = gtk_action_group_new ("keyserver"), (actions == NULL ? NULL : (actions = (g_object_unref (actions), NULL))), _tmp3);
		gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
		gtk_action_group_add_actions (actions, SEAHORSE_KEYSERVER_RESULTS_SERVER_ENTRIES, G_N_ELEMENTS (SEAHORSE_KEYSERVER_RESULTS_SERVER_ENTRIES), self);
		g_signal_connect_object (gtk_action_group_get_action (actions, "remote-find"), "activate", ((GCallback) (_seahorse_keyserver_results_on_remote_find_gtk_action_activate)), self, 0);
		seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), actions);
		_tmp4 = NULL;
		self->priv->_key_actions = (_tmp4 = gtk_action_group_new ("key"), (self->priv->_key_actions == NULL ? NULL : (self->priv->_key_actions = (g_object_unref (self->priv->_key_actions), NULL))), _tmp4);
		gtk_action_group_set_translation_domain (self->priv->_key_actions, GETTEXT_PACKAGE);
		gtk_action_group_add_actions (self->priv->_key_actions, SEAHORSE_KEYSERVER_RESULTS_KEY_ENTRIES, G_N_ELEMENTS (SEAHORSE_KEYSERVER_RESULTS_KEY_ENTRIES), self);
		g_signal_connect_object (gtk_action_group_get_action (self->priv->_key_actions, "key-import-keyring"), "activate", ((GCallback) (_seahorse_keyserver_results_on_key_import_keyring_gtk_action_activate)), self, 0);
		g_object_set (gtk_action_group_get_action (self->priv->_key_actions, "key-import-keyring"), "is-important", TRUE, NULL);
		seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), self->priv->_key_actions);
		/* init key list & selection settings */
		_tmp7 = NULL;
		_tmp6 = NULL;
		self->priv->_view = (_tmp7 = (_tmp6 = GTK_TREE_VIEW (seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "key_list")), (_tmp6 == NULL ? NULL : g_object_ref (_tmp6))), (self->priv->_view == NULL ? NULL : (self->priv->_view = (g_object_unref (self->priv->_view), NULL))), _tmp7);
		gtk_tree_selection_set_mode (gtk_tree_view_get_selection (self->priv->_view), GTK_SELECTION_MULTIPLE);
		g_signal_connect_object (gtk_tree_view_get_selection (self->priv->_view), "changed", ((GCallback) (_seahorse_keyserver_results_on_view_selection_changed_gtk_tree_selection_changed)), self, 0);
		g_signal_connect_object (self->priv->_view, "row-activated", ((GCallback) (_seahorse_keyserver_results_on_row_activated_gtk_tree_view_row_activated)), self, 0);
		g_signal_connect_object (GTK_WIDGET (self->priv->_view), "button-press-event", ((GCallback) (_seahorse_keyserver_results_on_key_list_button_pressed_gtk_widget_button_press_event)), self, 0);
		g_signal_connect_object (GTK_WIDGET (self->priv->_view), "popup-menu", ((GCallback) (_seahorse_keyserver_results_on_key_list_popup_menu_gtk_widget_popup_menu)), self, 0);
		gtk_widget_realize (GTK_WIDGET (self->priv->_view));
		/* Set focus to the current key list */
		gtk_widget_grab_focus (GTK_WIDGET (self->priv->_view));
		/* To avoid flicker */
		seahorse_viewer_ensure_updated (SEAHORSE_VIEWER (self));
		seahorse_widget_show (SEAHORSE_WIDGET (SEAHORSE_VIEWER (self)));
		/* Our predicate for filtering keys */
		self->priv->_pred.ktype = g_quark_from_string ("openpgp");
		self->priv->_pred.etype = SKEY_PUBLIC;
		self->priv->_pred.location = SKEY_LOC_REMOTE;
		self->priv->_pred.custom = (_tmp8 = _seahorse_keyserver_results_on_filter_keyset_seahorse_key_predicate_func, self->priv->_pred.custom_target = self, _tmp8);
		/* Our keyset all nicely filtered */
		_tmp9 = NULL;
		self->priv->_keyset = (_tmp9 = seahorse_keyset_new_full (&self->priv->_pred), (self->priv->_keyset == NULL ? NULL : (self->priv->_keyset = (g_object_unref (self->priv->_keyset), NULL))), _tmp9);
		_tmp10 = NULL;
		self->priv->_store = (_tmp10 = seahorse_key_manager_store_new (self->priv->_keyset, self->priv->_view), (self->priv->_store == NULL ? NULL : (self->priv->_store = (g_object_unref (self->priv->_store), NULL))), _tmp10);
		seahorse_keyserver_results_on_view_selection_changed (self, gtk_tree_view_get_selection (self->priv->_view));
		title = (g_free (title), NULL);
		(actions == NULL ? NULL : (actions = (g_object_unref (actions), NULL)));
	}
	return obj;
}


static void seahorse_keyserver_results_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SeahorseKeyserverResults * self;
	self = SEAHORSE_KEYSERVER_RESULTS (object);
	switch (property_id) {
		case SEAHORSE_KEYSERVER_RESULTS_SEARCH:
		g_value_set_string (value, seahorse_keyserver_results_get_search (self));
		break;
		case SEAHORSE_KEYSERVER_RESULTS_SELECTED_KEY:
		g_value_set_object (value, seahorse_keyserver_results_real_get_selected_key (self));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_keyserver_results_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
	SeahorseKeyserverResults * self;
	self = SEAHORSE_KEYSERVER_RESULTS (object);
	switch (property_id) {
		case SEAHORSE_KEYSERVER_RESULTS_SEARCH:
		seahorse_keyserver_results_set_search (self, g_value_get_string (value));
		break;
		case SEAHORSE_KEYSERVER_RESULTS_SELECTED_KEY:
		seahorse_keyserver_results_real_set_selected_key (self, g_value_get_object (value));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_keyserver_results_class_init (SeahorseKeyserverResultsClass * klass) {
	seahorse_keyserver_results_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseKeyserverResultsPrivate));
	G_OBJECT_CLASS (klass)->get_property = seahorse_keyserver_results_get_property;
	G_OBJECT_CLASS (klass)->set_property = seahorse_keyserver_results_set_property;
	G_OBJECT_CLASS (klass)->constructor = seahorse_keyserver_results_constructor;
	G_OBJECT_CLASS (klass)->dispose = seahorse_keyserver_results_dispose;
	SEAHORSE_VIEWER_CLASS (klass)->get_selected_keys = seahorse_keyserver_results_real_get_selected_keys;
	SEAHORSE_VIEWER_CLASS (klass)->set_selected_keys = seahorse_keyserver_results_real_set_selected_keys;
	SEAHORSE_VIEWER_CLASS (klass)->get_selected_key_and_uid = seahorse_keyserver_results_real_get_selected_key_and_uid;
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_KEYSERVER_RESULTS_SEARCH, g_param_spec_string ("search", "search", "search", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_KEYSERVER_RESULTS_SELECTED_KEY, "selected-key");
}


static void seahorse_keyserver_results_seahorse_view_interface_init (SeahorseViewIface * iface) {
	seahorse_keyserver_results_seahorse_view_parent_iface = g_type_interface_peek_parent (iface);
	iface->get_selected_keys = seahorse_keyserver_results_real_get_selected_keys;
	iface->set_selected_keys = seahorse_keyserver_results_real_set_selected_keys;
	iface->get_selected_key_and_uid = seahorse_keyserver_results_real_get_selected_key_and_uid;
}


static void seahorse_keyserver_results_instance_init (SeahorseKeyserverResults * self) {
	self->priv = SEAHORSE_KEYSERVER_RESULTS_GET_PRIVATE (self);
}


static void seahorse_keyserver_results_dispose (GObject * obj) {
	SeahorseKeyserverResults * self;
	self = SEAHORSE_KEYSERVER_RESULTS (obj);
	self->priv->_search_string = (g_free (self->priv->_search_string), NULL);
	(self->priv->_view == NULL ? NULL : (self->priv->_view = (g_object_unref (self->priv->_view), NULL)));
	(self->priv->_key_actions == NULL ? NULL : (self->priv->_key_actions = (g_object_unref (self->priv->_key_actions), NULL)));
	(self->priv->_store == NULL ? NULL : (self->priv->_store = (g_object_unref (self->priv->_store), NULL)));
	(self->priv->_keyset == NULL ? NULL : (self->priv->_keyset = (g_object_unref (self->priv->_keyset), NULL)));
	G_OBJECT_CLASS (seahorse_keyserver_results_parent_class)->dispose (obj);
}


GType seahorse_keyserver_results_get_type (void) {
	static GType seahorse_keyserver_results_type_id = 0;
	if (G_UNLIKELY (seahorse_keyserver_results_type_id == 0)) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorseKeyserverResultsClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_keyserver_results_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorseKeyserverResults), 0, (GInstanceInitFunc) seahorse_keyserver_results_instance_init };
		static const GInterfaceInfo seahorse_view_info = { (GInterfaceInitFunc) seahorse_keyserver_results_seahorse_view_interface_init, (GInterfaceFinalizeFunc) NULL, NULL};
		seahorse_keyserver_results_type_id = g_type_register_static (SEAHORSE_TYPE_VIEWER, "SeahorseKeyserverResults", &g_define_type_info, 0);
		g_type_add_interface_static (seahorse_keyserver_results_type_id, SEAHORSE_TYPE_VIEW, &seahorse_view_info);
	}
	return seahorse_keyserver_results_type_id;
}




