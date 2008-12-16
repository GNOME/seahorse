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

#include "config.h"

#include "seahorse-keyserver-results.h"
#include "seahorse-key-manager-store.h"
#include "seahorse-windows.h"

#include "seahorse-operation.h"
#include "seahorse-progress.h"

#include <glib/gi18n.h>

#include <string.h>

enum {
	PROP_0,
	PROP_SEARCH,
	PROP_SELECTED
};

struct _SeahorseKeyserverResultsPrivate {
	char *search_string;
	GtkTreeView *view;
	GtkActionGroup *object_actions;
	SeahorseKeyManagerStore *store;
	SeahorseSet *objects;
	SeahorseObjectPredicate pred;
};

G_DEFINE_TYPE (SeahorseKeyserverResults, seahorse_keyserver_results, SEAHORSE_TYPE_VIEWER);

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */


static gboolean 
on_filter_objects (SeahorseObject *obj, SeahorseKeyserverResults *self) 
{
	gboolean ret;
	char* name;

	g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), FALSE);
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (obj), FALSE);
	if (g_utf8_strlen (self->pv->search_string, -1) == 0)
		return TRUE;

	name = g_utf8_casefold (seahorse_object_get_label (obj), -1);
	ret = strstr (name, self->pv->search_string) != NULL;
	g_free (name);
	return ret;
}


static gboolean 
fire_selection_changed (SeahorseKeyserverResults* self) 
{
	gint rows;
	GtkTreeSelection* selection;
	g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), FALSE);

	selection = gtk_tree_view_get_selection (self->pv->view);
	rows = gtk_tree_selection_count_selected_rows (selection);
	seahorse_viewer_set_numbered_status (SEAHORSE_VIEWER (self), ngettext ("Selected %d key", "Selected %d keys", rows), rows);
	gtk_action_group_set_sensitive (self->pv->object_actions, rows > 0);
	g_signal_emit_by_name (G_OBJECT (SEAHORSE_VIEW (self)), "selection-changed");
	return FALSE;
}

static void 
on_view_selection_changed (GtkTreeSelection *selection, SeahorseKeyserverResults *self) 
{
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_idle_add ((GSourceFunc)fire_selection_changed, self);
}

static void 
on_row_activated (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, SeahorseKeyserverResults *self) 
{
	SeahorseObject *obj;

	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (GTK_IS_TREE_VIEW (view));
	g_return_if_fail (path != NULL);
	g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (column));

	obj = seahorse_key_manager_store_get_object_from_path (view, path);
	if (obj != NULL) 
		seahorse_viewer_show_properties (SEAHORSE_VIEWER (self), obj);
}

static gboolean 
on_key_list_button_pressed (GtkTreeView* view, GdkEventButton* event, SeahorseKeyserverResults* self) 
{
	g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), FALSE);
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), FALSE);
	if (event->button == 3)
		seahorse_viewer_show_context_menu (SEAHORSE_VIEWER (self), event->button, event->time);
	return FALSE;
}

static gboolean 
on_key_list_popup_menu (GtkTreeView* view, SeahorseKeyserverResults* self) 
{
	SeahorseObject* key;

	g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), FALSE);
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), FALSE);

	key = seahorse_viewer_get_selected (SEAHORSE_VIEWER (self));
	if (key == NULL)
		return FALSE;
	seahorse_viewer_show_context_menu (SEAHORSE_VIEWER (self), 0, gtk_get_current_event_time ());
	return TRUE;
}

static void 
on_view_expand_all (GtkAction* action, SeahorseKeyserverResults* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	gtk_tree_view_expand_all (self->pv->view);
}


static void 
on_view_collapse_all (GtkAction* action, SeahorseKeyserverResults* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	gtk_tree_view_collapse_all (self->pv->view);
}

static void 
on_app_close (GtkAction* action, SeahorseKeyserverResults* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (action == NULL || GTK_IS_ACTION (action));
	seahorse_widget_destroy (SEAHORSE_WIDGET (self));
}

