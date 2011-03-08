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

#include "seahorse-commands.h"
#include "seahorse-object.h"
#include "seahorse-preferences.h"
#include "seahorse-progress.h"
#include "seahorse-util.h"
#include "seahorse-view.h"
#include "seahorse-viewer.h"

#include "common/seahorse-object-list.h"
#include "common/seahorse-registry.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

enum {
	PROP_0,
	PROP_SELECTED,
	PROP_CURRENT_SET,
	PROP_WINDOW
};

typedef struct _ViewerPredicate {
	SeahorseObjectPredicate pred;
	gboolean is_commands;
	GObject *commands_or_actions;
} ViewerPredicate;

struct _SeahorseViewerPrivate {
	GtkUIManager *ui_manager;
	GtkActionGroup *object_actions;
	GtkActionGroup *export_actions;
	GtkActionGroup *import_actions;
	GtkAction *delete_action;
	GArray *predicates;
	GList *all_commands;
};

static void seahorse_viewer_implement_view (SeahorseViewIface *iface);
G_DEFINE_TYPE_EXTENDED (SeahorseViewer, seahorse_viewer, SEAHORSE_TYPE_WIDGET, 0,
                        G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_VIEW, seahorse_viewer_implement_view));

#define SEAHORSE_VIEWER_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_TYPE_VIEWER, SeahorseViewerPrivate))

/* Predicates which control export and delete commands, inited in class_init */
static SeahorseObjectPredicate exportable_predicate = { 0, };
static SeahorseObjectPredicate deletable_predicate = { 0, };
static SeahorseObjectPredicate importable_predicate = { 0, };
static SeahorseObjectPredicate remote_predicate = { 0, };

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

typedef gboolean (*ForEachCommandsFunc) (SeahorseViewer *self, SeahorseCommands *commands, 
                                         SeahorseObjectPredicate *predicate, gpointer user_data);

static void
for_each_commands (SeahorseViewer *self, ForEachCommandsFunc func, gpointer user_data)
{
	SeahorseViewerPrivate *pv = SEAHORSE_VIEWER_GET_PRIVATE (self);
	ViewerPredicate *predicate;
	guint i;
	
	for (i = 0; i < pv->predicates->len; ++i)
	{
		predicate = &g_array_index (pv->predicates, ViewerPredicate, i);
		if (predicate->is_commands) {
			if (!(func) (self, SEAHORSE_COMMANDS (predicate->commands_or_actions), 
			            &predicate->pred, user_data))
				return;
		}
	}
}

G_MODULE_EXPORT void
on_app_preferences (GtkAction* action, SeahorseViewer* self) 
{
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_ACTION (action));

	seahorse_preferences_show (seahorse_view_get_window (SEAHORSE_VIEW (self)), NULL);
}

static void 
on_app_about (GtkAction* action, SeahorseViewer* self) 
{
	GtkAboutDialog *about;
	
	const gchar *authors[] = { 
		"Jacob Perkins <jap1@users.sourceforge.net>",
		"Jose Carlos Garcia Sogo <jsogo@users.sourceforge.net>",
		"Jean Schurger <yshark@schurger.org>",
		"Stef Walter <stef@memberwebs.com>",
		"Adam Schreiber <sadam@clemson.edu>",
		"",
		N_("Contributions:"),
		"Albrecht Dreß <albrecht.dress@arcor.de>",
		"Jim Pharis <binbrain@gmail.com>",
		NULL
	};
	
	const gchar *documenters[] = {
		"Jacob Perkins <jap1@users.sourceforge.net>",
		"Adam Schreiber <sadam@clemson.edu>",
		"Milo Casagrande <milo_casagrande@yahoo.it>",
		NULL
	};
	
	const gchar *artists[] = {
		"Jacob Perkins <jap1@users.sourceforge.net>",
		"Stef Walter <stef@memberwebs.com>",
		NULL
	};

	about = GTK_ABOUT_DIALOG (gtk_about_dialog_new ());
	gtk_about_dialog_set_artists (about, artists);
	gtk_about_dialog_set_authors (about, authors);
	gtk_about_dialog_set_documenters (about, documenters);
	gtk_about_dialog_set_version (about, VERSION);
	gtk_about_dialog_set_comments (about, _("Encryption Key Manager"));
	gtk_about_dialog_set_copyright (about, "Copyright \xc2\xa9 2002 - 2010 Seahorse Project");
	gtk_about_dialog_set_translator_credits (about, _("translator-credits"));
	gtk_about_dialog_set_logo_icon_name (about, "seahorse");
	gtk_about_dialog_set_website (about, "http://www.gnome.org/projects/seahorse");
	gtk_about_dialog_set_website_label (about, _("Seahorse Project Homepage"));
	
	g_signal_connect (about, "response", G_CALLBACK (gtk_widget_hide), NULL);
	gtk_window_set_transient_for (GTK_WINDOW (about), seahorse_viewer_get_window (self));
	
	gtk_dialog_run (GTK_DIALOG (about));
	gtk_widget_destroy (GTK_WIDGET (about));
}

