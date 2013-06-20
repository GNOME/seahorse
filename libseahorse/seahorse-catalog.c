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

#include "seahorse-action.h"
#include "seahorse-actions.h"
#include "seahorse-catalog.h"
#include "seahorse-common.h"
#include "seahorse-object.h"
#include "seahorse-prefs.h"
#include "seahorse-progress.h"
#include "seahorse-util.h"

#include <gcr/gcr.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

void                on_app_preferences                (GtkAction* action,
                                                       gpointer user_data);

enum {
	PROP_0,
	PROP_WINDOW
};

struct _SeahorseCatalogPrivate {
	GtkUIManager *ui_manager;
	GHashTable *actions;
	GtkAction *edit_delete;
	GtkAction *properties_object;
	GtkAction *file_export;
	GtkAction *edit_copy;
	GList *selection_actions;
};

G_DEFINE_TYPE (SeahorseCatalog, seahorse_catalog, SEAHORSE_TYPE_WIDGET);

/* -----------------------------------------------------------------------------
 * INTERNAL
 */


G_MODULE_EXPORT void
on_app_preferences (GtkAction* action,
                    gpointer user_data)
{
	SeahorseCatalog *self = SEAHORSE_CATALOG (user_data);
	seahorse_prefs_show (seahorse_catalog_get_window (self), NULL);
}

static void
on_app_about (GtkAction* action, SeahorseCatalog *self)
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
	gtk_about_dialog_set_comments (about, _("Passwords and Keys"));
	gtk_about_dialog_set_copyright (about, "Copyright \xc2\xa9 2002 - 2010 Seahorse Project");
	gtk_about_dialog_set_translator_credits (about, _("translator-credits"));
	gtk_about_dialog_set_logo_icon_name (about, "seahorse");
	gtk_about_dialog_set_website (about, "http://www.gnome.org/projects/seahorse");
	gtk_about_dialog_set_website_label (about, _("Seahorse Project Homepage"));

	g_signal_connect (about, "response", G_CALLBACK (gtk_widget_hide), NULL);
	gtk_window_set_transient_for (GTK_WINDOW (about), seahorse_catalog_get_window (self));

	gtk_dialog_run (GTK_DIALOG (about));
	gtk_widget_destroy (GTK_WIDGET (about));
}

