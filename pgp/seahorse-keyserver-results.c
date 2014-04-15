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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "seahorse-keyserver-results.h"

#include "seahorse-pgp-backend.h"
#include "seahorse-gpgme-keyring.h"
#include "seahorse-keyserver-search.h"

#include "libseahorse/seahorse-key-manager-store.h"
#include "libseahorse/seahorse-progress.h"
#include "libseahorse/seahorse-util.h"

#include <glib/gi18n.h>

#include <string.h>

enum {
	PROP_0,
	PROP_SEARCH
};

struct _SeahorseKeyserverResultsPrivate {
	char *search_string;
	SeahorsePredicate pred;
	GtkTreeView *view;
	GcrSimpleCollection *collection;
	GtkActionGroup *import_actions;
	SeahorseKeyManagerStore *store;
	GSettings *settings;
};

G_DEFINE_TYPE (SeahorseKeyserverResults, seahorse_keyserver_results, SEAHORSE_TYPE_CATALOG);

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
	if (self->pv->import_actions)
		gtk_action_group_set_sensitive (self->pv->import_actions, rows > 0);
	g_signal_emit_by_name (self, "selection-changed");
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
	GObject *obj;

	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (GTK_IS_TREE_VIEW (view));
	g_return_if_fail (path != NULL);
	g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (column));

	obj = seahorse_key_manager_store_get_object_from_path (view, path);
	if (obj != NULL)
		seahorse_catalog_show_properties (SEAHORSE_CATALOG (self), obj);
}

static gboolean
on_key_list_button_pressed (GtkTreeView* view, GdkEventButton* event, SeahorseKeyserverResults* self)
{
	g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), FALSE);
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), FALSE);
	if (event->button == 3)
		seahorse_catalog_show_context_menu (SEAHORSE_CATALOG (self),
		                                   SEAHORSE_CATALOG_MENU_OBJECT,
		                                   event->button, event->time);
	return FALSE;
}

static gboolean
on_key_list_popup_menu (GtkTreeView* view, SeahorseKeyserverResults* self)
{
	GList *objects;

	g_return_val_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self), FALSE);
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), FALSE);

	objects = seahorse_catalog_get_selected_objects (SEAHORSE_CATALOG (self));
	if (objects != NULL)
		seahorse_catalog_show_context_menu (SEAHORSE_CATALOG (self),
		                                   SEAHORSE_CATALOG_MENU_OBJECT,
		                                   0, gtk_get_current_event_time ());
	g_list_free (objects);
	return TRUE;
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
	gtk_widget_destroy (GTK_WIDGET (self));
}

