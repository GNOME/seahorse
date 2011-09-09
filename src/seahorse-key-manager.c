/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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
#include "seahorse-sidebar.h"

#include "seahorse-collection.h"
#include "seahorse-registry.h"
#include "seahorse-progress.h"
#include "seahorse-util.h"

#include "gkr/seahorse-gkr-item.h"

#include <glib/gi18n.h>

void           on_keymanager_row_activated              (GtkTreeView* view,
                                                         GtkTreePath* path,
                                                         GtkTreeViewColumn* column,
                                                         SeahorseKeyManager* self);

gboolean       on_keymanager_key_list_button_pressed    (GtkTreeView* view,
                                                         GdkEventButton* event,
                                                         SeahorseKeyManager* self);

gboolean       on_keymanager_key_list_popup_menu        (GtkTreeView* view,
                                                         SeahorseKeyManager* self);

void           on_keymanager_new_button                 (GtkButton* button,
                                                         SeahorseKeyManager* self);

void           on_keymanager_import_button              (GtkButton* button,
                                                         SeahorseKeyManager* self);

enum {
	PROP_0,
	PROP_SELECTED
};

struct _SeahorseKeyManagerPrivate {
	GtkActionGroup* view_actions;
	GtkEntry* filter_entry;

	GtkTreeView* view;
	GcrCollection *collection; /* owned by the sidebar */
	SeahorseKeyManagerStore* store;

	GSettings *settings;
	gint sidebar_width;
	guint sidebar_width_sig;
};

enum  {
	TARGETS_PLAIN,
	TARGETS_URIS
};

G_DEFINE_TYPE (SeahorseKeyManager, seahorse_key_manager, SEAHORSE_TYPE_VIEWER);

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static gboolean 
fire_selection_changed (gpointer user_data)
{
	SeahorseKeyManager* self = SEAHORSE_KEY_MANAGER (user_data);

	/* Fire the signal */
	g_signal_emit_by_name (self, "selection-changed");
	
	/* This is called as a one-time idle handler, return FALSE so we don't get run again */
	return FALSE;
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

#if REFACTOR_FIRST
static gboolean 
on_first_timer (SeahorseKeyManager* self) 
{
	GtkWidget* widget;

	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER (self), FALSE);
	
	/* 
	 * Although not all the keys have completed we'll know whether we have 
	 * any or not at this point 
	 */

	if (gcr_collection_get_length (GCR_COLLECTION (self->pv->collection)) == 0) {
		widget = seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "first-time-box");
		gtk_widget_show (widget);
	}
	
	return FALSE;
}
#endif

static void
on_clear_clicked (GtkEntry* entry, GtkEntryIconPosition icon_pos, GdkEvent* event, gpointer user_data)
{
	gtk_entry_set_text (entry, "");
}

static void 
on_filter_changed (GtkEntry* entry, SeahorseKeyManager* self) 
{
	const gchar *text;

	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_ENTRY (entry));

	text = gtk_entry_get_text (entry);
	g_object_set (self->pv->store, "filter", text, NULL);
}

#ifdef REFACTOR_IMPORT

static void 
on_import_complete (GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
	SeahorseKeyManager* self = SEAHORSE_KEY_MANAGER (user_data);
	GError *error = NULL;

	if (!seahorse_source_import_finish (SEAHORSE_SOURCE (source), result, &error)) {
		seahorse_util_handle_error (&error, seahorse_viewer_get_window (SEAHORSE_VIEWER (self)),
		                            "%s", _("Couldn't import keys"));
	} else {
		seahorse_viewer_set_status (SEAHORSE_VIEWER (self), _("Imported keys"));
	}

	g_object_unref (self);
}

static void 
import_files (SeahorseKeyManager* self, const gchar** uris) 
{
	GError *error = NULL;
	GFileInputStream* input;
	GCancellable *cancellable;
	const gchar *uri;
	GString *errmsg;
	GFile* file;

	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	errmsg = g_string_new ("");
	cancellable = g_cancellable_new ();

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

		seahorse_source_import_async (sksrc, G_INPUT_STREAM (input),
		                              cancellable, on_import_complete,
		                              g_object_ref (self));
	}

	seahorse_progress_show (cancellable, _("Importing keys"), TRUE);
	g_object_unref (cancellable);

	if (errmsg->len > 0)
		seahorse_util_show_error (GTK_WIDGET (seahorse_viewer_get_window (SEAHORSE_VIEWER (self))), 
		                          _("Couldn't import keys"), errmsg->str);

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

#endif /* REFACTOR_IMPORT */

static void 
on_key_import_file (GtkAction* action, SeahorseKeyManager* self) 
{
#ifdef REFACTOR_IMPORT
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	import_prompt (self);
#endif
}

G_MODULE_EXPORT void 
on_keymanager_import_button (GtkButton* button, SeahorseKeyManager* self) 
{
#ifdef REFACTOR_IMPORT
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_BUTTON (button));
	import_prompt (self);
#endif
}

#ifdef REFACTOR_IMPORT

