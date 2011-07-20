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

#include "seahorse-generate-select.h"
#include "seahorse-key-manager.h"
#include "seahorse-key-manager-store.h"
#include "seahorse-preferences.h"
#include "seahorse-windows.h"
#include "seahorse-keyserver-results.h"

#include "seahorse-gconf.h"
#include "seahorse-operation.h"
#include "seahorse-progress.h"
#include "seahorse-util.h"

#include "gkr/seahorse-gkr.h"

#include <glib/gi18n.h>

enum {
	PROP_0,
	PROP_SELECTED
};
typedef struct _TabInfo {
	guint id;
	gint page;
	GtkTreeView* view;
	SeahorseSet* objects;
	GtkWidget* widget;
	SeahorseKeyManagerStore* store;
} TabInfo;

struct _SeahorseKeyManagerPrivate {
	GtkNotebook* notebook;
	GtkActionGroup* view_actions;
	GtkEntry* filter_entry;
	GQuark track_selected_id;
	guint track_selected_tab;
	TabInfo* tabs;
};

enum  {
	TARGETS_PLAIN,
	TARGETS_URIS
};

enum  {
	TAB_PUBLIC = 0,
	TAB_PRIVATE,
	TAB_PASSWORD,
	TAB_NUM_TABS
} SeahorseKeyManagerTabs;

static SeahorseObjectPredicate pred_public = {
	0, 
	0, 
	0, 
	SEAHORSE_LOCATION_LOCAL, 
	SEAHORSE_USAGE_PUBLIC_KEY, 
	0, 
	0, 
	NULL
};

static SeahorseObjectPredicate pred_private = {
	0, 
	0, 
	0, 
	SEAHORSE_LOCATION_LOCAL, 
	SEAHORSE_USAGE_PRIVATE_KEY, 
	0, 
	0, 
	NULL
};

static SeahorseObjectPredicate pred_password = {
	0, 
	0, /* Tag filled in later */ 
	0, 
	SEAHORSE_LOCATION_LOCAL, 
	0, 
	0, 
	0, 
	NULL
};

G_DEFINE_TYPE (SeahorseKeyManager, seahorse_key_manager, SEAHORSE_TYPE_VIEWER);

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static TabInfo* 
get_tab_for_object (SeahorseKeyManager* self, SeahorseObject* obj) 
{
	gint i;
	
	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), NULL);
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (obj), NULL);
	
	for (i = 0; i < TAB_NUM_TABS; ++i) {
		TabInfo* tab = &self->pv->tabs[i];
		if (seahorse_set_has_object (tab->objects, obj))
			return tab;
	}
	
	return NULL;
}

static TabInfo* 
get_tab_info (SeahorseKeyManager* self, gint page) 
{
	gint i;
	
	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), NULL);

	if (page < 0) 
		page = gtk_notebook_get_current_page (self->pv->notebook);
	if (page < 0)
		return NULL;

	for (i = 0; i < TAB_NUM_TABS; ++i)
	{
		TabInfo* tab = &self->pv->tabs[i];
		if (tab->page == page)
			return tab;
	}

	return NULL;
}

static GtkTreeView* 
get_current_view (SeahorseKeyManager* self) 
{
	TabInfo* tab;
	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), NULL);
	tab = get_tab_info (self, -1);
	if (tab == NULL)
		return NULL;
	return tab->view;
}

static guint 
get_tab_id (SeahorseKeyManager* self, TabInfo* tab) 
{
	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), 0U);
	if (tab == NULL) 
		return 0;
	return tab->id;
}

static void 
set_tab_current (SeahorseKeyManager* self, TabInfo* tab) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	gtk_notebook_set_current_page (self->pv->notebook, tab->page);
	g_signal_emit_by_name (G_OBJECT (SEAHORSE_VIEW (self)), "selection-changed");
}