static void 
imported_keys (SeahorseOperation* op, SeahorseKeyserverResults* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (SEAHORSE_IS_OPERATION (op));
	
	if (!seahorse_operation_is_successful (op)) {
		seahorse_operation_display_error (op, _ ("Couldn't import keys"), 
		                                  GTK_WIDGET (seahorse_viewer_get_window (SEAHORSE_VIEWER (self))));
		return;
	}
	
	seahorse_viewer_set_status (SEAHORSE_VIEWER (self), _ ("Imported keys"));
}

static void 
on_key_import_keyring (SeahorseKeyserverResults* self, GtkAction* action) 
{
	GList* keys;
	SeahorseOperation* op;

	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (GTK_IS_ACTION (action));

	keys = seahorse_viewer_get_selected_objects (SEAHORSE_VIEWER (self));
	
	/* No keys, nothing to do */
	if (keys == NULL) 
		return;

	op = seahorse_context_transfer_objects (seahorse_context_for_app (), keys, NULL);
	seahorse_progress_show (op, _ ("Importing keys from key servers"), TRUE);
	seahorse_operation_watch (op, (SeahorseDoneFunc)imported_keys, self, NULL, NULL);
	
	g_object_unref (op);
	g_list_free (keys);
}

static void 
on_remote_find (GtkAction* action, SeahorseKeyserverResults* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	seahorse_keyserver_search_show (seahorse_viewer_get_window (SEAHORSE_VIEWER (self)));
}


/* When this window closes we quit seahorse */
static gboolean 
on_delete_event (GtkWidget* widget, GdkEvent* event, SeahorseKeyserverResults* self) 
{
	g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	on_app_close (NULL, self);
	return TRUE;
}

static const GtkActionEntry GENERAL_ENTRIES[] = {
	{ "remote-menu", NULL, N_("_Remote") }, 
	{ "app-close", GTK_STOCK_CLOSE, N_("_Close"), "<control>W", 
	  N_("Close this window"), G_CALLBACK (on_app_close) }, 
	{ "view-expand-all", GTK_STOCK_ADD, N_("_Expand All"), NULL, 
	  N_("Expand all listings"), G_CALLBACK (on_view_expand_all) }, 
	{ "view-collapse-all", GTK_STOCK_REMOVE, N_("_Collapse All"), NULL, 
	  N_("Collapse all listings"), G_CALLBACK (on_view_collapse_all) } 
};

static const GtkActionEntry SERVER_ENTRIES[] = {
	{ "remote-find", GTK_STOCK_FIND, N_("_Find Remote Keys..."), "", 
	  N_("Search for keys on a key server"), G_CALLBACK (on_remote_find) }
};

static const GtkActionEntry KEY_ENTRIES[] = {
	{ "key-import-keyring", GTK_STOCK_ADD, N_("_Import"), "", 
	  N_("Import selected keys to local key ring"), G_CALLBACK (on_key_import_keyring) }
};

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static GList* 
seahorse_keyserver_results_get_selected_objects (SeahorseViewer* base) 
{
	SeahorseKeyserverResults * self = SEAHORSE_KEYSERVER_RESULTS (base);
	return seahorse_key_manager_store_get_selected_objects (self->pv->view);
}

static void 
seahorse_keyserver_results_set_selected_objects (SeahorseViewer* base, GList* keys) 
{
	SeahorseKeyserverResults * self = SEAHORSE_KEYSERVER_RESULTS (base);
	seahorse_key_manager_store_set_selected_objects (self->pv->view, keys);
}

static SeahorseObject* 
seahorse_keyserver_results_get_selected (SeahorseViewer* base) 
{
	SeahorseKeyserverResults* self;
	self = SEAHORSE_KEYSERVER_RESULTS (base);
	return seahorse_key_manager_store_get_selected_object (self->pv->view);
}

static void 
seahorse_keyserver_results_set_selected (SeahorseViewer* base, SeahorseObject* value) 
{
	SeahorseKeyserverResults* self = SEAHORSE_KEYSERVER_RESULTS (base);
	GList* keys = NULL;

	if (value != NULL)
		keys = g_list_prepend (keys, value);

	seahorse_viewer_set_selected_objects (SEAHORSE_VIEWER (self), keys);
	g_list_free (keys);;
	g_object_notify (G_OBJECT (self), "selected");
}


