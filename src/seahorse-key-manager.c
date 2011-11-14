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
#include "seahorse-import-dialog.h"
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

enum {
	SHOW_ANY,
	SHOW_PERSONAL,
	SHOW_TRUSTED,
};

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

struct _SeahorseKeyManagerPrivate {
	GtkActionGroup* view_actions;
	GtkEntry* filter_entry;
	SeahorsePredicate pred;
	SeahorseSidebar *sidebar;

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
	GObject* obj;

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
		seahorse_viewer_show_context_menu (SEAHORSE_VIEWER (self),
		                                   SEAHORSE_VIEWER_MENU_OBJECT,
		                                   event->button, event->time);

	return FALSE;
}

G_MODULE_EXPORT gboolean
on_keymanager_key_list_popup_menu (GtkTreeView* view, SeahorseKeyManager* self) 
{
	GList *objects;

	objects = seahorse_viewer_get_selected_objects (SEAHORSE_VIEWER (self));
	if (objects != NULL)
		seahorse_viewer_show_context_menu (SEAHORSE_VIEWER (self),
		                                   SEAHORSE_VIEWER_MENU_OBJECT,
		                                   0, gtk_get_current_event_time ());
	g_list_free (objects);
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

static void 
import_files (SeahorseKeyManager* self,
              const gchar** uris)
{
	GtkDialog *dialog;
	GtkWindow *parent;

	parent = seahorse_viewer_get_window (SEAHORSE_VIEWER (self));
	dialog = seahorse_import_dialog_new (parent);
	seahorse_import_dialog_add_uris (SEAHORSE_IMPORT_DIALOG (dialog), uris);

	gtk_dialog_run (dialog);
	gtk_widget_destroy (GTK_WIDGET (dialog));
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
import_text (SeahorseKeyManager* self,
             const gchar *display_name,
             const char* text)
{
	GtkDialog *dialog;
	GtkWindow *parent;

	parent = seahorse_viewer_get_window (SEAHORSE_VIEWER (self));
	dialog = seahorse_import_dialog_new (parent);
	seahorse_import_dialog_add_text (SEAHORSE_IMPORT_DIALOG (dialog),
	                                 display_name, text);

	gtk_dialog_run (dialog);
	gtk_widget_destroy (GTK_WIDGET (dialog));
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
		import_text (self, _("Dropped text"), (gchar*)text);
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
			import_text (self, _("Clipboard text"), text);
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
on_view_show_changed (GtkRadioAction *action,
                      GtkRadioAction *current,
                      gpointer user_data)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (user_data);
	const gchar *value = "";
	gint radio;

	radio = gtk_radio_action_get_current_value (action);
	switch (radio) {
	case SHOW_PERSONAL:
		self->pv->pred.flags = SEAHORSE_FLAG_PERSONAL;
		value = "personal";
		break;
	case SHOW_TRUSTED:
		self->pv->pred.flags = SEAHORSE_FLAG_TRUSTED;
		value = "trusted";
		break;
	case SHOW_ANY:
		self->pv->pred.flags = 0;
		value = "";
		break;
	}

	seahorse_key_manager_store_refilter (self->pv->store);
	g_settings_set_string (self->pv->settings, "item-filter", value);
}

static void
on_item_filter_changed (GSettings *settings,
                        const gchar *key,
                        gpointer user_data)
{
	GtkAction* action = GTK_ACTION (user_data);
	gchar *value;
	gint radio;

	value = g_settings_get_string (settings, key);
	if (value == NULL || g_str_equal (value, ""))
		radio = SHOW_ANY;
	else if (g_str_equal (value, "personal"))
		radio = SHOW_PERSONAL;
	else if (g_str_equal (value, "trusted"))
		radio = SHOW_TRUSTED;
	else
		action = NULL;
	if (action != NULL)
		gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action), radio);
	g_free (value);
}

#define       SEAHORSE_TYPE_MENU_ACTION          (seahorse_menu_action_get_type ())

GType         seahorse_menu_action_get_type      (void) G_GNUC_CONST;

typedef       GtkAction                          SeahorseMenuAction;