static void
on_help_show (GtkAction *action,
              SeahorseCatalog *self)
{
	g_return_if_fail (SEAHORSE_IS_CATALOG (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	seahorse_widget_show_help (SEAHORSE_WIDGET (self));
}

static GList *
lookup_actions_for_objects (SeahorseCatalog *self,
                            GList *objects)
{
	GtkActionGroup *actions;
	GHashTableIter iter;
	GList *results;
	GHashTable *table;
	GQueue *queue;
	GList *l;

	table = g_hash_table_new_full (g_direct_hash,
	                               g_direct_equal,
	                               g_object_unref,
	                               (GDestroyNotify)g_queue_free);

	for (l = objects; l != NULL; l = g_list_next (l)) {
		g_object_get (l->data, "actions", &actions, NULL);
		if (actions == NULL)
			continue;
		if (g_hash_table_lookup (self->pv->actions, actions) == NULL)
			seahorse_catalog_include_actions (self, actions);
		queue = g_hash_table_lookup (table, actions);
		if (queue == NULL) {
			queue = g_queue_new ();
			g_hash_table_insert (table, g_object_ref (actions), queue);
		}
		g_queue_push_tail (queue, l->data);
		g_object_unref (actions);
	}

	results = NULL;
	g_hash_table_iter_init (&iter, table);
	while (g_hash_table_iter_next (&iter, (gpointer *)&actions, (gpointer *)&queue)) {
		seahorse_actions_update (actions, self);
		results = g_list_prepend (results, g_object_ref (actions));
	}

	g_hash_table_destroy (table);
	return results;
}

static void
on_object_delete (GtkAction *action,
                  gpointer user_data)
{
	SeahorseCatalog *self = SEAHORSE_CATALOG (user_data);
	GError *error = NULL;
	GtkWindow *window;
	GList *objects;

	window = seahorse_catalog_get_window (self);
	objects = seahorse_catalog_get_selected_objects (self);
	seahorse_deletable_delete_with_prompt_wait (objects, window, &error);

	if (error != NULL)
		seahorse_util_handle_error (&error, window, _("Cannot delete"));

	g_list_free (objects);
}

static void
on_properties_object (GtkAction *action,
                      gpointer user_data)
{
	SeahorseCatalog *self = SEAHORSE_CATALOG (user_data);
	GList *objects;

	objects = seahorse_catalog_get_selected_objects (self);
	if (objects != NULL)
		seahorse_catalog_show_properties (self, objects->data);
	g_list_free (objects);
}

static void
on_properties_place (GtkAction *action,
                     gpointer user_data)
{
	SeahorseCatalog *self = SEAHORSE_CATALOG (user_data);
	SeahorsePlace *place;

	place = seahorse_catalog_get_focused_place (self);
	if (place != NULL)
		seahorse_catalog_show_properties (self, G_OBJECT (place));
}

static void
on_key_export_file (GtkAction* action, SeahorseCatalog *self)
{
	GError *error = NULL;
	GtkWindow *window;
	GList *objects;

	g_return_if_fail (SEAHORSE_IS_CATALOG (self));
	g_return_if_fail (GTK_IS_ACTION (action));

	window = seahorse_catalog_get_window (self);
	objects = seahorse_catalog_get_selected_objects (self);
	seahorse_exportable_export_to_prompt_wait (objects, window, &error);
	g_list_free (objects);

	/* TODO: message if only partially exported */

	if (error != NULL)
		seahorse_util_handle_error (&error, window, _("Couldn't export keys"));
}

static void
on_key_export_clipboard (GtkAction* action,
                         SeahorseCatalog *self)
{
	GList* objects;
	guchar *output;
	gsize size;
	GError *error = NULL;
	GdkAtom atom;
	GtkClipboard* board;

	g_return_if_fail (SEAHORSE_IS_CATALOG (self));
	g_return_if_fail (GTK_IS_ACTION (action));

	objects = seahorse_catalog_get_selected_objects (self);
	seahorse_exportable_export_to_text_wait (objects, &output, &size, &error);
	g_list_free (objects);

	/* TODO: Print message if only partially exported */

	if (error == NULL) {
		atom = gdk_atom_intern ("CLIPBOARD", FALSE);
		board = gtk_clipboard_get (atom);
		gtk_clipboard_set_text (board, (gchar *)output, (gint)size);
	} else {
		seahorse_util_handle_error (&error, seahorse_catalog_get_window (self),
		                            _("Couldn't export data"));
	}

	g_free (output);
}

static const GtkActionEntry UI_ENTRIES[] = {

	/* Top menu items */
	{ "file-menu", NULL, N_("_File") },
	{ "file-export", GTK_STOCK_SAVE_AS, N_("E_xport..."), NULL,
	  N_("Export to a file"), G_CALLBACK (on_key_export_file) },
	{ "edit-menu", NULL, N_("_Edit") },
	/*Translators: This text refers to deleting an item from its type's backing store*/
	{ "edit-export-clipboard", GTK_STOCK_COPY, NULL, "<control>C",
	  N_("Copy to the clipboard"), G_CALLBACK (on_key_export_clipboard) },
	{ "edit-delete", GTK_STOCK_DELETE, N_("_Delete"), NULL,
	  N_("Delete selected items"), G_CALLBACK (on_object_delete) },
	{ "properties-object", GTK_STOCK_PROPERTIES, NULL, NULL,
	  N_("Show the properties of this item"), G_CALLBACK (on_properties_object) },
	{ "properties-keyring", GTK_STOCK_PROPERTIES, NULL, NULL,
	  N_("Show the properties of this keyring"), G_CALLBACK (on_properties_place) },
	{ "app-preferences", GTK_STOCK_PREFERENCES, N_("Prefere_nces"), NULL,
	  N_("Change preferences for this program"), G_CALLBACK (on_app_preferences) },
	{ "view-menu", NULL, N_("_View") },
	{ "help-menu", NULL, N_("_Help") },
	{ "app-about", GTK_STOCK_ABOUT, NULL, NULL,
	  N_("About this program"), G_CALLBACK (on_app_about) },
	{ "help-show", GTK_STOCK_HELP, N_("_Contents"), "F1",
	  N_("Show Seahorse help"), G_CALLBACK (on_help_show) }
};

static void
on_ui_manager_pre_activate (GtkUIManager *ui_manager,
                            GtkAction *action,
                            gpointer user_data)
{
	SeahorseCatalog *self = SEAHORSE_CATALOG (user_data);
	seahorse_action_pre_activate (action, self, seahorse_catalog_get_window (self));
}

static void
on_ui_manager_post_activate (GtkUIManager *ui_manager,
                             GtkAction *action,
                             gpointer user_data)
{
	seahorse_action_post_activate (action);
}

static void
seahorse_catalog_real_selection_changed (SeahorseCatalog *self)
{
	GList *groups;
	GList *objects;
	GList *previous;
	gboolean can_properties;
	gboolean can_delete;
	gboolean can_export;
	GList *l;

	can_properties = FALSE;
	can_delete = FALSE;
	can_export = FALSE;

	objects = seahorse_catalog_get_selected_objects (self);
	for (l = objects; l != NULL; l = g_list_next (l)) {
		if (seahorse_exportable_can_export (l->data))
			can_export = TRUE;
		if (seahorse_deletable_can_delete (l->data))
			can_delete = TRUE;
		if (seahorse_viewable_can_view (l->data))
			can_properties = TRUE;
		if (can_export && can_delete && can_properties)
			break;
	}

	groups = lookup_actions_for_objects (self, objects);
	g_list_free (objects);

	gtk_action_set_sensitive (self->pv->properties_object, can_properties);
	gtk_action_set_sensitive (self->pv->edit_delete, can_delete);
	gtk_action_set_sensitive (self->pv->edit_copy, can_export);
	gtk_action_set_sensitive (self->pv->file_export, can_export);

	previous = self->pv->selection_actions;
	self->pv->selection_actions = groups;

	for (l = previous; l != NULL; l = g_list_next (l))
		gtk_action_group_set_visible (l->data, FALSE);
	for (l = self->pv->selection_actions; l != NULL; l = g_list_next (l))
		gtk_action_group_set_visible (l->data, TRUE);
	g_list_free_full (previous, g_object_unref);
}

static void
on_ui_manager_add_widget (GtkUIManager *ui_manager,
                          GtkWidget *widget,
                          gpointer user_data)
{
	SeahorseCatalog *self = SEAHORSE_CATALOG (user_data);
	const char* name = NULL;
	GtkWidget* holder;

	if (GTK_IS_MENU_BAR (widget))
		name = "menu-placeholder";
	else if (GTK_IS_TOOLBAR (widget))
		name = "toolbar-placeholder";
	else
		name = NULL;

	holder = seahorse_widget_get_widget (SEAHORSE_WIDGET (self), name);
	if (holder != NULL) {
		gtk_container_add ((GTK_CONTAINER (holder)), widget);
		gtk_widget_show (widget);
	} else {
		g_warning ("no place holder found for: %s", name);
	}
}

static void
seahorse_catalog_constructed (GObject *obj)
{
	SeahorseCatalog *self = SEAHORSE_CATALOG (obj);
	GError *error = NULL;
	GtkWidget *win;
	const gchar *name;
	gchar *path;
	GtkActionGroup *actions;
	GtkAction *action;

	G_OBJECT_CLASS (seahorse_catalog_parent_class)->constructed (obj);

	/* The widgts get added in an idle loop later */
	name = seahorse_widget_get_name (SEAHORSE_WIDGET (self));
	path = g_strdup_printf ("%sseahorse-%s.ui", SEAHORSE_UIDIR, name);
	if (!gtk_ui_manager_add_ui_from_file (self->pv->ui_manager, path, &error)) {
		g_warning ("couldn't load ui description for '%s': %s", name, error->message);
		g_clear_error (&error);
	}

	g_free (path);

	win = seahorse_widget_get_toplevel (SEAHORSE_WIDGET (self));
	if (G_TYPE_FROM_INSTANCE (G_OBJECT (win)) == GTK_TYPE_WINDOW)
		gtk_window_add_accel_group (GTK_WINDOW (win),
		                            gtk_ui_manager_get_accel_group (self->pv->ui_manager));

	actions = gtk_action_group_new ("main");
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, UI_ENTRIES, G_N_ELEMENTS (UI_ENTRIES), self);
	action = gtk_action_group_get_action (actions, "app-preferences");
	gtk_action_set_visible (action, seahorse_prefs_available ());
	self->pv->edit_delete = gtk_action_group_get_action (actions, "edit-delete");
	g_object_ref (self->pv->edit_delete);
	self->pv->properties_object = gtk_action_group_get_action (actions, "properties-object");
	g_object_ref (self->pv->properties_object);
	self->pv->edit_copy = gtk_action_group_get_action (actions, "edit-export-clipboard");
	g_object_ref (self->pv->edit_copy);
	self->pv->file_export = gtk_action_group_get_action (actions, "file-export");
	g_object_ref (self->pv->file_export);
	gtk_ui_manager_insert_action_group (self->pv->ui_manager, actions, 0);
	g_object_unref (actions);
}

static void
seahorse_catalog_init (SeahorseCatalog *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_CATALOG,
	                                        SeahorseCatalogPrivate);
	self->pv->actions = g_hash_table_new_full (g_direct_hash, g_direct_equal,
	                                           g_object_unref, NULL);

	self->pv->ui_manager = gtk_ui_manager_new ();
	g_signal_connect (self->pv->ui_manager, "add-widget",
	                  G_CALLBACK (on_ui_manager_add_widget), self);
	g_signal_connect (self->pv->ui_manager, "pre-activate",
	                  G_CALLBACK (on_ui_manager_pre_activate), self);
	g_signal_connect (self->pv->ui_manager, "post-activate",
	                  G_CALLBACK (on_ui_manager_post_activate), self);
}