static void 
on_help_show (GtkAction* action, SeahorseViewer* self) 
{
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	seahorse_widget_show_help (SEAHORSE_WIDGET (self));
}

static const GtkActionEntry UI_ENTRIES[] = {

	/* Top menu items */
	{ "file-menu", NULL, N_("_File") },
	{ "edit-menu", NULL, N_("_Edit") },
	{ "view-menu", NULL, N_("_View") },
	{ "help-menu", NULL, N_("_Help") },
#if defined (WITH_KEYSERVER) || defined (WITH_SHARING)
	{ "app-preferences", GTK_STOCK_PREFERENCES, N_("Prefere_nces"), NULL,
	  N_("Change preferences for this program"), G_CALLBACK (on_app_preferences) },
#endif
	{ "app-about", GTK_STOCK_ABOUT, NULL, NULL, 
	  N_("About this program"), G_CALLBACK (on_app_about) }, 
	{ "help-show", GTK_STOCK_HELP, N_("_Contents"), "F1",
	  N_("Show Seahorse help"), G_CALLBACK (on_help_show) } 
};

static void 
on_key_properties (GtkAction* action, SeahorseViewer* self) 
{
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	
	if (seahorse_viewer_get_selected (self) != NULL)
		seahorse_viewer_show_properties (self, seahorse_viewer_get_selected (self));
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

static GList*
filter_matching_objects (SeahorseObjectPredicate *pred, GList** all_objects)
{
	GList *results, *next, *here;
	SeahorseObject *object;
	
	results = NULL;
	
	next = *all_objects;
	while ((here = next) != NULL) {
		g_return_val_if_fail (SEAHORSE_IS_OBJECT (here->data), NULL);
		object = SEAHORSE_OBJECT (here->data);
		next = g_list_next (here);

		/* If it matches then separate it */
		if (seahorse_object_predicate_match (pred, object)) {
			results = g_list_append (results, object);
			*all_objects = g_list_delete_link (*all_objects, here);
		}
	}
	
	return results;
}

static gboolean
has_matching_objects (SeahorseObjectPredicate *pred, GList *objects)
{
	SeahorseObject *object;
	GList *l;

	for (l = objects; l; l = g_list_next (l)) {
		g_return_val_if_fail (SEAHORSE_IS_OBJECT (l->data), FALSE);
		object = SEAHORSE_OBJECT (l->data);

		/* If it matches then separate it */
		if (seahorse_object_predicate_match (pred, object)) 
			return TRUE;
	}
	
	return FALSE;
}

static void 
on_export_done (SeahorseOperation* op, SeahorseViewer* self) 
{
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (SEAHORSE_IS_OPERATION (op));
	
	if (!seahorse_operation_is_successful (op))
		seahorse_operation_display_error (op, _ ("Couldn't export keys"), 
		                                  GTK_WIDGET (seahorse_view_get_window (SEAHORSE_VIEW (self))));
}

static void 
on_key_export_file (GtkAction* action, SeahorseViewer* self) 
{
	GError *error = NULL;
	GList *objects;
	GtkDialog *dialog;
	gchar *uri, *unesc_uri;
	
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	
	objects = seahorse_viewer_get_selected_objects (self);
	objects = objects_prune_non_exportable (objects);
	if (objects == NULL)
		return;

	dialog = seahorse_util_chooser_save_new (_("Export public key"), 
	                                         seahorse_view_get_window (SEAHORSE_VIEW (self)));
	seahorse_util_chooser_show_key_files (dialog);
	seahorse_util_chooser_set_filename_full (dialog, objects);
	uri = seahorse_util_chooser_save_prompt (dialog);
	if (uri != NULL) {
		GFile* file;
		GOutputStream* output;
		SeahorseOperation* op;
		
		file = g_file_new_for_uri (uri);
		output = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE, 0, NULL, &error));
		if (output == NULL) {
		    unesc_uri = g_uri_unescape_string (seahorse_util_uri_get_last (uri), NULL);
			seahorse_util_handle_error (error, _ ("Couldn't export key to \"%s\""), 
			                            unesc_uri, NULL);
			g_clear_error (&error);
			g_free (unesc_uri);
		} else {
			op = seahorse_source_export_objects (objects, output);
			seahorse_progress_show (op, _("Exporting keys"), TRUE);
			seahorse_operation_watch (op, (SeahorseDoneFunc)on_export_done, self, NULL, NULL);
			g_object_unref (op);
		}
		

		g_object_unref (file);
		g_object_unref (output);
		g_free (uri);
	}
}