static gboolean 
fire_selection_changed (SeahorseKeyManager* self) 
{
	gboolean selected;
	gint rows;
	GtkTreeView* view;
	gboolean dotracking;
	guint tabid;
	GQuark keyid;
	GList* objects;

	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), FALSE);

	selected = FALSE;
	rows = 0;

	view = get_current_view (self);
	
	if (view != NULL) {
		GtkTreeSelection* selection = gtk_tree_view_get_selection (view);
		rows = gtk_tree_selection_count_selected_rows (selection);
		selected = rows > 0;
	}
	
	dotracking = TRUE;
	
	/* See which tab we're on, if different from previous, no tracking */
	tabid = get_tab_id (self, get_tab_info (self, -1));
	if (tabid != self->pv->track_selected_tab) {
		dotracking = FALSE;
		self->pv->track_selected_tab = tabid;
	}
	
	/* Retrieve currently tracked, and reset tracking */
	keyid = self->pv->track_selected_id;
	self->pv->track_selected_id = 0;
	
	/* no selection, see if selected key moved to another tab */
	if (dotracking && rows == 0 && keyid != 0) {
		/* Find it */
		SeahorseObject* obj = seahorse_context_find_object (NULL, keyid, SEAHORSE_LOCATION_LOCAL);
		if (obj != NULL) {
			/* If it's still around, then select it */
			TabInfo* tab = get_tab_for_object (self, obj);
			if (tab != NULL && tab != get_tab_info (self, -1)) {
				/* Make sure we don't end up in a loop  */
				g_assert (self->pv->track_selected_id == 0);
				seahorse_viewer_set_selected (SEAHORSE_VIEWER (self), obj);
			}
		}
	}

	if (selected) {
		seahorse_viewer_set_numbered_status (SEAHORSE_VIEWER (self), ngettext ("Selected %d key", "Selected %d keys", rows), rows);
		objects = seahorse_viewer_get_selected_objects (SEAHORSE_VIEWER (self));
		
		/* If one key is selected then mark it down for following across tabs */
		if (objects->data)
			self->pv->track_selected_id = seahorse_object_get_id (objects->data);
		
		g_list_free (objects);
	}

	/* Fire the signal */
	g_signal_emit_by_name (self, "selection-changed");
	
	/* This is called as a one-time idle handler, return FALSE so we don't get run again */
	return FALSE;
}

static void 
on_tab_changed (GtkNotebook* notebook, void* unused, guint page_num, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
	gtk_entry_set_text (self->pv->filter_entry, "");
	
	/* Don't track the selected key when tab is changed on purpose */
	self->pv->track_selected_id = 0;
	
	fire_selection_changed (self);
}

static void 
on_view_selection_changed (GtkTreeSelection* selection, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_idle_add ((GSourceFunc)fire_selection_changed, self);
}

G_MODULE_EXPORT void
on_keymanager_row_activated (GtkTreeView* view, GtkTreePath* path, 
                                  GtkTreeViewColumn* column, SeahorseKeyManager* self) 
{
	SeahorseObject* obj;

	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_TREE_VIEW (view));
	g_return_if_fail (path != NULL);
	g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (column));
	
	obj = seahorse_key_manager_store_get_object_from_path (view, path);
	if (obj != NULL)
		seahorse_viewer_show_properties (SEAHORSE_VIEWER (self), obj);
}

G_MODULE_EXPORT gboolean
on_keymanager_key_list_button_pressed (GtkTreeView* view, GdkEventButton* event, SeahorseKeyManager* self) 
{
	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), FALSE);
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), FALSE);
	
	if (event->button == 3)
		seahorse_viewer_show_context_menu (SEAHORSE_VIEWER (self), event->button, event->time);

	return FALSE;
}

G_MODULE_EXPORT gboolean
on_keymanager_key_list_popup_menu (GtkTreeView* view, SeahorseKeyManager* self) 
{
	SeahorseObject* obj;

	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), FALSE);
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), FALSE);

	obj = seahorse_viewer_get_selected (SEAHORSE_VIEWER (self));
	if (obj != NULL) 
		seahorse_viewer_show_context_menu (SEAHORSE_VIEWER (self), 0, gtk_get_current_event_time ());
	return FALSE;
}

static void 
on_file_new (GtkAction* action, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	seahorse_generate_select_show (seahorse_viewer_get_window (SEAHORSE_VIEWER (self)));
}

G_MODULE_EXPORT void 
on_keymanager_new_button (GtkButton* button, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_BUTTON (button));
	seahorse_generate_select_show (seahorse_viewer_get_window (SEAHORSE_VIEWER (self)));
}

static void 
initialize_tab (SeahorseKeyManager* self, const char* tabwidget, guint tabid, const char* viewwidget, 
                const SeahorseObjectPredicate* pred) 
{
	SeahorseSet *objects;
	GtkTreeSelection *selection;
	GtkTreeView *view;
	
	g_assert (tabid < (int)TAB_NUM_TABS);
	
	self->pv->tabs[tabid].id = tabid;
	self->pv->tabs[tabid].widget = seahorse_widget_get_widget (SEAHORSE_WIDGET (self), tabwidget);
	g_return_if_fail (self->pv->tabs[tabid].widget != NULL);
	
	self->pv->tabs[tabid].page = gtk_notebook_page_num (self->pv->notebook, self->pv->tabs[tabid].widget);
	g_return_if_fail (self->pv->tabs[tabid].page >= 0);
	
	objects = seahorse_set_new_full ((SeahorseObjectPredicate*)pred);
	self->pv->tabs[tabid].objects = objects;

	/* Init key list & selection settings */
	view = GTK_TREE_VIEW (seahorse_widget_get_widget (SEAHORSE_WIDGET (self), viewwidget));
	self->pv->tabs[tabid].view = view;
	g_return_if_fail (view != NULL);
	
	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (selection, "changed", G_CALLBACK (on_view_selection_changed), self);
	gtk_widget_realize (GTK_WIDGET (view));

	/* Add new key store and associate it */
	self->pv->tabs[tabid].store = seahorse_key_manager_store_new (objects, view);
}