static void
seahorse_catalog_dispose (GObject *obj)
{
	SeahorseCatalog *self = SEAHORSE_CATALOG (obj);
	GList *l;

	g_clear_object (&self->pv->edit_copy);
	g_clear_object (&self->pv->edit_delete);
	g_clear_object (&self->pv->file_export);
	g_clear_object (&self->pv->properties_object);

	for (l = self->pv->selection_actions; l != NULL; l = g_list_next (l))
		gtk_ui_manager_remove_action_group (self->pv->ui_manager, l->data);
	g_list_free_full (self->pv->selection_actions, g_object_unref);

	g_signal_handlers_disconnect_by_func (self->pv->ui_manager, on_ui_manager_add_widget, self);
	g_signal_handlers_disconnect_by_func (self->pv->ui_manager, on_ui_manager_pre_activate, self);
	g_signal_handlers_disconnect_by_func (self->pv->ui_manager, on_ui_manager_post_activate, self);
	g_clear_object (&self->pv->ui_manager);

	g_hash_table_remove_all (self->pv->actions);

	G_OBJECT_CLASS (seahorse_catalog_parent_class)->dispose (obj);
}

static void
seahorse_catalog_finalize (GObject *obj)
{
	SeahorseCatalog *self = SEAHORSE_CATALOG (obj);

	g_assert (self->pv->ui_manager == NULL);
	g_assert (g_hash_table_size (self->pv->actions) == 0);
	g_hash_table_destroy (self->pv->actions);

	G_OBJECT_CLASS (seahorse_catalog_parent_class)->finalize (obj);
}