static void 
on_copy_complete (SeahorseOperation* op, SeahorseViewer* self) 
{
	GMemoryOutputStream* output;
	const gchar* text;
	guint size;
	GdkAtom atom;
	GtkClipboard* board;
	
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (SEAHORSE_IS_OPERATION (op));
	
	if (!seahorse_operation_is_successful (op)) {
		seahorse_operation_display_error (op, _ ("Couldn't retrieve data from key server"), 
		                                  GTK_WIDGET (seahorse_view_get_window (SEAHORSE_VIEW (self))));
		return;
	}
	
	output = G_MEMORY_OUTPUT_STREAM (seahorse_operation_get_result (op));
	g_return_if_fail (G_IS_MEMORY_OUTPUT_STREAM (output));
	
	text = g_memory_output_stream_get_data (output);
	g_return_if_fail (text != NULL);
	
	size = seahorse_util_memory_output_length (output);
	g_return_if_fail (size >= 0);
	
	atom = gdk_atom_intern ("CLIPBOARD", FALSE);

	board = gtk_clipboard_get (atom);
	gtk_clipboard_set_text (board, text, (gint)size);
	/* Translators: "Copied" is a verb (used as a status indicator), not an adjective for the word "keys" */
	seahorse_viewer_set_status (self, _ ("Copied keys"));
}

static void 
on_key_export_clipboard (GtkAction* action, SeahorseViewer* self) 
{
	GList* objects;
	GOutputStream* output;
	SeahorseOperation* op;
	
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	
	objects = seahorse_viewer_get_selected_objects (self);
	objects = objects_prune_non_exportable (objects);
	if (objects == NULL)
		return;

	output = G_OUTPUT_STREAM (g_memory_output_stream_new (NULL, 0, g_realloc, g_free));
	op = seahorse_source_export_objects (objects, output);
	seahorse_progress_show (op, _ ("Retrieving keys"), TRUE);
	seahorse_operation_watch (op, (SeahorseDoneFunc)on_copy_complete, self, NULL, NULL);
	
	g_list_free (objects);
	g_object_unref (output);
	g_object_unref (op);
}

static void 
on_delete_complete (SeahorseOperation* op, SeahorseViewer* self) 
{
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (SEAHORSE_IS_OPERATION (op));
	
	if (!seahorse_operation_is_successful (op))
		seahorse_operation_display_error (op, _ ("Couldn't delete."), 
		                                  GTK_WIDGET (seahorse_view_get_window (SEAHORSE_VIEW (self))));
}

static gboolean
delete_objects_for_selected (SeahorseViewer *self, SeahorseCommands *commands, 
                             SeahorseObjectPredicate *pred, gpointer user_data)
{
	SeahorseOperation* op;
	GList **all_objects;
	GList *objects;
	
	all_objects = (GList**)user_data;
	
	/* Stop the enumeration if nothing still selected */
	if (!*all_objects)
		return FALSE;
	
	objects = filter_matching_objects (pred, all_objects);
	
	/* No objects matched this predicate? */
	if (!objects) 
		return TRUE;
	
	/* Indicate to our users what is being operated on */
	seahorse_viewer_set_selected_objects (self, objects);
	
	op = seahorse_commands_delete_objects (commands, objects);
	g_list_free (objects);
	
	/* Did the user cancel? */
	if (op == NULL)
		return FALSE;

	seahorse_progress_show (op, _ ("Deleting..."), TRUE);
	seahorse_operation_watch (op, (SeahorseDoneFunc)on_delete_complete, self, NULL, NULL);
	g_object_unref (op);
	
	return TRUE;
}