static gboolean 
on_first_timer (SeahorseKeyManager* self) 
{
	GtkWidget* widget;

	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), FALSE);
	
	/* 
	 * Although not all the keys have completed we'll know whether we have 
	 * any or not at this point 
	 */
	
	if (seahorse_context_get_count (NULL) == 0) {
		widget = seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "first-time-box");
		gtk_widget_show (widget);
	}
	
	return FALSE;
}

static void
on_clear_clicked (GtkEntry* entry, GtkEntryIconPosition icon_pos, GdkEvent* event, gpointer user_data)
{
	gtk_entry_set_text (entry, "");
}

static void 
on_filter_changed (GtkEntry* entry, SeahorseKeyManager* self) 
{
	const gchar *text;
	gint i;

	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_ENTRY (entry));

	text = gtk_entry_get_text (entry);
	for (i = 0; i < TAB_NUM_TABS; ++i)
		g_object_set (self->pv->tabs[i].store, "filter", text, NULL);
}

static void 
imported_keys (SeahorseOperation* op, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (SEAHORSE_IS_OPERATION (op));
	
	if (!seahorse_operation_is_successful (op)) {
		seahorse_operation_display_error (op, _("Couldn't import keys"), 
		                                  GTK_WIDGET (seahorse_viewer_get_window (SEAHORSE_VIEWER (self))));
		return;
	}
	
	seahorse_viewer_set_status (SEAHORSE_VIEWER (self), _("Imported keys"));
}

static void 
import_files (SeahorseKeyManager* self, const gchar** uris) 
{
	GError *error = NULL;
	SeahorseMultiOperation* mop;
	GFileInputStream* input;
	SeahorseOperation* op;
	const gchar *uri;
	GString *errmsg;
	GFile* file;
	
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	mop = g_object_new (SEAHORSE_TYPE_MULTI_OPERATION, NULL);
	errmsg = g_string_new ("");
	
	for (uri = *uris; uri; uris++, uri = *uris) {
		GQuark ktype;
		SeahorseSource* sksrc;
		
		if(!uri[0])
			continue;
			
		/* Figure out where to import to */
		ktype = seahorse_util_detect_file_type (uri);
		if (ktype == 0) {
			g_string_append_printf (errmsg, "%s: Invalid file format\n", uri);
			continue;
		}
		
		/* All our supported key types have a local source */
		sksrc = seahorse_context_find_source (NULL, ktype, SEAHORSE_LOCATION_LOCAL);
		g_return_if_fail (sksrc != NULL);

		file = g_file_new_for_uri (uri);
		input = g_file_read (file, NULL, &error);
		if (error) {
			g_string_append_printf (errmsg, "%s: %s\n", uri, error->message);
			g_clear_error (&error);
			continue;
		}			
		
		op = seahorse_source_import (sksrc, G_INPUT_STREAM (input));
		seahorse_multi_operation_take (mop, op);
	}
	
	if (seahorse_operation_is_running (SEAHORSE_OPERATION (mop))) {
		seahorse_progress_show (SEAHORSE_OPERATION (mop), _("Importing keys"), TRUE);
		seahorse_operation_watch (SEAHORSE_OPERATION (mop), (SeahorseDoneFunc)imported_keys, self, NULL, NULL);
	}
	
	if (errmsg->len > 0)
		seahorse_util_show_error (GTK_WIDGET (seahorse_viewer_get_window (SEAHORSE_VIEWER (self))), 
		                          _("Couldn't import keys"), errmsg->str);

	
	g_object_unref (mop);
	g_string_free (errmsg, TRUE);
}


