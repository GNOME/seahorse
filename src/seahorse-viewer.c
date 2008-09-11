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
#include "seahorse-key.h"
#include "seahorse-preferences.h"
#include "seahorse-progress.h"
#include "seahorse-util.h"
#include "seahorse-view.h"
#include "seahorse-viewer.h"

#include "common/seahorse-registry.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

enum {
	PROP_0,
	PROP_SELECTED,
	PROP_CURRENT_SET,
	PROP_WINDOW
};

struct _SeahorseViewerPrivate {
	GtkUIManager *ui_manager;
	GtkActionGroup *object_actions;
	GtkActionGroup *export_actions;
	GHashTable *commands;
};

static void seahorse_viewer_implement_view (SeahorseViewIface *iface);
G_DEFINE_TYPE_EXTENDED (SeahorseViewer, seahorse_viewer, SEAHORSE_TYPE_WIDGET, 0,
                        G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_VIEW, seahorse_viewer_implement_view));

#define SEAHORSE_VIEWER_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_TYPE_VIEWER, SeahorseViewerPrivate))

static gboolean about_initialized = FALSE;

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static void 
on_app_preferences (GtkAction* action, SeahorseViewer* self) 
{
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_ACTION (action));

	seahorse_preferences_show (seahorse_view_get_window (SEAHORSE_VIEW (self)), NULL);
}

static void 
on_about_link_clicked (GtkAboutDialog* about, const char* url, gpointer unused) 
{
	GError *error = NULL;
	GAppLaunchContext* ctx;
	
	g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));
	g_return_if_fail (url != NULL);
	
	
	ctx = g_app_launch_context_new ();
	if (!g_app_info_launch_default_for_uri (url, ctx, &error)) {
		g_warning ("couldn't launch url: %s: %s", url, error->message);
		g_clear_error (&error);
	}
	
	g_object_unref (ctx);
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

	if (!about_initialized) {
		about_initialized = TRUE;
		gtk_about_dialog_set_url_hook (on_about_link_clicked, NULL, NULL);
	}
	
	about = GTK_ABOUT_DIALOG (gtk_about_dialog_new ());
	gtk_about_dialog_set_artists (about, artists);
	gtk_about_dialog_set_authors (about, authors);
	gtk_about_dialog_set_documenters (about, documenters);
	gtk_about_dialog_set_version (about, VERSION);
	gtk_about_dialog_set_comments (about, _("Encryption Key Manager"));
	gtk_about_dialog_set_copyright (about, "Copyright \xc2\xa9 2002 - 2008 Seahorse Project");
	gtk_about_dialog_set_translator_credits (about, _("translator-credits"));
	gtk_about_dialog_set_logo_icon_name (about, "seahorse");
	gtk_about_dialog_set_website (about, "http://www.gnome.org/projects/seahorse");
	gtk_about_dialog_set_website_label (about, _("Seahorse Project Homepage"));
	
	g_signal_connect (about, "response", G_CALLBACK (gtk_widget_hide), NULL);
	
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
	{ "key-menu", NULL, N_("_Key") },
	{ "edit-menu", NULL, N_("_Edit") },
	{ "view-menu", NULL, N_("_View") },
	{ "help-menu", NULL, N_("_Help") },
	
	{ "app-preferences", GTK_STOCK_PREFERENCES, N_("Prefere_nces"), NULL,
	  N_("Change preferences for this program"), G_CALLBACK (on_app_preferences) },
	{ "app-about", "gtk-about", N_("_About"), NULL, 
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
		if (seahorse_object_get_flags (l->data) & SKEY_FLAG_EXPORTABLE)
			exportable = g_list_append (exportable, l->data);
	}
	
	g_list_free (objects);
	return exportable;
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
	GList* objects;
	GtkDialog* dialog;
	char* uri;
	
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
			seahorse_util_handle_error (error, _ ("Couldn't export key to \"%s\""), 
			                            seahorse_util_uri_get_last (uri), NULL);
			g_clear_error (&error);
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

static gint 
compare_by_tag (SeahorseObject* one, SeahorseObject* two, SeahorseViewer* self) 
{
	GQuark kone;
	GQuark ktwo;
	
	g_return_val_if_fail (SEAHORSE_IS_VIEWER (self), 0);
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (one), 0);
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (two), 0);
	
	kone = seahorse_object_get_tag (one);
	ktwo = seahorse_object_get_tag (two);
	
	if (kone < ktwo)
		return -1;
	if (kone > ktwo)
		return 1;
	return 0;
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