static void
on_key_delete (GtkAction* action, SeahorseViewer* self) 
{
	GList *objects = NULL;
	GList *all_objects = NULL;
	GList *l;

	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_ACTION (action));

	all_objects = seahorse_viewer_get_selected_objects (self);
	objects = filter_matching_objects (&deletable_predicate, &all_objects);
	g_list_free (all_objects);

	/* Check for private objects */
	for (l = objects; l; l = g_list_next (l)) {
		SeahorseObject* object = l->data;
		
		if (seahorse_object_get_usage (object) == SEAHORSE_USAGE_PRIVATE_KEY) {
			gchar* prompt = g_strdup_printf (_("%s is a private key. Are you sure you want to proceed?"), 
			                                 seahorse_object_get_label (object));
			if (!seahorse_util_prompt_delete (prompt, GTK_WIDGET (seahorse_view_get_window (SEAHORSE_VIEW (self))))) {
				g_free (prompt);
				g_list_free (objects);
				return;
			}
		}
	}
	
	/* Go through the list of predicates, and call each one */
	for_each_commands (self, delete_objects_for_selected, &objects);

	g_list_free (objects);
}

static void 
imported_keys (SeahorseOperation* op, SeahorseViewer* self) 
{
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (SEAHORSE_IS_OPERATION (op));
	
	if (!seahorse_operation_is_successful (op)) {
		seahorse_operation_display_error (op, _ ("Couldn't import keys"), 
		                                  GTK_WIDGET (seahorse_viewer_get_window (self)));
		return;
	}
	
	seahorse_viewer_set_status (self, _ ("Imported keys"));
}

static void 
on_key_import_keyring (GtkAction* action, SeahorseViewer* self) 
{
	GList* objects;
	SeahorseOperation* op;

	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_ACTION (action));

	objects = seahorse_viewer_get_selected_objects (self);
	objects = objects_prune_non_exportable (objects);
		
	/* No objects, nothing to do */
	if (objects == NULL) 
		return;

	op = seahorse_context_transfer_objects (seahorse_context_for_app (), objects, NULL);
	seahorse_progress_show (op, _ ("Importing keys from key servers"), TRUE);
	seahorse_operation_watch (op, (SeahorseDoneFunc)imported_keys, self, NULL, NULL);
	
	g_object_unref (op);
	g_list_free (objects);
}


static gboolean
show_properties_for_selected (SeahorseViewer *self, SeahorseCommands *commands, 
                              SeahorseObjectPredicate *pred, gpointer user_data)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (user_data), FALSE);
	
	/* If not mactched, then continue enumeration */
	if (!seahorse_object_predicate_match (pred, user_data))
		return TRUE;
	
	seahorse_commands_show_properties (commands, user_data);
	
	/* Stop enumeration */
	return FALSE;
}
		
static const GtkActionEntry KEY_ENTRIES[] = {
	{ "show-properties", GTK_STOCK_PROPERTIES, NULL, NULL,
	  N_("Show properties"), G_CALLBACK (on_key_properties) },
	{ "edit-delete", GTK_STOCK_DELETE, NC_("This text refers to deleting an item from its type's backing store.", "_Delete"), NULL,
	  N_("Delete selected items"), G_CALLBACK (on_key_delete) }
};

static const GtkActionEntry EXPORT_ENTRIES[] = {
	{ "file-export", GTK_STOCK_SAVE_AS, N_("E_xport..."), NULL,
	  N_("Export to a file"), G_CALLBACK (on_key_export_file) },
	{ "edit-export-clipboard", GTK_STOCK_COPY, NULL, "<control>C",
	  N_("Copy to the clipboard"), G_CALLBACK (on_key_export_clipboard) }
};

static const GtkActionEntry IMPORT_ENTRIES[] = {
	{ "key-import-keyring", GTK_STOCK_ADD, N_("_Import"), "", 
	  N_("Import selected keys to local key ring"), G_CALLBACK (on_key_import_keyring) }
};
		