static void 
import_prompt (SeahorseKeyManager* self) 
{
	GtkDialog* dialog;
	gchar *uris[2];
	gchar* uri;

	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));

	dialog = seahorse_util_chooser_open_new (_("Import Key"), 
	                                         seahorse_viewer_get_window (SEAHORSE_VIEWER (self)));
	seahorse_util_chooser_show_key_files (dialog);
	
	uri = seahorse_util_chooser_open_prompt (dialog);
	if (uri != NULL) {
		uris[0] = uri;
		uris[1] = NULL;
		import_files (self, (const gchar**)uris);
	}
	
	g_free (uri);
}

static void 
on_key_import_file (GtkAction* action, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	import_prompt (self);
}

G_MODULE_EXPORT void 
on_keymanager_import_button (GtkButton* button, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_BUTTON (button));
	import_prompt (self);
}

static void 
import_text (SeahorseKeyManager* self, const char* text) 
{
	glong len;
	GQuark ktype;
	SeahorseSource* sksrc;
	GMemoryInputStream* input;
	SeahorseOperation* op;
	
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (text != NULL);
	
	len = g_utf8_strlen (text, -1);
	ktype = seahorse_util_detect_data_type (text, len);
	if (ktype == 0) {
		seahorse_util_show_error (GTK_WIDGET (seahorse_viewer_get_window (SEAHORSE_VIEWER (self))), 
		                          _("Couldn't import keys"), _("Unrecognized key type, or invalid data format"));
		return;
	}
	
	/* All our supported key types have a local key source */
	sksrc = seahorse_context_find_source (seahorse_context_instance (), ktype, SEAHORSE_LOCATION_LOCAL);
	g_return_if_fail (sksrc != NULL);

	input = seahorse_util_memory_input_string (text, len);
	op = seahorse_source_import (sksrc, G_INPUT_STREAM (input));
	
	seahorse_progress_show (op, _("Importing Keys"), TRUE);
	seahorse_operation_watch (op, (SeahorseDoneFunc)imported_keys, self, NULL, NULL);

	g_object_unref (input);
	g_object_unref (op);
}

static void 
on_target_drag_data_received (GtkWindow* window, GdkDragContext* context, gint x, gint y, 
                              GtkSelectionData* selection_data, guint info, guint time_,
                              SeahorseKeyManager* self) 
{
	guchar *text;
	gchar **uris;
	gchar **uri;
	
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_WINDOW (window));
	g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
	g_return_if_fail (selection_data != NULL);
	
	if (info == TARGETS_PLAIN) {
		text = gtk_selection_data_get_text (selection_data);
		import_text (self, (gchar*)text);
		g_free (text);
	} else if (info == TARGETS_URIS) {
		uris = gtk_selection_data_get_uris (selection_data);
		for (uri = uris; *uri; ++uri) 
			g_strstrip (*uri);
		import_files (self, (const gchar**)uris);
		g_strfreev (uris);
	}
}

static void 
on_clipboard_received (GtkClipboard* board, const char* text, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_CLIPBOARD (board));
	g_return_if_fail (text != NULL);

    g_assert(self->pv->filter_entry);
    if (gtk_widget_is_focus (GTK_WIDGET (self->pv->filter_entry)) == TRUE)
	    gtk_editable_paste_clipboard (GTK_EDITABLE (self->pv->filter_entry));
    else	
    	if (text != NULL && g_utf8_strlen (text, -1) > 0)
    		import_text (self, text);
}

static void 
on_key_import_clipboard (GtkAction* action, SeahorseKeyManager* self) 
{
	GdkAtom atom;
	GtkClipboard* board;
	
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	
	atom = gdk_atom_intern ("CLIPBOARD", FALSE);
	board = gtk_clipboard_get (atom);
	gtk_clipboard_request_text (board, (GtkClipboardTextReceivedFunc)on_clipboard_received, self);
}

#ifdef WITH_KEYSERVER
static void 
on_remote_find (GtkAction* action, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	seahorse_keyserver_search_show (seahorse_viewer_get_window (SEAHORSE_VIEWER (self)));
}

static void 
on_remote_sync (GtkAction* action, SeahorseKeyManager* self) 
{
	GList* objects;

	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	
	objects = seahorse_viewer_get_selected_objects (SEAHORSE_VIEWER (self));
	if (objects == NULL)
		objects = seahorse_context_find_objects (NULL, 0, 0, SEAHORSE_LOCATION_LOCAL);
	seahorse_keyserver_sync_show (objects, seahorse_viewer_get_window (SEAHORSE_VIEWER (self)));
	g_list_free (objects);
}
#endif

static gboolean
quit_app_later (gpointer unused)
{
	seahorse_context_destroy (seahorse_context_instance ());
	return FALSE;
}


static void 
on_app_quit (GtkAction* action, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	g_idle_add (quit_app_later, NULL);
}