static void 
delete_object_batch (SeahorseViewer* self, GList* objects) 
{
	SeahorseViewerPrivate *pv = SEAHORSE_VIEWER_GET_PRIVATE (self);
	SeahorseCommands* commands;
	SeahorseOperation* op;

	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (objects != NULL);
	g_assert (objects != NULL);
	
	commands = g_hash_table_lookup (pv->commands, GINT_TO_POINTER (seahorse_object_get_tag (objects->data)));
	if (commands == NULL)
		return;

	op = seahorse_commands_delete_objects (commands, objects);
	if (op != NULL) {
		seahorse_progress_show (op, _ ("Deleting..."), TRUE);
		seahorse_operation_watch (op, (SeahorseDoneFunc)on_delete_complete, self, NULL, NULL);
		g_object_unref (op);
	}
}

static void
on_key_delete (GtkAction* action, SeahorseViewer* self) 
{
	GList *objects = NULL;
	GList *batch = NULL;
	GQuark ktype = 0;
	GList *l;
	guint num;

	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	
	/* Get the selected objects and sort them by ktype */
	objects = seahorse_viewer_get_selected_objects (self);
	objects = g_list_sort (objects, (GCompareFunc)compare_by_tag);
	
	num = g_list_length (objects);
	if (num == 0)
		return;

	/* Check for private objects */
	for (l = objects; l; l = g_list_next (l)) {
		SeahorseObject* object = l->data;
		
		if (seahorse_object_get_usage (object) == SEAHORSE_USAGE_PRIVATE_KEY) {
			gchar* prompt = NULL;
			if (num == 1)
				prompt = g_strdup_printf (_("%s is a private key. Are you sure you want to proceed?"), 
				                          seahorse_object_get_display_name (object));
			else
				prompt = g_strdup (_("One or more of the deleted keys are private keys. Are you sure you want to proceed?"));
						
			if (!seahorse_util_prompt_delete (prompt, GTK_WIDGET (seahorse_view_get_window (SEAHORSE_VIEW (self))))) {
				g_free (prompt);
				g_list_free (objects);
				return;
			}
		}
	}
	
	batch = NULL;
	for (l = objects; l; l = g_list_next (l)) {
		SeahorseObject* object = SEAHORSE_OBJECT (l->data);

		/* Process that batch */
		if (ktype != seahorse_object_get_tag (object) && batch != NULL) {
			delete_object_batch (self, batch);
			g_list_free (batch);
			batch = NULL;
		}
		
		/* Add to the batch */
		batch = g_list_prepend (batch, object);
	}

	/* Process last batch */
	if (batch != NULL)
		delete_object_batch (self, batch);

	g_list_free (objects);
	g_list_free (batch);
}
		
static const GtkActionEntry KEY_ENTRIES[] = {
	{ "key-properties", GTK_STOCK_PROPERTIES, N_("P_roperties"), NULL,
	  N_("Show key properties"), G_CALLBACK (on_key_properties) }, 
	{ "key-delete", GTK_STOCK_DELETE, N_("_Delete Key"), NULL,
	  N_("Delete selected keys"), G_CALLBACK (on_key_delete) }
};

