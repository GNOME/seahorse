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

#include "seahorse-pgp-backend.h"
#include "seahorse-gpgme-keyring.h"
#include "seahorse-keyserver-search.h"
#include "seahorse-keyserver-results.h"

#include "seahorse-key-manager-store.h"
#include "seahorse-progress.h"
#include "seahorse-util.h"

#include <glib/gi18n.h>

#include <string.h>

gboolean              on_key_list_button_pressed           (GtkTreeView* view,
                                                            GdkEventButton* event,
                                                            SeahorseKeyserverResults* self);

gboolean              on_key_list_popup_menu               (GtkTreeView* view,
                                                            SeahorseKeyserverResults* self);

enum {
	PROP_0,
	PROP_SEARCH,
	PROP_SELECTED
};

struct _SeahorseKeyserverResultsPrivate {
	char *search_string;
	GtkTreeView *view;
	GcrSimpleCollection *collection;
	GtkActionGroup *import_actions;
	SeahorseKeyManagerStore *store;
	GSettings *settings;
};

G_DEFINE_TYPE (SeahorseKeyserverResults, seahorse_keyserver_results, SEAHORSE_TYPE_VIEWER);

/* -----------------------------------------------------------------------------
 * INTERNAL
 */


static gboolean
fire_selection_changed (SeahorseKeyserverResults* self)
{
	gint rows;
	GtkTreeSelection* selection;
	g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), FALSE);

	selection = gtk_tree_view_get_selection (self->pv->view);
	rows = gtk_tree_selection_count_selected_rows (selection);
	seahorse_viewer_set_numbered_status (SEAHORSE_VIEWER (self), ngettext ("Selected %d key", "Selected %d keys", rows), rows);
	if (self->pv->import_actions)
		gtk_action_group_set_sensitive (self->pv->import_actions, rows > 0);
	g_signal_emit_by_name (G_OBJECT (SEAHORSE_VIEW (self)), "selection-changed");
	return FALSE;
}

/**
* selection:
* self: the results object
*
* Adds fire_selection_changed as idle function
*
**/
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

G_MODULE_EXPORT gboolean
on_key_list_button_pressed (GtkTreeView* view, GdkEventButton* event, SeahorseKeyserverResults* self)
{
	g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), FALSE);
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), FALSE);
	if (event->button == 3)
		seahorse_viewer_show_context_menu (SEAHORSE_VIEWER (self), event->button, event->time);
	return FALSE;
}

G_MODULE_EXPORT gboolean
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

/**
* action:
* self: The result object to expand the nodes in
*
* Expands all the nodes in the view of the object
*
**/
static void
on_view_expand_all (GtkAction* action, SeahorseKeyserverResults* self)
{
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	gtk_tree_view_expand_all (self->pv->view);
}


/**
* action:
* self: The result object to collapse the nodes in
*
* Collapses all the nodes in the view of the object
*
**/
static void
on_view_collapse_all (GtkAction* action, SeahorseKeyserverResults* self)

{
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	gtk_tree_view_collapse_all (self->pv->view);
}

/**
* action: the closing action or NULL
* self: The SeahorseKeyServerResults widget to destroy
*
* destroys the widget
*
**/
static void
on_app_close (GtkAction* action, SeahorseKeyserverResults* self)
{
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (action == NULL || GTK_IS_ACTION (action));
	seahorse_widget_destroy (SEAHORSE_WIDGET (self));
}

static void
on_remote_find (GtkAction* action, SeahorseKeyserverResults* self)
{
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	seahorse_keyserver_search_show (seahorse_viewer_get_window (SEAHORSE_VIEWER (self)));
}

static void
on_import_complete (GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
	SeahorseViewer *self = SEAHORSE_VIEWER (user_data);
	GError *error = NULL;

	if (!seahorse_pgp_backend_transfer_finish (SEAHORSE_PGP_BACKEND (source),
	                                           result, &error))
		seahorse_util_handle_error (&error, seahorse_viewer_get_window (self),
		                            _("Couldn't import keys"));
	else
		seahorse_viewer_set_status (self, _ ("Imported keys"));

	g_object_unref (self);
}

static GList*
objects_prune_non_exportable (GList *objects)
{
	GList *exportable = NULL;
	GList *l;

	for (l = objects; l; l = g_list_next (l)) {
		if (seahorse_object_get_flags (l->data) & SEAHORSE_FLAG_EXPORTABLE)
			exportable = g_list_append (exportable, l->data);
	}

	g_list_free (objects);
	return exportable;
}

static void
on_key_import_keyring (GtkAction* action, SeahorseViewer* self)
{
	GCancellable *cancellable;
	SeahorsePgpBackend *backend;
	SeahorseGpgmeKeyring *keyring;
	GList* objects;

	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_ACTION (action));

	objects = seahorse_viewer_get_selected_objects (self);
	objects = objects_prune_non_exportable (objects);

	/* No objects, nothing to do */
	if (objects == NULL)
		return;

	cancellable = g_cancellable_new ();
	backend = seahorse_pgp_backend_get ();
	keyring = seahorse_pgp_backend_get_default_keyring (NULL);
	seahorse_pgp_backend_transfer_async (backend, objects, SEAHORSE_SOURCE (keyring),
	                                     cancellable, on_import_complete, g_object_ref (self));
	seahorse_progress_show (cancellable, _ ("Importing keys from key servers"), TRUE);
	g_object_unref (cancellable);

	g_list_free (objects);
}