/* When this window closes we quit seahorse */
static gboolean 
on_delete_event (GtkWidget* widget, GdkEvent* event, SeahorseKeyManager* self) 
{
	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	g_idle_add (quit_app_later, NULL);
	return TRUE;
}

static void 
on_view_type_activate (GtkToggleAction* action, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));
	seahorse_gconf_set_boolean (SHOW_TYPE_KEY, gtk_toggle_action_get_active (action));
}


static void 
on_view_expires_activate (GtkToggleAction* action, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));
	seahorse_gconf_set_boolean (SHOW_EXPIRES_KEY, gtk_toggle_action_get_active (action));
}


static void 
on_view_validity_activate (GtkToggleAction* action, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));
	seahorse_gconf_set_boolean (SHOW_VALIDITY_KEY, gtk_toggle_action_get_active (action));
}

static void 
on_view_trust_activate (GtkToggleAction* action, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));
	seahorse_gconf_set_boolean (SHOW_TRUST_KEY, gtk_toggle_action_get_active (action));
}

static void 
on_gconf_notify (GConfClient* client, guint cnxn_id, GConfEntry* entry, SeahorseKeyManager* self) 
{
	GtkToggleAction* action;
	const gchar* key;
	const char* name;

	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GCONF_IS_CLIENT (client));
	g_return_if_fail (entry != NULL);
	
	key = entry->key;
	g_return_if_fail (key);
	
	if (g_str_equal (key, SHOW_TRUST_KEY)) 
		name = "view-trust";
	else if (g_str_equal (key, SHOW_TYPE_KEY)) 
		name = "view-type";
	else if (g_str_equal (key, SHOW_EXPIRES_KEY)) 
		name = "view-expires";
	else if (g_str_equal (key, SHOW_VALIDITY_KEY)) 
		name = "view-validity";
	else
		return;
	
	action = GTK_TOGGLE_ACTION (gtk_action_group_get_action (self->pv->view_actions, name));
	g_return_if_fail (action != NULL);
	
	gtk_toggle_action_set_active (action, gconf_value_get_bool (entry->value));
}

static void
on_refreshing (SeahorseContext *sctx, SeahorseOperation *operation, SeahorseWidget *swidget)
{
	seahorse_progress_status_set_operation (swidget, operation);
}


static const GtkActionEntry GENERAL_ENTRIES[] = {
	{ "remote-menu", NULL, N_("_Remote") }, 
	{ "app-quit", GTK_STOCK_QUIT, NULL, "<control>Q", 
	  N_("Close this program"), G_CALLBACK (on_app_quit) }, 
	{ "file-new", GTK_STOCK_NEW, N_("_New..."), "<control>N", 
	  N_("Create a new key or item"), G_CALLBACK (on_file_new) }, 
	{ "file-import", GTK_STOCK_OPEN, N_("_Import..."), "<control>I", 
	  N_("Import from a file"), G_CALLBACK (on_key_import_file) }, 
	{ "edit-import-clipboard", GTK_STOCK_PASTE, NULL, "<control>V", 
	  N_("Import from the clipboard"), G_CALLBACK (on_key_import_clipboard) }
};

#ifdef WITH_KEYSERVER
static const GtkActionEntry SERVER_ENTRIES[] = {
	{ "remote-find", GTK_STOCK_FIND, N_("_Find Remote Keys..."), "", 
	  N_("Search for keys on a key server"), G_CALLBACK (on_remote_find) }, 
	{ "remote-sync", GTK_STOCK_REFRESH, N_("_Sync and Publish Keys..."), "", 
	  N_("Publish and/or synchronize your keys with those online."), G_CALLBACK (on_remote_sync) }
};
#endif

static const GtkToggleActionEntry VIEW_ENTRIES[] = {
	{ "view-type", NULL, N_("T_ypes"), NULL, N_("Show type column"), 
	  G_CALLBACK (on_view_type_activate), FALSE }, 
	{ "view-expires", NULL, N_("_Expiry"), NULL, N_("Show expiry column"), 
	  G_CALLBACK (on_view_expires_activate), FALSE }, 
	{ "view-trust", NULL, N_("_Trust"), NULL, N_("Show owner trust column"), 
	  G_CALLBACK (on_view_trust_activate), FALSE}, 
	{ "view-validity", NULL, N_("_Validity"), NULL, N_("Show validity column"), 
	  G_CALLBACK (on_view_validity_activate), FALSE }
};

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static GList* 
seahorse_key_manager_get_selected_objects (SeahorseViewer* base) 
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (base); 
	TabInfo* tab = get_tab_info (self, -1);
	if (tab == NULL)
		return NULL;
	return seahorse_key_manager_store_get_selected_objects (tab->view);
}