static void
on_remote_find (GtkAction* action, SeahorseKeyserverResults* self)
{
	g_return_if_fail (SEAHORSE_IS_KEYSERVER_RESULTS (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	seahorse_keyserver_search_show (seahorse_catalog_get_window (SEAHORSE_CATALOG (self)));
}

static void
on_import_complete (GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
	SeahorseCatalog *self = SEAHORSE_CATALOG (user_data);
	GError *error = NULL;

	if (!seahorse_pgp_backend_transfer_finish (SEAHORSE_PGP_BACKEND (source),
	                                           result, &error))
		seahorse_util_handle_error (&error, seahorse_catalog_get_window (self),
		                            _("Couldn't import keys"));

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
on_key_import_keyring (GtkAction* action, SeahorseCatalog* self)
{
	GCancellable *cancellable;
	SeahorsePgpBackend *backend;
	SeahorseGpgmeKeyring *keyring;
	GList* objects;

	g_return_if_fail (SEAHORSE_IS_CATALOG (self));
	g_return_if_fail (GTK_IS_ACTION (action));

	objects = seahorse_catalog_get_selected_objects (self);
	objects = objects_prune_non_exportable (objects);

	/* No objects, nothing to do */
	if (objects == NULL)
		return;

	cancellable = g_cancellable_new ();
	backend = seahorse_pgp_backend_get ();
	keyring = seahorse_pgp_backend_get_default_keyring (NULL);
	seahorse_pgp_backend_transfer_async (backend, objects, SEAHORSE_PLACE (keyring),
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
	/* TRANSLATORS: The "Remote" menu contains key operations on remote systems. */
	{ "remote-menu", NULL, N_("_Remote") },
	{ "app-close", GTK_STOCK_CLOSE, NULL, "<control>W",
	  N_("Close this window"), G_CALLBACK (on_app_close) },
};

static const GtkActionEntry SERVER_ENTRIES[] = {
	{ "remote-find", GTK_STOCK_FIND, N_("_Find Remote Keys..."), "",
	  N_("Search for keys on a key server"), G_CALLBACK (on_remote_find) }
};

static const GtkActionEntry IMPORT_ENTRIES[] = {
	{ "key-import-keyring", GTK_STOCK_ADD, N_("_Import"), "",
	  N_("Import selected keys to local key ring"), G_CALLBACK (on_key_import_keyring) }
};

static GList *
seahorse_keyserver_results_get_selected_objects (SeahorseCatalog *catalog)
{
	SeahorseKeyserverResults * self = SEAHORSE_KEYSERVER_RESULTS (catalog);
	return seahorse_key_manager_store_get_selected_objects (self->pv->view);
}

static SeahorsePlace *
seahorse_keyserver_results_get_focused_place (SeahorseCatalog *catalog)
{
	SeahorseGpgmeKeyring *keyring;
	keyring = seahorse_pgp_backend_get_default_keyring (NULL);
	return SEAHORSE_PLACE (keyring);
}

static void
seahorse_keyserver_results_constructed (GObject *obj)
{
	SeahorseKeyserverResults *self = SEAHORSE_KEYSERVER_RESULTS (obj);
	GtkActionGroup* actions;
	GtkTreeSelection *selection;
	GtkWindow *window;
	GtkBuilder *builder;
	char* title;

	G_OBJECT_CLASS (seahorse_keyserver_results_parent_class)->constructed (obj);

	if (g_utf8_strlen (self->pv->search_string, -1) == 0) {
		title = g_strdup (_("Remote Keys"));
	} else {
		title = g_strdup_printf (_ ("Remote Keys Containing '%s'"), self->pv->search_string);
	}

	window = seahorse_catalog_get_window (SEAHORSE_CATALOG (self));
	gtk_window_set_default_geometry(window, 640, 476);
	gtk_widget_set_events (GTK_WIDGET (window), GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	gtk_window_set_title (window, title);
	gtk_widget_set_visible (GTK_WIDGET (window), TRUE);
	g_free (title);

	g_signal_connect (window, "delete-event", G_CALLBACK (on_delete_event), self);

	actions = gtk_action_group_new ("general");
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, GENERAL_ENTRIES, G_N_ELEMENTS (GENERAL_ENTRIES), self);
	seahorse_catalog_include_actions (SEAHORSE_CATALOG (self), actions);

	actions = gtk_action_group_new ("keyserver");
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, SERVER_ENTRIES, G_N_ELEMENTS (SERVER_ENTRIES), self);
	seahorse_catalog_include_actions (SEAHORSE_CATALOG (self), actions);

	self->pv->import_actions = gtk_action_group_new ("import");
	gtk_action_group_set_translation_domain (self->pv->import_actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (self->pv->import_actions, IMPORT_ENTRIES, G_N_ELEMENTS (IMPORT_ENTRIES), self);
	g_object_set (gtk_action_group_get_action (self->pv->import_actions, "key-import-keyring"), "is-important", TRUE, NULL);
	seahorse_catalog_include_actions (SEAHORSE_CATALOG (self), self->pv->import_actions);

	/* init key list & selection settings */
	builder = seahorse_catalog_get_builder (SEAHORSE_CATALOG (self));
	self->pv->view = GTK_TREE_VIEW (gtk_builder_get_object (builder, "key_list"));
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
	seahorse_catalog_ensure_updated (SEAHORSE_CATALOG (self));
	gtk_widget_show (GTK_WIDGET (self));

	self->pv->store = seahorse_key_manager_store_new (GCR_COLLECTION (self->pv->collection),
	                                                  self->pv->view,
	                                                  &self->pv->pred,
	                                                  self->pv->settings);
	on_view_selection_changed (selection, self);

	/* Include actions from the backend */
	actions = NULL;
	g_object_get (seahorse_pgp_backend_get (), "actions", &actions, NULL);
	seahorse_catalog_include_actions (SEAHORSE_CATALOG (self), actions);
	g_object_unref (actions);
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
	SeahorseCatalogClass *catalog_class = SEAHORSE_CATALOG_CLASS (klass);

	seahorse_keyserver_results_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseKeyserverResultsPrivate));

	gobject_class->constructed = seahorse_keyserver_results_constructed;
	gobject_class->finalize = seahorse_keyserver_results_finalize;
	gobject_class->set_property = seahorse_keyserver_results_set_property;
	gobject_class->get_property = seahorse_keyserver_results_get_property;

	catalog_class->get_selected_objects = seahorse_keyserver_results_get_selected_objects;
	catalog_class->get_focused_place = seahorse_keyserver_results_get_focused_place;

	g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SEARCH,
	         g_param_spec_string ("search", "search", "search", NULL,
	                              G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
on_search_completed (GObject *source,
                     GAsyncResult *result,
                     gpointer user_data)
{
	SeahorseKeyserverResults *self = SEAHORSE_KEYSERVER_RESULTS (user_data);
	GError *error = NULL;
	GtkWindow *window;

	seahorse_pgp_backend_search_remote_finish (NULL, result, &error);
	if (error != NULL) {
		window = seahorse_catalog_get_window (SEAHORSE_CATALOG (self));
		g_dbus_error_strip_remote_error (error);
		seahorse_util_show_error (GTK_WIDGET (window),
		                          _("The search for keys failed."), error->message);
		g_error_free (error);
	}

	g_object_unref (self);
}
/**
 * seahorse_keyserver_results_show:
 * @search_text: The test to search for
 * @parent: A GTK window as parent (or NULL)
 *
 * Creates a search results window and adds the operation to it's progress status.
 *
 */
void
seahorse_keyserver_results_show (const char* search_text, GtkWindow *parent)
{
	SeahorseKeyserverResults* self;
	GCancellable *cancellable;
	GtkBuilder *builder;

	g_return_if_fail (search_text != NULL);

	self = g_object_new (SEAHORSE_TYPE_KEYSERVER_RESULTS,
	                     "ui-name", "keyserver-results",
	                     "search", search_text,
			     "transient-for", parent,
	                     NULL);

	/* Destorys itself with destroy */
	g_object_ref_sink (self);

	cancellable = g_cancellable_new ();

	seahorse_pgp_backend_search_remote_async (NULL, search_text,
	                                          self->pv->collection,
	                                          cancellable, on_search_completed,
	                                          g_object_ref (self));

	builder = seahorse_catalog_get_builder (SEAHORSE_CATALOG (self));
	seahorse_progress_attach (cancellable, builder);

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