static GObject* 
seahorse_keyserver_results_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	SeahorseKeyserverResults *self = SEAHORSE_KEYSERVER_RESULTS (G_OBJECT_CLASS (seahorse_keyserver_results_parent_class)->constructor(type, n_props, props));
	GtkActionGroup* actions;
	GtkTreeSelection *selection;
	GtkWindow *window;
	char* title;

	g_return_val_if_fail (self, NULL);	


	if (g_utf8_strlen (self->pv->search_string, -1) == 0) {
		title = g_strdup (_("Remote Keys"));
	} else {
		title = g_strdup_printf (_ ("Remote Keys Containing '%s'"), self->pv->search_string);
	}

	window = seahorse_viewer_get_window (SEAHORSE_VIEWER (self));
	gtk_window_set_title (window, title);
	g_free (title);
	
	g_signal_connect (window, "delete-event", G_CALLBACK (on_delete_event), self);
	
	actions = gtk_action_group_new ("general");
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, GENERAL_ENTRIES, G_N_ELEMENTS (GENERAL_ENTRIES), self);
	seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), actions);

	actions = gtk_action_group_new ("keyserver");
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, SERVER_ENTRIES, G_N_ELEMENTS (SERVER_ENTRIES), self);
	seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), actions);
	
	self->pv->object_actions = gtk_action_group_new ("key");
	gtk_action_group_set_translation_domain (self->pv->object_actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (self->pv->object_actions, KEY_ENTRIES, G_N_ELEMENTS (KEY_ENTRIES), self);
	g_object_set (gtk_action_group_get_action (self->pv->object_actions, "key-import-keyring"), "is-important", TRUE, NULL);
	seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), self->pv->object_actions);

	/* init key list & selection settings */
	self->pv->view = GTK_TREE_VIEW (seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "key_list"));
	selection = gtk_tree_view_get_selection (self->pv->view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect_object (selection, "changed", G_CALLBACK (on_view_selection_changed), self, 0);
	g_signal_connect_object (self->pv->view, "row-activated", G_CALLBACK (on_row_activated), self, 0);
	g_signal_connect_object (self->pv->view, "button-press-event", G_CALLBACK (on_key_list_button_pressed), self, 0);
	g_signal_connect_object (self->pv->view, "popup-menu", G_CALLBACK (on_key_list_popup_menu), self, 0);
	gtk_widget_realize (GTK_WIDGET (self->pv->view));
	
	/* Set focus to the current key list */
	gtk_widget_grab_focus (GTK_WIDGET (self->pv->view));
	
	/* To avoid flicker */
	seahorse_viewer_ensure_updated (SEAHORSE_VIEWER (self));
	seahorse_widget_show (SEAHORSE_WIDGET (SEAHORSE_VIEWER (self)));
	
	/* Our predicate for filtering keys */
	self->pv->pred.tag = g_quark_from_string ("openpgp");
	self->pv->pred.usage = SEAHORSE_USAGE_PUBLIC_KEY;
	self->pv->pred.location = SEAHORSE_LOCATION_REMOTE;
	self->pv->pred.custom = (SeahorseObjectPredicateFunc)on_filter_objects;
	self->pv->pred.custom_target = self;
	
	/* Our set all nicely filtered */
	self->pv->objects = seahorse_set_new_full (&self->pv->pred);
	self->pv->store = seahorse_key_manager_store_new (self->pv->objects, self->pv->view);
	on_view_selection_changed (selection, self);
	
	return G_OBJECT (self);
}

static void
seahorse_keyserver_results_init (SeahorseKeyserverResults *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_KEYSERVER_RESULTS, SeahorseKeyserverResultsPrivate);
}