static void 
include_basic_actions (SeahorseViewer* self) 
{
	SeahorseViewerPrivate *pv = SEAHORSE_VIEWER_GET_PRIVATE (self);
	GtkActionGroup* actions;

	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	
	actions = gtk_action_group_new ("main");
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, UI_ENTRIES, G_N_ELEMENTS (UI_ENTRIES), self);
	seahorse_viewer_include_actions (self, actions);
	g_object_unref (actions);
	
	pv->object_actions = gtk_action_group_new ("key");
	gtk_action_group_set_translation_domain (pv->object_actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (pv->object_actions, KEY_ENTRIES, G_N_ELEMENTS (KEY_ENTRIES), self);
	/* Mark the properties toolbar button as important */
	g_object_set (gtk_action_group_get_action (pv->object_actions, "show-properties"), "is-important", TRUE, NULL);
	seahorse_viewer_include_actions (self, pv->object_actions);
	pv->delete_action = gtk_action_group_get_action (pv->object_actions, "edit-delete");
	g_return_if_fail (pv->delete_action);
	g_object_ref (pv->delete_action);
	
	pv->export_actions = gtk_action_group_new ("export");
	gtk_action_group_set_translation_domain (pv->export_actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (pv->export_actions, EXPORT_ENTRIES, G_N_ELEMENTS (EXPORT_ENTRIES), self);
	seahorse_viewer_include_actions (self, pv->export_actions);
	
	pv->import_actions = gtk_action_group_new ("import");
	gtk_action_group_set_translation_domain (pv->import_actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (pv->import_actions, IMPORT_ENTRIES, G_N_ELEMENTS (IMPORT_ENTRIES), self);
	g_object_set (gtk_action_group_get_action (pv->import_actions, "key-import-keyring"), "is-important", TRUE, NULL);
	seahorse_viewer_include_actions (self, pv->import_actions);
}

static void 
on_selection_changed (SeahorseView* view, SeahorseViewer* self) 
{
	SeahorseViewerPrivate *pv = SEAHORSE_VIEWER_GET_PRIVATE (self);
	ViewerPredicate *predicate;
	GList *objects;
	guint i;
	
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (SEAHORSE_IS_VIEW (view));
	
	/* Enable if anything is selected */
	gtk_action_group_set_sensitive (pv->object_actions, seahorse_view_get_selected (view) != NULL);
	
	objects = seahorse_viewer_get_selected_objects (self);
	
	/* Enable if any exportable objects are selected */
	gtk_action_group_set_sensitive (pv->export_actions, 
	                                has_matching_objects (&exportable_predicate, objects));
	                                
    /* Enable if any importable objects are selected */
    gtk_action_group_set_sensitive (pv->import_actions, 
	                                has_matching_objects (&importable_predicate, objects));
	
	/* Enable if any deletable objects are selected */
	gtk_action_set_sensitive (pv->delete_action, 
	                          has_matching_objects (&deletable_predicate, objects));
	
	/* Go through the list of actions and disable all those which have no matches */
	for (i = 0; i < pv->predicates->len; ++i) {
		predicate = &g_array_index (pv->predicates, ViewerPredicate, i);
		if (predicate->is_commands)
			continue;
		gtk_action_group_set_visible (GTK_ACTION_GROUP (predicate->commands_or_actions),
		                              has_matching_objects (&predicate->pred, objects));
	}
	
	g_list_free (objects);
}

static void 
on_add_widget (GtkUIManager* ui, GtkWidget* widget, SeahorseViewer* self) 
{
	const char* name = NULL;
	GtkWidget* holder;
	
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_UI_MANAGER (ui));
	g_return_if_fail (GTK_IS_WIDGET (widget));
	
	if (GTK_IS_MENU_BAR (widget))
		name = "menu-placeholder"; 
	else if (GTK_IS_TOOLBAR (widget))
		name = "toolbar-placeholder";
	else
		name = NULL;

	holder = seahorse_widget_get_widget (SEAHORSE_WIDGET (self), name);
	if (holder != NULL) 
		gtk_container_add ((GTK_CONTAINER (holder)), widget);
	else
		g_warning ("no place holder found for: %s", name);
}


/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static GList*
seahorse_viewer_get_selected_matching (SeahorseView *base, SeahorseObjectPredicate *pred)
{
	GList *all_objects;
	GList *objects;
	
	g_return_val_if_fail (SEAHORSE_IS_VIEW (base), NULL);
	g_return_val_if_fail (pred, NULL);
	
	all_objects = seahorse_view_get_selected_objects (base);
	objects = filter_matching_objects (pred, &all_objects);
	g_list_free (all_objects);
	
	return objects;
}

static GObject* 
seahorse_viewer_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	GObject *obj = G_OBJECT_CLASS (seahorse_viewer_parent_class)->constructor (type, n_props, props);
	SeahorseViewer *self = NULL;
	SeahorseViewerPrivate *pv;
	GError *error = NULL;
	GtkWidget *win;
	const gchar *name;
	gchar *path;
	GList *types, *l;
	
	if (obj) {
		pv = SEAHORSE_VIEWER_GET_PRIVATE (obj);
		self = SEAHORSE_VIEWER (obj);

		/* The widgts get added in an idle loop later */
		name = seahorse_widget_get_name (SEAHORSE_WIDGET (self));
		path = g_strdup_printf ("%sseahorse-%s.ui", SEAHORSE_UIDIR, name);
		if (!gtk_ui_manager_add_ui_from_file (pv->ui_manager, path, &error)) {
			g_warning ("couldn't load ui description for '%s': %s", name, error->message);
			g_clear_error (&error);
		}

		g_free (path);
		
		win = seahorse_widget_get_toplevel (SEAHORSE_WIDGET (self));
		if (G_TYPE_FROM_INSTANCE (G_OBJECT (win)) == GTK_TYPE_WINDOW)
			gtk_window_add_accel_group (GTK_WINDOW (win), 
			                            gtk_ui_manager_get_accel_group (pv->ui_manager));

		include_basic_actions (self);
		
		g_signal_connect (SEAHORSE_VIEW (self), "selection-changed", 
		                  G_CALLBACK (on_selection_changed), self);

		/* Setup the commands */
		types = seahorse_registry_object_types (seahorse_registry_get (), "commands", NULL, NULL);
		for (l = types; l; l = g_list_next (l)) {
			GType typ = GPOINTER_TO_SIZE (l->data);
			SeahorseCommands *commands;
			
			commands = g_object_new (typ, "view", self, NULL);
			pv->all_commands = seahorse_object_list_prepend (pv->all_commands, commands);
			g_object_unref (commands);
		}
	}
	
	return obj;
}