static void 
seahorse_key_manager_set_selected_objects (SeahorseViewer* base, GList* objects) 
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (base);
	GList** tab_lists;
	GList *l;
	gint i;
	guint highest_matched;
	TabInfo* highest_tab;

	tab_lists = g_new0 (GList*, TAB_NUM_TABS + 1);
	
	/* Break objects into what's on each tab */
	for (l = objects; l; l = g_list_next (l)) {
		SeahorseObject* obj = SEAHORSE_OBJECT (l->data);
		TabInfo* tab = get_tab_for_object (self, obj);
		if (tab == NULL) 
			continue;

		g_assert (tab->id < TAB_NUM_TABS);
		tab_lists[tab->id] = g_list_prepend (tab_lists[tab->id], obj);
	}
	
	highest_matched = 0;
	highest_tab = NULL;
	
	for (i = 0; i < TAB_NUM_TABS; ++i) {
		GList* list = tab_lists[i];
		TabInfo* tab = &self->pv->tabs[i];

		/* Save away the tab that had the most objects */
		guint num = g_list_length (list);
		if (num > highest_matched) {
			highest_matched = num;
			highest_tab = tab;
		}
		
		/* Select the objects on that tab */
		seahorse_key_manager_store_set_selected_objects (tab->view, list);
		
		/* Free the broken down list */
		g_list_free (list);
	}
	
	g_free (tab_lists);
	
	/* Change to the tab with the most objects */
	if (highest_tab != NULL)
		set_tab_current (self, highest_tab);
}

static SeahorseObject* 
seahorse_key_manager_get_selected (SeahorseViewer* base) 
{
	SeahorseKeyManager* self = SEAHORSE_KEY_MANAGER (base);
	TabInfo* tab = get_tab_info (self, -1);
	if (tab == NULL)
		return NULL;
	return seahorse_key_manager_store_get_selected_object (tab->view);
}

static void 
seahorse_key_manager_set_selected (SeahorseViewer* base, SeahorseObject* value) 
{
	SeahorseKeyManager* self = SEAHORSE_KEY_MANAGER (base);
	GList* objects = NULL; 
	objects = g_list_prepend (objects, value);
	seahorse_viewer_set_selected_objects (SEAHORSE_VIEWER (self), objects);
	g_list_free (objects);
	g_object_notify (G_OBJECT (self), "selected");
}

static GObject* 
seahorse_key_manager_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (G_OBJECT_CLASS (seahorse_key_manager_parent_class)->constructor(type, n_props, props));
	GtkActionGroup* actions;
	GtkToggleAction* action;
	GtkTargetList* targets;
	GtkWidget* widget;

	g_return_val_if_fail (self, NULL);	

	self->pv->tabs = g_new0 (TabInfo, TAB_NUM_TABS);

	self->pv->notebook = GTK_NOTEBOOK (seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "notebook"));
	gtk_window_set_title (seahorse_viewer_get_window (SEAHORSE_VIEWER (self)), _("Passwords and Encryption Keys"));
	
	actions = gtk_action_group_new ("general");
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, GENERAL_ENTRIES, G_N_ELEMENTS (GENERAL_ENTRIES), self);
	seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), actions);

#ifdef WITH_KEYSERVER	
	actions = gtk_action_group_new ("keyserver");
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, SERVER_ENTRIES, G_N_ELEMENTS (SERVER_ENTRIES), self);
	seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), actions);