static void
seahorse_catalog_get_property (GObject *obj, guint prop_id, GValue *value,
                              GParamSpec *pspec)
{
	SeahorseCatalog *self = SEAHORSE_CATALOG (obj);

	switch (prop_id) {
	case PROP_WINDOW:
		g_value_set_object (value, seahorse_catalog_get_window (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_catalog_class_init (SeahorseCatalogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (SeahorseCatalogPrivate));

	gobject_class->constructed = seahorse_catalog_constructed;
	gobject_class->dispose = seahorse_catalog_dispose;
	gobject_class->finalize = seahorse_catalog_finalize;
	gobject_class->get_property = seahorse_catalog_get_property;

	klass->selection_changed = seahorse_catalog_real_selection_changed;

	g_object_class_install_property (gobject_class, PROP_WINDOW,
	           g_param_spec_object ("window", "Window", "Window of View",
	                                GTK_TYPE_WIDGET, G_PARAM_READABLE));

	g_signal_new ("selection-changed", SEAHORSE_TYPE_CATALOG,
	              G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SeahorseCatalogClass, selection_changed),
	              NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

void
seahorse_catalog_ensure_updated (SeahorseCatalog *self)
{
	g_return_if_fail (SEAHORSE_IS_CATALOG (self));
	gtk_ui_manager_ensure_update (self->pv->ui_manager);
}

void
seahorse_catalog_include_actions (SeahorseCatalog *self,
                                 GtkActionGroup* actions)
{
	const gchar *definition;
	GError *error = NULL;

	g_return_if_fail (SEAHORSE_IS_CATALOG (self));
	g_return_if_fail (GTK_IS_ACTION_GROUP (actions));

	gtk_ui_manager_insert_action_group (self->pv->ui_manager, actions, 10);

	if (SEAHORSE_IS_ACTIONS (actions)) {
		seahorse_actions_set_catalog (SEAHORSE_ACTIONS (actions), self);
		definition = seahorse_actions_get_definition (SEAHORSE_ACTIONS (actions));
		if (definition != NULL) {
			gtk_ui_manager_add_ui_from_string (self->pv->ui_manager, definition, -1, &error);
			if (error != NULL) {
				g_warning ("couldn't add ui defintion for action group: %s: %s",
				           gtk_action_group_get_name (actions), definition);
				g_clear_error (&error);
			}
		}
	}

	g_hash_table_insert (self->pv->actions, g_object_ref (actions), actions);
}

void
seahorse_catalog_show_context_menu (SeahorseCatalog *self,
                                   const gchar *name,
                                   guint button,
                                   guint time)
{
	GtkWidget *menu;
	gchar *path;

	g_return_if_fail (SEAHORSE_IS_CATALOG (self));
	g_return_if_fail (name != NULL);

	path = g_strdup_printf ("/%s", name);
	menu = gtk_ui_manager_get_widget (self->pv->ui_manager, path);
	g_free (path);

	if (menu == NULL)
		return;
	if (!GTK_IS_MENU (menu)) {
		g_warning ("the object /%s isn't a menu", name);
		return;
	}

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, time);
	gtk_widget_show (menu);
}

void
seahorse_catalog_show_properties (SeahorseCatalog *self,
                                  GObject* obj)
{
	seahorse_viewable_view (obj, seahorse_catalog_get_window (self));
}

GtkWindow *
seahorse_catalog_get_window (SeahorseCatalog *self)
{
	g_return_val_if_fail (SEAHORSE_IS_CATALOG (self), NULL);
	return GTK_WINDOW (seahorse_widget_get_toplevel (SEAHORSE_WIDGET (self)));
}

GList *
seahorse_catalog_get_selected_objects (SeahorseCatalog *self)
{
	g_return_val_if_fail (SEAHORSE_IS_CATALOG (self), NULL);
	g_return_val_if_fail (SEAHORSE_CATALOG_GET_CLASS (self)->get_selected_objects, NULL);
	return SEAHORSE_CATALOG_GET_CLASS (self)->get_selected_objects (self);
}

SeahorsePlace *
seahorse_catalog_get_focused_place (SeahorseCatalog *self)
{
	g_return_val_if_fail (SEAHORSE_IS_CATALOG (self), NULL);
	g_return_val_if_fail (SEAHORSE_CATALOG_GET_CLASS (self)->get_focused_place, NULL);
	return SEAHORSE_CATALOG_GET_CLASS (self)->get_focused_place (self);
}

GList *
seahorse_catalog_get_backends (SeahorseCatalog *self)
{
	g_return_val_if_fail (SEAHORSE_IS_CATALOG (self), NULL);
	g_return_val_if_fail (SEAHORSE_CATALOG_GET_CLASS (self)->get_backends, NULL);
	return SEAHORSE_CATALOG_GET_CLASS (self)->get_backends (self);
}