static void
seahorse_viewer_init (SeahorseViewer *self)
{
	SeahorseViewerPrivate *pv = SEAHORSE_VIEWER_GET_PRIVATE (self);
	
	pv->ui_manager = gtk_ui_manager_new ();
	g_signal_connect (pv->ui_manager, "add-widget", G_CALLBACK (on_add_widget), self);

	pv->predicates = g_array_new (FALSE, TRUE, sizeof (ViewerPredicate));
}

static void
seahorse_viewer_dispose (GObject *obj)
{
	SeahorseViewerPrivate *pv = SEAHORSE_VIEWER_GET_PRIVATE (obj);
	ViewerPredicate *predicate;
	guint i;
	
	if (pv->ui_manager)
		g_object_unref (pv->ui_manager);
	pv->ui_manager = NULL;
	
	if (pv->object_actions)
		g_object_unref (pv->object_actions);
	pv->object_actions = NULL;
	
	if (pv->export_actions)
		g_object_unref (pv->export_actions);
	pv->export_actions = NULL;
	
	if (pv->delete_action)
		g_object_unref (pv->delete_action);
	pv->delete_action = NULL;

	for (i = 0; i < pv->predicates->len; ++i) {
		predicate = &g_array_index (pv->predicates, ViewerPredicate, i);
		g_object_unref (predicate->commands_or_actions);
	}
	g_array_set_size (pv->predicates, 0);
	
	seahorse_object_list_free (pv->all_commands);
	pv->all_commands = NULL;
	
	G_OBJECT_CLASS (seahorse_viewer_parent_class)->dispose (obj);
}

static void
seahorse_viewer_finalize (GObject *obj)
{
	SeahorseViewerPrivate *pv = SEAHORSE_VIEWER_GET_PRIVATE(obj);
	
	g_assert (pv->object_actions == NULL);
	g_assert (pv->export_actions == NULL);
	g_assert (pv->delete_action == NULL);
	g_assert (pv->all_commands == NULL);
	g_assert (pv->ui_manager == NULL);
	g_array_free (pv->predicates, TRUE);

	G_OBJECT_CLASS (seahorse_viewer_parent_class)->finalize (obj);
}