#endif /* WITH_KEYSERVER */
	
	self->pv->view_actions = gtk_action_group_new ("view");
	gtk_action_group_set_translation_domain (self->pv->view_actions, GETTEXT_PACKAGE);
	gtk_action_group_add_toggle_actions (self->pv->view_actions, VIEW_ENTRIES, G_N_ELEMENTS (VIEW_ENTRIES), self);
	action = GTK_TOGGLE_ACTION (gtk_action_group_get_action (self->pv->view_actions, "view-type"));
	gtk_toggle_action_set_active (action, seahorse_gconf_get_boolean (SHOW_TYPE_KEY));
	action = GTK_TOGGLE_ACTION (gtk_action_group_get_action (self->pv->view_actions, "view-expires"));
	gtk_toggle_action_set_active (action, seahorse_gconf_get_boolean (SHOW_EXPIRES_KEY));
	action = GTK_TOGGLE_ACTION (gtk_action_group_get_action (self->pv->view_actions, "view-trust"));
	gtk_toggle_action_set_active (action, seahorse_gconf_get_boolean (SHOW_TRUST_KEY));
	action = GTK_TOGGLE_ACTION (gtk_action_group_get_action (self->pv->view_actions, "view-validity"));
	gtk_toggle_action_set_active (action, seahorse_gconf_get_boolean (SHOW_VALIDITY_KEY));
	seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), self->pv->view_actions);
	
	/* Notify us when gconf stuff changes under this key */
	seahorse_gconf_notify_lazy (LISTING_SCHEMAS, (GConfClientNotifyFunc)on_gconf_notify, self, self);
	
	/* close event */
	g_signal_connect_object (seahorse_widget_get_toplevel (SEAHORSE_WIDGET (self)), 
	                         "delete-event", G_CALLBACK (on_delete_event), self, 0);
	
	/* first time signals */
	g_signal_connect_object (seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "import-button"), 
	                         "clicked", G_CALLBACK (on_keymanager_import_button), self, 0);
	g_signal_connect_object (seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "new-button"), 
	                         "clicked", G_CALLBACK (on_keymanager_new_button), self, 0);
	
	/* The notebook */
	g_signal_connect_object (self->pv->notebook, "switch-page", G_CALLBACK (on_tab_changed), self, G_CONNECT_AFTER);
	
	/* Flush all updates */
	seahorse_viewer_ensure_updated (SEAHORSE_VIEWER (self));
	
	/* Find the toolbar */
	widget = seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "toolbar-placeholder");
	if (widget != NULL) {
		GList* children = gtk_container_get_children ((GTK_CONTAINER (widget)));
		if (children != NULL && children->data != NULL) {
			GtkToolbar* toolbar;

			/* The toolbar is the first (and only) element */
			toolbar = GTK_TOOLBAR (children->data);
			if (toolbar != NULL && G_TYPE_FROM_INSTANCE (G_OBJECT (toolbar)) == GTK_TYPE_TOOLBAR) {
				GtkSeparatorToolItem* sep;
				GtkBox* box;
				GtkToolItem* item;

				gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (toolbar)),
				                             GTK_STYLE_CLASS_PRIMARY_TOOLBAR);
				gtk_widget_reset_style (GTK_WIDGET (toolbar));
				
				/* Insert a separator to right align the filter */
				sep = GTK_SEPARATOR_TOOL_ITEM (gtk_separator_tool_item_new ());
				gtk_separator_tool_item_set_draw (sep, FALSE);
				gtk_tool_item_set_expand (GTK_TOOL_ITEM (sep), TRUE);
				gtk_widget_show_all (GTK_WIDGET (sep));
				gtk_toolbar_insert (toolbar, GTK_TOOL_ITEM (sep), -1);
				
				/* Insert a filter bar */
				box = GTK_BOX (gtk_hbox_new (FALSE, 0));
				gtk_box_pack_start (box, GTK_WIDGET (gtk_label_new (_("Filter:"))), FALSE, TRUE, 3);
				
				self->pv->filter_entry = GTK_ENTRY (gtk_entry_new ());
				gtk_box_pack_start (box, GTK_WIDGET (self->pv->filter_entry), FALSE, TRUE, 0);

				gtk_box_pack_start (box, gtk_label_new (NULL), FALSE, FALSE, 0);
				gtk_widget_show_all (GTK_WIDGET (box));
				
				item = gtk_tool_item_new ();
				gtk_container_add (GTK_CONTAINER (item), GTK_WIDGET (box));
				gtk_widget_show_all (GTK_WIDGET (item));
				gtk_toolbar_insert (toolbar, item, -1);
			}
		}
	}

	gtk_entry_set_icon_from_icon_name (self->pv->filter_entry,
					   GTK_ENTRY_ICON_PRIMARY,
					   GTK_STOCK_FIND);
	gtk_entry_set_icon_from_icon_name (self->pv->filter_entry,
					   GTK_ENTRY_ICON_SECONDARY,
					   GTK_STOCK_CLEAR);

	gtk_entry_set_icon_activatable (self->pv->filter_entry,
					GTK_ENTRY_ICON_PRIMARY, FALSE);
	gtk_entry_set_icon_activatable (self->pv->filter_entry,
					GTK_ENTRY_ICON_SECONDARY, TRUE);

	gtk_entry_set_width_chars (self->pv->filter_entry, 30);

	g_signal_connect (self->pv->filter_entry, "icon-release",
			  G_CALLBACK (on_clear_clicked), NULL);
	
	/* For the filtering */
	g_signal_connect_object (GTK_EDITABLE (self->pv->filter_entry), "changed", 
	                         G_CALLBACK (on_filter_changed), self, 0);
	
	/* Initialize the tabs, and associate them up */
	initialize_tab (self, "pub-key-tab", TAB_PUBLIC, "pub-key-list", &pred_public);
	initialize_tab (self, "sec-key-tab", TAB_PRIVATE, "sec-key-list", &pred_private);
	pred_password.tag = SEAHORSE_GKR_TYPE;
	initialize_tab (self, "password-tab", TAB_PASSWORD, "password-list", &pred_password);
	
	/* Set focus to the current key list */
	widget = GTK_WIDGET (get_current_view (self));
	gtk_widget_grab_focus (widget);

	g_signal_emit_by_name (self, "selection-changed");

	/* To avoid flicker */
	seahorse_widget_show (SEAHORSE_WIDGET (SEAHORSE_VIEWER (self)));
	
	/* Setup drops */
	gtk_drag_dest_set (GTK_WIDGET (seahorse_viewer_get_window (SEAHORSE_VIEWER (self))), 
	                   GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
	targets = gtk_target_list_new (NULL, 0);
	gtk_target_list_add_uri_targets (targets, TARGETS_URIS);
	gtk_target_list_add_text_targets (targets, TARGETS_PLAIN);
	gtk_drag_dest_set_target_list (GTK_WIDGET (seahorse_viewer_get_window (SEAHORSE_VIEWER (self))), targets);
	
	g_signal_connect_object (seahorse_viewer_get_window (SEAHORSE_VIEWER (self)), "drag-data-received", 
	                         G_CALLBACK (on_target_drag_data_received), self, 0);
	
	/* To show first time dialog */
	g_timeout_add_seconds (1, (GSourceFunc)on_first_timer, self);
	
	g_signal_connect (seahorse_context_instance (), "refreshing", G_CALLBACK (on_refreshing), self);

	return G_OBJECT (self);
}