static void 
import_text (SeahorseKeyManager* self, const char* text) 
{
	glong len;
	GQuark ktype;
	SeahorseSource* sksrc;
	GMemoryInputStream* input;
	GCancellable *cancellable;

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

	input = G_MEMORY_INPUT_STREAM (g_memory_input_stream_new_from_data (g_strndup (text, len),
	                                                                    len, g_free));

	cancellable = g_cancellable_new ();
	seahorse_source_import_async (sksrc, G_INPUT_STREAM (input), cancellable,
	                              on_import_complete, g_object_ref (self));

	seahorse_progress_show (cancellable, _("Importing Keys"), TRUE);
	g_object_unref (cancellable);

	g_object_unref (input);
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

#endif /* REFACTOR_IMPORT */

static void 
on_key_import_clipboard (GtkAction* action, SeahorseKeyManager* self) 
{
#ifdef REFACTOR_IMPORT
	GdkAtom atom;
	GtkClipboard* board;
	
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	
	atom = gdk_atom_intern ("CLIPBOARD", FALSE);
	board = gtk_clipboard_get (atom);
	gtk_clipboard_request_text (board, (GtkClipboardTextReceivedFunc)on_clipboard_received, self);
#endif
}

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
	g_settings_set_boolean (self->pv->settings, "show-type", gtk_toggle_action_get_active (action));
}


static void 
on_view_expires_activate (GtkToggleAction* action, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));
	g_settings_set_boolean (self->pv->settings, "show-expiry", gtk_toggle_action_get_active (action));
}


static void 
on_view_validity_activate (GtkToggleAction* action, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));
	g_settings_set_boolean (self->pv->settings, "show-validity", gtk_toggle_action_get_active (action));
}

static void 
on_view_trust_activate (GtkToggleAction* action, SeahorseKeyManager* self) 
{
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER (self));
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));
	g_settings_set_boolean (self->pv->settings, "show-trust", gtk_toggle_action_get_active (action));
}

static void
on_manager_settings_changed (GSettings *settings, const gchar *key, gpointer user_data)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (user_data);
	GtkToggleAction* action;
	const gchar* name;

	if (g_str_equal (key, "show-trust"))
		name = "view-trust";
	else if (g_str_equal (key, "show-type"))
		name = "view-type";
	else if (g_str_equal (key, "show-expiry"))
		name = "view-expires";
	else if (g_str_equal (key, "show-validity"))
		name = "view-validity";
	else
		return;

	action = GTK_TOGGLE_ACTION (gtk_action_group_get_action (self->pv->view_actions, name));
	g_return_if_fail (action != NULL);

	gtk_toggle_action_set_active (action, g_settings_get_boolean (settings, key));
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
	return seahorse_key_manager_store_get_selected_objects (self->pv->view);
}

static void 
seahorse_key_manager_set_selected_objects (SeahorseViewer* base, GList* objects) 
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (base);
	seahorse_key_manager_store_set_selected_objects (self->pv->view, objects);

}

static SeahorseObject* 
seahorse_key_manager_get_selected (SeahorseViewer* base) 
{
	SeahorseKeyManager* self = SEAHORSE_KEY_MANAGER (base);
	return seahorse_key_manager_store_get_selected_object (self->pv->view);
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

static gboolean
on_idle_save_sidebar_width (gpointer user_data)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (user_data);

	self->pv->sidebar_width_sig = 0;
	g_settings_set_int (self->pv->settings, "sidebar-width", self->pv->sidebar_width);

	return FALSE;
}

static void
on_sidebar_panes_size_allocate (GtkWidget *widget,
                                GtkAllocation *allocation,
                                gpointer user_data)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (user_data);

	if (self->pv->sidebar_width_sig != 0) {
		g_source_remove (self->pv->sidebar_width_sig);
		self->pv->sidebar_width_sig = 0;
	}

	if (allocation->width != self->pv->sidebar_width && allocation->width > 1) {
		self->pv->sidebar_width = allocation->width;
		self->pv->sidebar_width_sig = g_idle_add (on_idle_save_sidebar_width, self);
	}
}

static GcrCollection *
setup_sidebar (SeahorseKeyManager *self)
{
	SeahorseSidebar *sidebar;
	GtkWidget *widget;

	sidebar = seahorse_sidebar_new ();
	widget = seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "sidebar-area");
	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (sidebar));
	gtk_widget_show (GTK_WIDGET (sidebar));

	self->pv->sidebar_width = g_settings_get_int (self->pv->settings, "sidebar-width");
	widget = seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "sidebar-panes");
	gtk_paned_set_position (GTK_PANED (widget), self->pv->sidebar_width);
	g_signal_connect (sidebar, "size_allocate", G_CALLBACK (on_sidebar_panes_size_allocate), self);

	return seahorse_sidebar_get_collection (sidebar);
}