/**
* widget: sending widget
* event: ignored
* self: The SeahorseKeyserverResults widget to destroy
*
* When this window closes we quit seahorse
*
* Returns TRUE on success
**/
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
	{ "app-close", GTK_STOCK_CLOSE, NULL, "<control>W",
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

static const GtkActionEntry IMPORT_ENTRIES[] = {
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


/**
* type: The type identifying this object
* n_props: Number of properties
* props: Properties
*
* Creates a new SeahorseKeyserverResults object, shows the resulting window
*
* Returns The SeahorseKeyserverResults object as GObject
**/
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

	self->pv->import_actions = gtk_action_group_new ("import");
	gtk_action_group_set_translation_domain (self->pv->import_actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (self->pv->import_actions, IMPORT_ENTRIES, G_N_ELEMENTS (IMPORT_ENTRIES), self);
	g_object_set (gtk_action_group_get_action (self->pv->import_actions, "key-import-keyring"), "is-important", TRUE, NULL);
	seahorse_viewer_include_actions (SEAHORSE_VIEWER (self), self->pv->import_actions);

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

	self->pv->store = seahorse_key_manager_store_new (GCR_COLLECTION (self->pv->collection),
	                                                  self->pv->view,
	                                                  self->pv->settings);
	on_view_selection_changed (selection, self);

	return G_OBJECT (self);
}

/**
* self: The SeahorseKeyserverResults object to init
*
* Inits the results object
*
**/
static void
seahorse_keyserver_results_init (SeahorseKeyserverResults *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_KEYSERVER_RESULTS, SeahorseKeyserverResultsPrivate);
	self->pv->settings = g_settings_new ("org.gnome.seahorse.manager");
	self->pv->collection = GCR_SIMPLE_COLLECTION (gcr_simple_collection_new ());
}

/**
* obj: SeahorseKeyserverResults object
*
* Finalize the results object
*
**/
static void
seahorse_keyserver_results_finalize (GObject *obj)
{
	SeahorseKeyserverResults *self = SEAHORSE_KEYSERVER_RESULTS (obj);

	g_free (self->pv->search_string);
	self->pv->search_string = NULL;

	g_clear_object (&self->pv->import_actions);

	g_clear_object (&self->pv->collection);

	if (self->pv->store)
		g_object_unref (self->pv->store);
	self->pv->store = NULL;

	if (self->pv->view)
		gtk_tree_view_set_model (self->pv->view, NULL);
	self->pv->view = NULL;

	g_clear_object (&self->pv->settings);
	g_clear_object (&self->pv->collection);

	G_OBJECT_CLASS (seahorse_keyserver_results_parent_class)->finalize (obj);
}

/**
* obj: The SeahorseKeyserverResults object to set
* prop_id: Property to set
* value: Value of this property
* pspec: GParamSpec for the warning
*
* Supported properties are PROP_SEARCH and PROP_SELECTED
*
**/
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

/**
* obj: The SeahorseKeyserverResults object to get properties from
* prop_id: the ide of the property to get
* value: Value returned
* pspec: GParamSpec for the warning
*
* Supported properties: PROP_SEARCH and PROP_SELECTED
*
**/
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

/**
* klass: The SeahorseKeyserverResultsClass
*
* Inits the class
*
**/
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

static void
on_search_completed (GObject *source,
                     GAsyncResult *result,
                     gpointer user_data)
{
	SeahorseKeyserverResults *self = SEAHORSE_KEYSERVER_RESULTS (user_data);
	GError *error = NULL;

	seahorse_pgp_backend_search_remote_finish (NULL, result, &error);
	if (error != NULL) {
		seahorse_viewer_set_status (SEAHORSE_VIEWER (self), error->message);
		g_error_free (error);
	}

	g_object_unref (self);
}
/**
 * seahorse_keyserver_results_show:
 * @parent: A GTK window as parent (or NULL)
 * @search_text: The test to search for
 *
 * Creates a search results window and adds the operation to it's progress status.
 *
 */
void
seahorse_keyserver_results_show (const char* search_text)
{
	SeahorseKeyserverResults* self;
	GCancellable *cancellable;

	g_return_if_fail (search_text != NULL);

	self = g_object_new (SEAHORSE_TYPE_KEYSERVER_RESULTS,
	                     "name", "keyserver-results",
	                     "search", search_text,
	                     NULL);

	/* Destorys itself with destroy */
	g_object_ref_sink (self);

	cancellable = g_cancellable_new ();

	seahorse_pgp_backend_search_remote_async (NULL, search_text,
	                                          self->pv->collection,
	                                          cancellable, on_search_completed,
	                                          g_object_ref (self));

	seahorse_progress_attach (cancellable, SEAHORSE_WIDGET (self));

	g_object_unref (cancellable);
}

/**
 * seahorse_keyserver_results_get_search:
 * @self:  SeahorseKeyserverResults object to get the search string from
 *
 *
 *
 * Returns: The search string in from the results
 */
const gchar*
seahorse_keyserver_results_get_search (SeahorseKeyserverResults* self)
{
	g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), NULL);
	return self->pv->search_string;
}