static void
seahorse_key_manager_init (SeahorseKeyManager *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_KEY_MANAGER, SeahorseKeyManagerPrivate);

}

static void
seahorse_key_manager_finalize (GObject *obj)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (obj);
	gint i;
	
	if (self->pv->view_actions)
		g_object_unref (self->pv->view_actions);
	self->pv->view_actions = NULL;
	
	self->pv->filter_entry = NULL;
	
	if (self->pv->tabs) {
		for (i = 0; i < TAB_NUM_TABS; ++i) {
			if (self->pv->tabs[i].store)
				g_object_unref (self->pv->tabs[i].store);
			if (self->pv->tabs[i].objects)
				g_object_unref (self->pv->tabs[i].objects);
		}
		g_free (self->pv->tabs);
		self->pv->tabs = NULL;
	}

	G_OBJECT_CLASS (seahorse_key_manager_parent_class)->finalize (obj);
}

static void
seahorse_key_manager_set_property (GObject *obj, guint prop_id, const GValue *value, 
                           GParamSpec *pspec)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (obj);
	
	switch (prop_id) {
	case PROP_SELECTED:
		seahorse_viewer_set_selected (SEAHORSE_VIEWER (self), g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_key_manager_get_property (GObject *obj, guint prop_id, GValue *value, 
                           GParamSpec *pspec)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (obj);
	
	switch (prop_id) {
	case PROP_SELECTED:
		g_value_set_object (value, seahorse_viewer_get_selected (SEAHORSE_VIEWER (self)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_key_manager_class_init (SeahorseKeyManagerClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    
	seahorse_key_manager_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseKeyManagerPrivate));

	gobject_class->constructor = seahorse_key_manager_constructor;
	gobject_class->finalize = seahorse_key_manager_finalize;
	gobject_class->set_property = seahorse_key_manager_set_property;
	gobject_class->get_property = seahorse_key_manager_get_property;
    
	SEAHORSE_VIEWER_CLASS (klass)->get_selected_objects = seahorse_key_manager_get_selected_objects;
	SEAHORSE_VIEWER_CLASS (klass)->set_selected_objects = seahorse_key_manager_set_selected_objects;
	SEAHORSE_VIEWER_CLASS (klass)->get_selected = seahorse_key_manager_get_selected;
	SEAHORSE_VIEWER_CLASS (klass)->set_selected = seahorse_key_manager_set_selected;

	g_object_class_override_property (G_OBJECT_CLASS (klass), PROP_SELECTED, "selected");
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */


GtkWindow* 
seahorse_key_manager_show (void) 
{
	SeahorseKeyManager *man = g_object_new (SEAHORSE_TYPE_KEY_MANAGER, "name", "key-manager", NULL);
	g_object_ref_sink (man);
	return GTK_WINDOW (seahorse_widget_get_toplevel (SEAHORSE_WIDGET (man)));
}