static const GtkActionEntry EXPORT_ENTRIES[] = {
	{ "key-export-file", GTK_STOCK_SAVE_AS, N_("E_xport Public Key..."), NULL,
	  N_("Export public part of key to a file"), G_CALLBACK (on_key_export_file) },
	{ "key-export-clipboard", GTK_STOCK_COPY, N_("_Copy Public Key"), "<control>C",
	  N_("Copy public part of selected keys to the clipboard"), G_CALLBACK (on_key_export_clipboard) }
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
	g_object_set (gtk_action_group_get_action (pv->object_actions, "key-properties"), "is-important", TRUE, NULL);
	seahorse_viewer_include_actions (self, pv->object_actions);
	
	pv->export_actions = gtk_action_group_new ("export");
	gtk_action_group_set_translation_domain (pv->export_actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (pv->export_actions, EXPORT_ENTRIES, G_N_ELEMENTS (EXPORT_ENTRIES), self);
	seahorse_viewer_include_actions (self, pv->export_actions);
}

static void 
on_selection_changed (SeahorseView* view, SeahorseViewer* self) 
{
	SeahorseViewerPrivate *pv = SEAHORSE_VIEWER_GET_PRIVATE (self);
	gboolean exportable = FALSE;
	GList *objects;
	
	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (SEAHORSE_IS_VIEW (view));
	
	/* Enable if anything is selected */
	gtk_action_group_set_sensitive (pv->object_actions, seahorse_view_get_selected (view) != NULL);
	
	objects = seahorse_viewer_get_selected_objects (self);
	objects = objects_prune_non_exportable (objects);
	exportable = (objects != NULL);
	g_list_free (objects);
	
	/* Enable if any exportable objects are selected */
	gtk_action_group_set_sensitive (pv->export_actions, exportable);
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
		path = g_strdup_printf ("%sseahorse-%s.ui", SEAHORSE_GLADEDIR, name);
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
		types = seahorse_registry_find_types (seahorse_registry_get (), "commands", NULL, NULL);
		for (l = types; l; l = g_list_next (l)) {
			GType typ = GPOINTER_TO_INT (l->data);
			SeahorseCommands *commands;
			GtkActionGroup *actions;
			const gchar *uidef;
			
			/* Add each commands to our hash table */
			commands = g_object_new (typ, "view", self, NULL);
			g_hash_table_insert (pv->commands, GINT_TO_POINTER (seahorse_commands_get_ktype (commands)), commands);

			actions = seahorse_commands_get_command_actions (commands);
			if (actions != NULL)
				seahorse_viewer_include_actions (self, actions);
			g_object_unref (actions);
			
			uidef = seahorse_commands_get_ui_definition (commands);
			if (uidef && uidef[0]) {
				if (!gtk_ui_manager_add_ui_from_string (pv->ui_manager, uidef, -1, &error)) {
					g_warning ("couldn't load UI description from commands: %s: %s", G_OBJECT_TYPE_NAME(commands), error->message);
					g_clear_error (&error);
				}
			}

			
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

	pv->commands = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
	
	
}

static void
seahorse_viewer_dispose (GObject *obj)
{
	SeahorseViewerPrivate *pv = SEAHORSE_VIEWER_GET_PRIVATE (obj);

	if (pv->ui_manager)
		g_object_unref (pv->ui_manager);
	pv->ui_manager = NULL;
	
	if (pv->object_actions)
		g_object_unref (pv->object_actions);
	pv->object_actions = NULL;
	
	if (pv->export_actions)
		g_object_unref (pv->export_actions);
	pv->export_actions = NULL;
	
	if (pv->commands)
		g_hash_table_unref (pv->commands);
	pv->commands = NULL;
	
	G_OBJECT_CLASS (seahorse_viewer_parent_class)->dispose (obj);
}

static void
seahorse_viewer_finalize (GObject *obj)
{
	SeahorseViewerPrivate *pv = SEAHORSE_VIEWER_GET_PRIVATE(obj);
	
	g_assert (pv->object_actions == NULL);
	g_assert (pv->export_actions == NULL);
	g_assert (pv->commands == NULL);
	g_assert (pv->ui_manager == NULL);

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
}

static void 
seahorse_viewer_implement_view (SeahorseViewIface *iface) 
{
	iface->get_selected_objects = (gpointer)seahorse_viewer_get_selected_objects;
	iface->set_selected_objects = (gpointer)seahorse_viewer_set_selected_objects;
	iface->get_selected = (gpointer)seahorse_viewer_get_selected;
	iface->set_selected = (gpointer)seahorse_viewer_set_selected;
	iface->get_current_set = (gpointer)seahorse_viewer_get_current_set;
	iface->get_window = (gpointer)seahorse_viewer_get_window;
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
	SeahorseViewerPrivate *pv = SEAHORSE_VIEWER_GET_PRIVATE (self);
	SeahorseCommands* commands;

	g_return_if_fail (SEAHORSE_IS_VIEWER (self));
	g_return_if_fail (SEAHORSE_IS_OBJECT (obj));
	
	commands = SEAHORSE_COMMANDS (g_hash_table_lookup (pv->commands, GINT_TO_POINTER (seahorse_object_get_tag (obj))));
	if (commands != NULL)
		seahorse_commands_show_properties (commands, obj);
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