typedef       GtkActionClass                     SeahorseMenuActionClass;

G_DEFINE_TYPE (SeahorseMenuAction, seahorse_menu_action, GTK_TYPE_ACTION);

static void
seahorse_menu_action_init (SeahorseMenuAction *self)
{

}

static void
seahorse_menu_action_class_init (SeahorseMenuActionClass *klass)
{
	GTK_ACTION_CLASS (klass)->toolbar_item_type = GTK_TYPE_MENU_TOOL_BUTTON;
}

static const GtkActionEntry GENERAL_ACTIONS[] = {
	{ "remote-menu", NULL, N_("_Remote") }, 
	{ "new-menu", NULL, N_("_New") },
	{ "app-quit", GTK_STOCK_QUIT, NULL, "<control>Q", 
	  N_("Close this program"), G_CALLBACK (on_app_quit) }, 
	{ "file-new", GTK_STOCK_NEW, N_("_New..."), "<control>N", 
	  N_("Create a new key or item"), G_CALLBACK (on_file_new) },
	{ "new-object", GTK_STOCK_ADD, N_("_New..."), NULL,
	  N_("Add a new key or item"), G_CALLBACK (on_file_new) },
	{ "file-import", GTK_STOCK_OPEN, N_("_Import..."), "<control>I", 
	  N_("Import from a file"), G_CALLBACK (on_key_import_file) }, 
	{ "edit-import-clipboard", GTK_STOCK_PASTE, NULL, "<control>V", 
	  N_("Import from the clipboard"), G_CALLBACK (on_key_import_clipboard) }
};

static const GtkToggleActionEntry SIDEBAR_ACTIONS[] = {
	{ "view-places", NULL, N_("P_laces"), NULL,
	  N_("Show places sidebar"), NULL, FALSE },
};

static const GtkRadioActionEntry VIEW_RADIO_ACTIONS[] = {
	{ "view-personal", NULL, N_("Show _personal"), NULL,
	  N_("Only show personal keys, certificates and passwords"), SHOW_PERSONAL },
	{ "view-trusted", NULL, N_("Show _trusted"), NULL,
	  N_("Only show trusted keys, certificates and passwords"), SHOW_TRUSTED },
	{ "view-any", NULL, N_("_Show _any"), NULL,
	  N_("Show all keys, certificates and passwords"), SHOW_ANY },
};

static GList *
seahorse_key_manager_get_selected_objects (SeahorseViewer* viewer)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (viewer);
	return seahorse_key_manager_store_get_selected_objects (self->pv->view);
}

static GList *
seahorse_key_manager_get_selected_places (SeahorseViewer* viewer)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (viewer);
	return seahorse_sidebar_get_selected_places (self->pv->sidebar);
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

	if (allocation->width != self->pv->sidebar_width && allocation->width > 1) {
		if (self->pv->sidebar_width_sig != 0) {
			g_source_remove (self->pv->sidebar_width_sig);
			self->pv->sidebar_width_sig = 0;
		}

		self->pv->sidebar_width = allocation->width;
		self->pv->sidebar_width_sig = g_idle_add (on_idle_save_sidebar_width, self);
	}
}

static void
on_sidebar_popup_menu (SeahorseSidebar *sidebar,
                       GcrCollection *collection,
                       gpointer user_data)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (user_data);
	const gchar *name;

	name = G_OBJECT_TYPE_NAME (collection);
	seahorse_viewer_show_context_menu (SEAHORSE_VIEWER (self), name,
	                                   0, gtk_get_current_event_time ());
}