static void
seahorse_viewer_get_property (GObject *obj, guint prop_id, GValue *value, 
                              GParamSpec *pspec)
{
	SeahorseViewer *self = SEAHORSE_VIEWER (obj);
	
	switch (prop_id) {
	case PROP_SELECTED:
		g_value_set_object (value, seahorse_viewer_get_selected (self));
		break;
	case PROP_CURRENT_SET:
		g_value_set_object (value, seahorse_viewer_get_current_set (self));
		break;
	case PROP_WINDOW:
		g_value_set_object (value, seahorse_view_get_window (SEAHORSE_VIEW (self)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_viewer_set_property (GObject *obj, guint prop_id, const GValue *value, 
                              GParamSpec *pspec)
{
	SeahorseViewer *self = SEAHORSE_VIEWER (obj);
	
	switch (prop_id) {
	case PROP_SELECTED:
		seahorse_viewer_set_selected (self, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_viewer_class_init (SeahorseViewerClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    
	seahorse_viewer_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseViewerPrivate));

	gobject_class->constructor = seahorse_viewer_constructor;
	gobject_class->dispose = seahorse_viewer_dispose;
	gobject_class->finalize = seahorse_viewer_finalize;
	gobject_class->set_property = seahorse_viewer_set_property;
	gobject_class->get_property = seahorse_viewer_get_property;
	
	g_object_class_install_property (gobject_class, PROP_SELECTED,
	           g_param_spec_object ("selected", "Selected", "Selected Object", 
					SEAHORSE_TYPE_OBJECT, G_PARAM_READWRITE));
	
	g_object_class_install_property (gobject_class, PROP_WINDOW,
	           g_param_spec_object ("window", "Window", "Window of View",
	                                GTK_TYPE_WIDGET, G_PARAM_READABLE));
	
	g_object_class_install_property (gobject_class, PROP_CURRENT_SET,
	           g_param_spec_object ("current-set", "Current Set", "Currently visible set of objects",
	                                SEAHORSE_TYPE_SET, G_PARAM_READABLE));
	
	exportable_predicate.flags = SEAHORSE_FLAG_EXPORTABLE;
	deletable_predicate.flags = SEAHORSE_FLAG_DELETABLE;
	importable_predicate.flags = SEAHORSE_FLAG_EXPORTABLE;
	importable_predicate.location = SEAHORSE_LOCATION_REMOTE;
	remote_predicate.location = SEAHORSE_LOCATION_REMOTE;
}

static void 
seahorse_viewer_implement_view (SeahorseViewIface *iface) 
{
	iface->get_selected_objects = (gpointer)seahorse_viewer_get_selected_objects;
	iface->set_selected_objects = (gpointer)seahorse_viewer_set_selected_objects;
	iface->get_selected = (gpointer)seahorse_viewer_get_selected;
	iface->set_selected = (gpointer)seahorse_viewer_set_selected;
	iface->get_selected_matching = (gpointer)seahorse_viewer_get_selected_matching;
	iface->get_current_set = (gpointer)seahorse_viewer_get_current_set;
	iface->get_window = (gpointer)seahorse_viewer_get_window;
	iface->register_ui = (gpointer)seahorse_viewer_register_ui;
	iface->register_commands = (gpointer)seahorse_viewer_register_commands;
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

void
seahorse_viewer_ensure_updated (SeahorseViewer* self)
{
	SeahorseViewerPrivate *pv = SEAHORSE_VIEWER_GET_PRIVATE (self);
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	
	gtk_ui_manager_ensure_update (pv->ui_manager);

}

void
seahorse_viewer_include_actions (SeahorseViewer* self, GtkActionGroup* actions)
{
	SeahorseViewerPrivate *pv = SEAHORSE_VIEWER_GET_PRIVATE (self);
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));

	gtk_ui_manager_insert_action_group (pv->ui_manager, actions, -1);
}

GList*
seahorse_viewer_get_selected_objects (SeahorseViewer* self)
{
	g_return_val_if_fail (SEAHORSE_IS_VIEWER (self), NULL);
	g_return_val_if_fail (SEAHORSE_VIEWER_GET_CLASS (self)->get_selected_objects, NULL);
	
	return SEAHORSE_VIEWER_GET_CLASS (self)->get_selected_objects (self);
}

void
seahorse_viewer_set_selected_objects (SeahorseViewer* self, GList* objects)
{
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (SEAHORSE_VIEWER_GET_CLASS (self)->set_selected_objects);
	
	SEAHORSE_VIEWER_GET_CLASS (self)->set_selected_objects (self, objects);
}

SeahorseObject*
seahorse_viewer_get_selected_object_and_uid (SeahorseViewer* self, guint* uid)
{
	g_return_val_if_fail (SEAHORSE_IS_VIEWER (self), NULL);
	g_return_val_if_fail (SEAHORSE_VIEWER_GET_CLASS (self)->get_selected_object_and_uid, NULL);
	
	return SEAHORSE_VIEWER_GET_CLASS (self)->get_selected_object_and_uid (self, uid);
}

void
seahorse_viewer_show_context_menu (SeahorseViewer* self, guint button, guint time)
{
	SeahorseViewerPrivate *pv = SEAHORSE_VIEWER_GET_PRIVATE (self);
	GtkMenu* menu;

	g_return_if_fail (SEAHORSE_IS_VIEWER (self));

	menu = GTK_MENU (gtk_ui_manager_get_widget (pv->ui_manager, "/KeyPopup"));
	g_return_if_fail (GTK_IS_MENU (menu));
	
	gtk_menu_popup (menu, NULL, NULL, NULL, NULL, button, time);
	gtk_widget_show (GTK_WIDGET (menu));
}

void
seahorse_viewer_show_properties (SeahorseViewer* self, SeahorseObject* obj)
{
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (SEAHORSE_IS_OBJECT (obj));
	
	for_each_commands (self, show_properties_for_selected, obj);
}

void
seahorse_viewer_set_status (SeahorseViewer* self, const char* text)
{
	GtkStatusbar* status;
	guint id;

	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (text != NULL);
	
	status = GTK_STATUSBAR (seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "status"));
	g_return_if_fail (GTK_IS_STATUSBAR (status));

	id = gtk_statusbar_get_context_id (status, "key-manager");
	gtk_statusbar_pop (status, id);
	gtk_statusbar_push (status, id, text);
}

void
seahorse_viewer_set_numbered_status (SeahorseViewer* self, const char* text, gint num)
{
	gchar* message;

	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (text != NULL);
	
	message = g_strdup_printf (text, num);
	seahorse_viewer_set_status (self, message);
	g_free (message);
}

SeahorseObject*
seahorse_viewer_get_selected (SeahorseViewer* self)
{
	g_return_val_if_fail (SEAHORSE_IS_VIEWER (self), NULL);
	g_return_val_if_fail (SEAHORSE_VIEWER_GET_CLASS (self)->get_selected, NULL);

	return SEAHORSE_VIEWER_GET_CLASS (self)->get_selected (self);
}

void
seahorse_viewer_set_selected (SeahorseViewer* self, SeahorseObject* value)
{
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (SEAHORSE_VIEWER_GET_CLASS (self)->set_selected);
	
	SEAHORSE_VIEWER_GET_CLASS (self)->set_selected (self, value);
}

SeahorseSet*
seahorse_viewer_get_current_set (SeahorseViewer* self)
{
	g_return_val_if_fail (SEAHORSE_IS_VIEWER (self), NULL);
	g_return_val_if_fail (SEAHORSE_VIEWER_GET_CLASS (self)->get_current_set, NULL);
	
	return SEAHORSE_VIEWER_GET_CLASS (self)->get_current_set (self);
}

GtkWindow* 
seahorse_viewer_get_window (SeahorseViewer* self) 
{
	g_return_val_if_fail (SEAHORSE_IS_VIEWER (self), NULL);
	return GTK_WINDOW (seahorse_widget_get_toplevel (SEAHORSE_WIDGET (self)));
}

void
seahorse_viewer_register_ui (SeahorseViewer *self, SeahorseObjectPredicate *pred,
                             const gchar *uidef, GtkActionGroup *actions)
{
	SeahorseViewerPrivate *pv = SEAHORSE_VIEWER_GET_PRIVATE (self);
	GError *error = NULL;
	ViewerPredicate predicate;
	
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));

	if (actions != NULL) {
		seahorse_viewer_include_actions (self, actions);
		
		/* Add this to the list */
		memset (&predicate, 0, sizeof (predicate));
		if (pred)
			predicate.pred = *pred;
		predicate.is_commands = FALSE;
		predicate.commands_or_actions = G_OBJECT (actions);
		g_object_ref (actions);
		g_array_append_val (pv->predicates, predicate);
	}
	
	if (uidef && uidef[0]) {
		if (!gtk_ui_manager_add_ui_from_string (pv->ui_manager, uidef, -1, &error)) {
			g_warning ("couldn't load UI description: %s", error->message);
			g_clear_error (&error);
		}
	}
}

void
seahorse_viewer_register_commands (SeahorseViewer *self, SeahorseObjectPredicate *pred, 
                                   SeahorseCommands *commands)
{
	SeahorseViewerPrivate *pv = SEAHORSE_VIEWER_GET_PRIVATE (self);
	ViewerPredicate predicate;
	
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (SEAHORSE_IS_COMMANDS (commands));

	/* Add this to the list of commands */
	memset (&predicate, 0, sizeof (predicate));
	if (pred)
		predicate.pred = *pred;
	predicate.is_commands = TRUE;
	predicate.commands_or_actions = G_OBJECT (commands);
	g_object_ref (commands);
	g_array_append_val (pv->predicates, predicate);
}