static void
seahorse_key_manager_constructed (GObject *object)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (object);
	GtkActionGroup* actions;
	GtkToggleAction* action;
	GtkTargetList* targets;
	GtkTreeSelection *selection;
	GtkWidget* widget;

	G_OBJECT_CLASS (seahorse_key_manager_parent_class)->constructed (object);

	self->pv->collection = setup_sidebar (self);

	gtk_window_set_title (seahorse_viewer_get_window (SEAHORSE_VIEWER (self)), _("Passwords and Keys"));
	
	actions = gtk_action_group_new ("general");
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, GENERAL_ENTRIES, G_N_ELEMENTS (GENERAL_ENTRIES), self);
	seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), actions);

	self->pv->view_actions = gtk_action_group_new ("view");
	gtk_action_group_set_translation_domain (self->pv->view_actions, GETTEXT_PACKAGE);
	gtk_action_group_add_toggle_actions (self->pv->view_actions, VIEW_ENTRIES, G_N_ELEMENTS (VIEW_ENTRIES), self);
	action = GTK_TOGGLE_ACTION (gtk_action_group_get_action (self->pv->view_actions, "view-type"));
	gtk_toggle_action_set_active (action, g_settings_get_boolean (self->pv->settings, "show-type"));
	action = GTK_TOGGLE_ACTION (gtk_action_group_get_action (self->pv->view_actions, "view-expires"));
	gtk_toggle_action_set_active (action, g_settings_get_boolean (self->pv->settings, "show-expiry"));
	action = GTK_TOGGLE_ACTION (gtk_action_group_get_action (self->pv->view_actions, "view-trust"));
	gtk_toggle_action_set_active (action, g_settings_get_boolean (self->pv->settings, "show-trust"));
	action = GTK_TOGGLE_ACTION (gtk_action_group_get_action (self->pv->view_actions, "view-validity"));
	gtk_toggle_action_set_active (action, g_settings_get_boolean (self->pv->settings, "show-validity"));
	seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), self->pv->view_actions);
	
	/* Notify us when settings change */
	g_signal_connect_object (self->pv->settings, "changed", G_CALLBACK (on_manager_settings_changed), self, 0);

	/* close event */
	g_signal_connect_object (seahorse_widget_get_toplevel (SEAHORSE_WIDGET (self)), 
	                         "delete-event", G_CALLBACK (on_delete_event), self, 0);

	/* first time signals */
	g_signal_connect_object (seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "import-button"), 
	                         "clicked", G_CALLBACK (on_keymanager_import_button), self, 0);

	g_signal_connect_object (seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "new-button"), 
	                         "clicked", G_CALLBACK (on_keymanager_new_button), self, 0);

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
				box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
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


	/* Init key list & selection settings */
	self->pv->view = GTK_TREE_VIEW (seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "key-list"));
	g_return_if_fail (self->pv->view != NULL);

	selection = gtk_tree_view_get_selection (self->pv->view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (selection, "changed", G_CALLBACK (on_view_selection_changed), self);
	gtk_widget_realize (GTK_WIDGET (self->pv->view));

	/* Add new key store and associate it */
	self->pv->store = seahorse_key_manager_store_new (self->pv->collection,
	                                                  self->pv->view,
	                                                  self->pv->settings);

	/* Set focus to the current key list */
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

#ifdef REFACTOR_IMPORT
	g_signal_connect_object (seahorse_viewer_get_window (SEAHORSE_VIEWER (self)), "drag-data-received", 
	                         G_CALLBACK (on_target_drag_data_received), self, 0);
#endif

#ifdef REFACTOR_FIRST
	/* To show first time dialog */
	g_timeout_add_seconds (1, (GSourceFunc)on_first_timer, self);
#endif
}

static void
seahorse_key_manager_init (SeahorseKeyManager *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_KEY_MANAGER, SeahorseKeyManagerPrivate);
	self->pv->settings = g_settings_new ("org.gnome.seahorse.manager");
}

static void
seahorse_key_manager_finalize (GObject *obj)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (obj);

	if (self->pv->sidebar_width_sig != 0) {
		g_source_remove (self->pv->sidebar_width_sig);
		self->pv->sidebar_width_sig = 0;
	}

	if (self->pv->view_actions)
		g_object_unref (self->pv->view_actions);
	self->pv->view_actions = NULL;
	
	self->pv->filter_entry = NULL;

	g_clear_object (&self->pv->settings);

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

	gobject_class->constructed = seahorse_key_manager_constructed;
	gobject_class->finalize = seahorse_key_manager_finalize;
	gobject_class->set_property = seahorse_key_manager_set_property;
	gobject_class->get_property = seahorse_key_manager_get_property;
    
	SEAHORSE_VIEWER_CLASS (klass)->get_selected_objects = seahorse_key_manager_get_selected_objects;
	SEAHORSE_VIEWER_CLASS (klass)->set_selected_objects = seahorse_key_manager_set_selected_objects;
	SEAHORSE_VIEWER_CLASS (klass)->get_selected = seahorse_key_manager_get_selected;
	SEAHORSE_VIEWER_CLASS (klass)->set_selected = seahorse_key_manager_set_selected;

	g_object_class_override_property (gobject_class, PROP_SELECTED, "selected");
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */


SeahorseWidget *
seahorse_key_manager_show (void)
{
	SeahorseKeyManager *self;

	self = g_object_new (SEAHORSE_TYPE_KEY_MANAGER,
	                     "name", "key-manager",
	                     NULL);

	g_object_ref_sink (self);

	return SEAHORSE_WIDGET (self);
}