static GcrCollection *
setup_sidebar (SeahorseKeyManager *self)
{
	GtkWidget *area, *panes;
	GtkActionGroup *actions;
	GtkAction *action;
	GList *backends, *l;

	self->pv->sidebar = seahorse_sidebar_new ();
	area = seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "sidebar-area");
	gtk_container_add (GTK_CONTAINER (area), GTK_WIDGET (self->pv->sidebar));
	gtk_widget_show (GTK_WIDGET (self->pv->sidebar));

	actions = gtk_action_group_new ("sidebar");
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_toggle_actions (actions, SIDEBAR_ACTIONS,
	                                     G_N_ELEMENTS (SIDEBAR_ACTIONS), self);
	action = gtk_action_group_get_action (actions, "view-places");
	g_object_bind_property (action, "active",
	                        area, "visible",
	                        G_BINDING_DEFAULT);
	g_object_bind_property (action, "active",
	                        self->pv->sidebar, "combined",
	                        G_BINDING_INVERT_BOOLEAN);
	g_settings_bind (self->pv->settings, "sidebar-visible",
	                 action, "active",
	                 G_SETTINGS_BIND_DEFAULT);
	seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), actions);
	g_object_unref (actions);

	g_settings_bind (self->pv->settings, "places-selected",
	                 self->pv->sidebar, "selected-uris",
	                 G_SETTINGS_BIND_DEFAULT);

	self->pv->sidebar_width = g_settings_get_int (self->pv->settings, "sidebar-width");
	panes = seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "sidebar-panes");
	gtk_paned_set_position (GTK_PANED (panes), self->pv->sidebar_width);
	g_signal_connect (self->pv->sidebar, "size_allocate", G_CALLBACK (on_sidebar_panes_size_allocate), self);
	g_signal_connect (self->pv->sidebar, "context-menu", G_CALLBACK (on_sidebar_popup_menu), self);

	backends = seahorse_sidebar_get_backends (self->pv->sidebar);
	for (l = backends; l != NULL; l = g_list_next (l)) {
		actions = NULL;
		g_object_get (l->data, "actions", &actions, NULL);
		if (actions != NULL) {
			seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), actions);
			g_object_unref (actions);
		}
	}

	return seahorse_sidebar_get_collection (self->pv->sidebar);
}

static void
seahorse_key_manager_constructed (GObject *object)
{
	SeahorseKeyManager *self = SEAHORSE_KEY_MANAGER (object);
	GtkActionGroup* actions;
	GtkTargetList* targets;
	GtkTreeSelection *selection;
	GtkWidget* widget;
	GtkAction *action;

	G_OBJECT_CLASS (seahorse_key_manager_parent_class)->constructed (object);

	self->pv->collection = setup_sidebar (self);

	gtk_window_set_title (seahorse_viewer_get_window (SEAHORSE_VIEWER (self)), _("Passwords and Keys"));

	actions = gtk_action_group_new ("general");
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, GENERAL_ACTIONS, G_N_ELEMENTS (GENERAL_ACTIONS), self);
	seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), actions);

	self->pv->view_actions = gtk_action_group_new ("view");
	gtk_action_group_set_translation_domain (self->pv->view_actions, GETTEXT_PACKAGE);
	gtk_action_group_add_radio_actions (self->pv->view_actions, VIEW_RADIO_ACTIONS,
	                                    G_N_ELEMENTS (VIEW_RADIO_ACTIONS), SHOW_PERSONAL,
	                                    G_CALLBACK (on_view_show_changed), self);
	action = gtk_action_group_get_action (self->pv->view_actions, "view-personal");
	seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), self->pv->view_actions);

	/* Notify us when settings change */
	g_signal_connect_object (self->pv->settings, "changed::item-filter",
	                         G_CALLBACK (on_item_filter_changed), action, 0);

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

				self->pv->filter_entry = GTK_ENTRY (gtk_entry_new ());
				gtk_entry_set_placeholder_text (self->pv->filter_entry, _("Filter"));
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
	                                                  &self->pv->pred,
	                                                  self->pv->settings);

	/* Set focus to the current key list */
	gtk_widget_grab_focus (GTK_WIDGET (self->pv->view));
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
seahorse_key_manager_class_init (SeahorseKeyManagerClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (SeahorseKeyManagerPrivate));

	gobject_class->constructed = seahorse_key_manager_constructed;
	gobject_class->finalize = seahorse_key_manager_finalize;

	SEAHORSE_VIEWER_CLASS (klass)->get_selected_objects = seahorse_key_manager_get_selected_objects;
	SEAHORSE_VIEWER_CLASS (klass)->get_selected_places = seahorse_key_manager_get_selected_places;
}

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