static void
seahorse_keyserver_results_finalize (GObject *obj)
{
	SeahorseKeyserverResults *self = SEAHORSE_KEYSERVER_RESULTS (obj);

	g_free (self->pv->search_string);
	self->pv->search_string = NULL;
	
	if (self->pv->object_actions)
		g_object_unref (self->pv->object_actions);
	self->pv->object_actions = NULL;
	
	if (self->pv->objects)
		g_object_unref (self->pv->objects);
	self->pv->objects = NULL;
	
	if (self->pv->store)
		g_object_unref (self->pv->store);
	self->pv->store = NULL;
	
	G_OBJECT_CLASS (seahorse_keyserver_results_parent_class)->finalize (obj);
}

static void
seahorse_keyserver_results_set_property (GObject *obj, guint prop_id, const GValue *value, 
                           GParamSpec *pspec)
{
	SeahorseKeyserverResults *self = SEAHORSE_KEYSERVER_RESULTS (obj);
	const gchar* str;
	
	switch (prop_id) {
	case PROP_SEARCH:
		/* Many key servers ignore spaces at the beginning and end, so we do too */
		str = g_value_get_string (value);
		if (!str)
			str = "";
		self->pv->search_string = g_strstrip (g_utf8_casefold (str, -1));
		break;
		
	case PROP_SELECTED:
		seahorse_viewer_set_selected (SEAHORSE_VIEWER (self), g_value_get_object (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_keyserver_results_get_property (GObject *obj, guint prop_id, GValue *value, 
                           GParamSpec *pspec)
{
	SeahorseKeyserverResults *self = SEAHORSE_KEYSERVER_RESULTS (obj);
	
	switch (prop_id) {
	case PROP_SEARCH:
		g_value_set_string (value, seahorse_keyserver_results_get_search (self));
		break;
	case PROP_SELECTED:
		g_value_set_object (value, seahorse_viewer_get_selected (SEAHORSE_VIEWER (self)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_keyserver_results_class_init (SeahorseKeyserverResultsClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    
	seahorse_keyserver_results_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseKeyserverResultsPrivate));

	gobject_class->constructor = seahorse_keyserver_results_constructor;
	gobject_class->finalize = seahorse_keyserver_results_finalize;
	gobject_class->set_property = seahorse_keyserver_results_set_property;
	gobject_class->get_property = seahorse_keyserver_results_get_property;
    
	SEAHORSE_VIEWER_CLASS (klass)->get_selected_objects = seahorse_keyserver_results_get_selected_objects;
	SEAHORSE_VIEWER_CLASS (klass)->set_selected_objects = seahorse_keyserver_results_set_selected_objects;
	SEAHORSE_VIEWER_CLASS (klass)->get_selected = seahorse_keyserver_results_get_selected;
	SEAHORSE_VIEWER_CLASS (klass)->set_selected = seahorse_keyserver_results_set_selected;
	
	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SEARCH, 
	         g_param_spec_string ("search", "search", "search", NULL, 
	                              G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_override_property (G_OBJECT_CLASS (klass), PROP_SELECTED, "selected");
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseKeyserverResults*
seahorse_keyserver_results_new (void)
{
	return g_object_new (SEAHORSE_TYPE_KEYSERVER_RESULTS, NULL);
}

void 
seahorse_keyserver_results_show (SeahorseOperation* op, GtkWindow* parent, const char* search_text) 
{
	SeahorseKeyserverResults* res;
	GtkWindow *window;
	
	g_return_if_fail (SEAHORSE_IS_OPERATION (op));
	g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));
	g_return_if_fail (search_text != NULL);
	
	res = g_object_new (SEAHORSE_TYPE_KEYSERVER_RESULTS, "name", "keyserver-results", "search", search_text, NULL);
	
	/* Destorys itself with destroy */
	g_object_ref_sink (res);
	
	if (parent != NULL) {
		window = GTK_WINDOW (seahorse_widget_get_toplevel (SEAHORSE_WIDGET (res)));
		gtk_window_set_transient_for (window, parent);
	}

	seahorse_progress_status_set_operation (SEAHORSE_WIDGET (res), op);
}

const gchar* 
seahorse_keyserver_results_get_search (SeahorseKeyserverResults* self) 
{
	g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), NULL);
	return self->pv->search_string;
}
