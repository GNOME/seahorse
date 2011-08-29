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

#include "seahorse-gkr-keyring-commands.h"

#include "seahorse-gkr.h"
#include "seahorse-gkr-keyring.h"
#include "seahorse-gkr-dialogs.h"
#include "seahorse-gkr-operation.h"
#include "seahorse-gkr-source.h"

#include "seahorse-progress.h"
#include "seahorse-registry.h"
#include "seahorse-source.h"
#include "seahorse-util.h"

#include <glib/gi18n.h>

struct _SeahorseGkrKeyringCommandsPrivate {
	GtkAction *action_lock;
	GtkAction *action_unlock;
	GtkAction *action_default;
};

G_DEFINE_TYPE (SeahorseGkrKeyringCommands, seahorse_gkr_keyring_commands, SEAHORSE_TYPE_COMMANDS);

static const char* UI_KEYRING = ""\
"<ui>"\
"	<popup name='KeyPopup'>"\
"		<menuitem action='keyring-default'/>"\
"		<menuitem action='keyring-lock'/>"\
"		<menuitem action='keyring-unlock'/>"\
"		<menuitem action='keyring-password'/>"\
"	</popup>"\
"</ui>";

static SeahorseObjectPredicate keyring_predicate = { 0, };

static void on_view_selection_changed (SeahorseView *view, gpointer user_data);

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static void
on_refresh_all_keyrings_complete (GObject *source,
                                  GAsyncResult *result,
                                  gpointer user_data)
{
	SeahorseCommands *self = SEAHORSE_COMMANDS (user_data);
	SeahorseView *view;

	g_return_if_fail (SEAHORSE_IS_COMMANDS (self));

	view = seahorse_commands_get_view (self);
	if (view != NULL)
		on_view_selection_changed (view, self);

	g_object_unref (self);
}

static void
refresh_all_keyrings (SeahorseCommands *self)
{
	seahorse_source_load_async (SEAHORSE_SOURCE (seahorse_gkr_source_default ()),
	                            NULL, on_refresh_all_keyrings_complete,
	                            g_object_ref (self));
}

/**
 * on_gkr_add_keyring:
 * @action:
 * @unused: ignored
 *
 * calls the function that shows the add keyring window
 */
static void
on_gkr_add_keyring (GtkAction *action, gpointer unused)
{
	g_return_if_fail (GTK_IS_ACTION (action));
	seahorse_gkr_add_keyring_show (NULL);
}

static void
on_gkr_add_item (GtkAction *action, gpointer unused)
{
	g_return_if_fail (GTK_IS_ACTION (action));
	seahorse_gkr_add_item_show (NULL);
}

static const GtkActionEntry ENTRIES_NEW[] = {
	{ "gkr-add-keyring", "folder", N_("Password Keyring"), "",
	  N_("Used to store application and network passwords"), G_CALLBACK (on_gkr_add_keyring) },
	{ "gkr-add-item", "emblem-readonly", N_("Stored Password"), "",
	  N_("Safely store a password or secret."), G_CALLBACK (on_gkr_add_item) }
};

static void
on_keyring_unlock_done (GnomeKeyringResult result, gpointer user_data)
{
	SeahorseCommands *self = SEAHORSE_COMMANDS (user_data);
	SeahorseView *view;

	if (result != GNOME_KEYRING_RESULT_OK &&
	    result != GNOME_KEYRING_RESULT_DENIED &&
	    result != GNOME_KEYRING_RESULT_CANCELLED) {
		view = seahorse_commands_get_view (self);
		seahorse_util_show_error (GTK_WIDGET (seahorse_view_get_window (view)),
		                          _("Couldn't unlock keyring"),
		                          gnome_keyring_result_to_message (result));
	}
	
	refresh_all_keyrings (self);
}

static void 
on_keyring_unlock (GtkAction *action, SeahorseGkrKeyringCommands *self)
{
	SeahorseView *view;
	GList *keys, *l;

	g_return_if_fail (SEAHORSE_IS_GKR_KEYRING_COMMANDS (self));
	g_return_if_fail (GTK_IS_ACTION (action));

	view = seahorse_commands_get_view (SEAHORSE_COMMANDS (self));
	keys = seahorse_view_get_selected_matching (view, &keyring_predicate);

	for (l = keys; l; l = g_list_next (l)) {
		g_return_if_fail (SEAHORSE_IS_GKR_KEYRING (l->data));
		gnome_keyring_unlock (seahorse_gkr_keyring_get_name (l->data), NULL, 
		                      on_keyring_unlock_done, g_object_ref (self), g_object_unref);
	}
	
	g_list_free (keys);
}

static void
on_keyring_lock_done (GnomeKeyringResult result, gpointer user_data)
{
	SeahorseCommands *self = SEAHORSE_COMMANDS (user_data);
	SeahorseView *view;

	if (result != GNOME_KEYRING_RESULT_OK &&
	    result != GNOME_KEYRING_RESULT_DENIED &&
	    result != GNOME_KEYRING_RESULT_CANCELLED) {
		view = seahorse_commands_get_view (self);
		seahorse_util_show_error (GTK_WIDGET (seahorse_view_get_window (view)),
		                          _("Couldn't lock keyring"),
		                          gnome_keyring_result_to_message (result));
	}

	refresh_all_keyrings (self);
}

static void 
on_keyring_lock (GtkAction *action, SeahorseGkrKeyringCommands *self)
{
	SeahorseView *view;
	GList *keyrings, *l;

	g_return_if_fail (SEAHORSE_IS_GKR_KEYRING_COMMANDS (self));
	g_return_if_fail (GTK_IS_ACTION (action));

	view = seahorse_commands_get_view (SEAHORSE_COMMANDS (self));
	keyrings = seahorse_view_get_selected_matching (view, &keyring_predicate);

	for (l = keyrings; l; l = g_list_next (l)) {
		g_return_if_fail (SEAHORSE_IS_GKR_KEYRING (l->data));
		gnome_keyring_lock (seahorse_gkr_keyring_get_name (l->data), 
		                    on_keyring_lock_done, g_object_ref (self), g_object_unref);
	}

	g_list_free (keyrings);
}

static void
on_set_default_keyring_done (GnomeKeyringResult result, gpointer user_data)
{
	SeahorseCommands *self = SEAHORSE_COMMANDS (user_data);
	SeahorseView *view;

	if (result != GNOME_KEYRING_RESULT_OK &&
	    result != GNOME_KEYRING_RESULT_DENIED &&
	    result != GNOME_KEYRING_RESULT_CANCELLED) {
		view = seahorse_commands_get_view (self);
		seahorse_util_show_error (GTK_WIDGET (seahorse_view_get_window (view)),
		                          _("Couldn't set default keyring"),
		                          gnome_keyring_result_to_message (result));
	}
	
	refresh_all_keyrings (self);
}

static void
on_keyring_default (GtkAction *action, SeahorseGkrKeyringCommands *self)
{
	SeahorseView *view;
	GList *keys;

	g_return_if_fail (SEAHORSE_IS_GKR_KEYRING_COMMANDS (self));
	g_return_if_fail (GTK_IS_ACTION (action));

	view = seahorse_commands_get_view (SEAHORSE_COMMANDS (self));
	keys = seahorse_view_get_selected_matching (view, &keyring_predicate);

	if (keys) {
		gnome_keyring_set_default_keyring (seahorse_gkr_keyring_get_name (keys->data), 
		                                   on_set_default_keyring_done, g_object_ref (self), g_object_unref);
	}
	
	g_list_free (keys);

}

static void
on_change_password_done (GnomeKeyringResult result, gpointer user_data)
{
	SeahorseCommands *self = SEAHORSE_COMMANDS (user_data);
	SeahorseView *view;

	if (result != GNOME_KEYRING_RESULT_OK &&
	    result != GNOME_KEYRING_RESULT_DENIED &&
	    result != GNOME_KEYRING_RESULT_CANCELLED) {
		view = seahorse_commands_get_view (self);
		seahorse_util_show_error (GTK_WIDGET (seahorse_view_get_window (view)),
		                          _("Couldn't change keyring password"),
		                          gnome_keyring_result_to_message (result));
	}
	
	refresh_all_keyrings (self);
}

static void
on_keyring_password (GtkAction *action, SeahorseGkrKeyringCommands *self)
{
	SeahorseView *view;
	GList *keys, *l;

	g_return_if_fail (SEAHORSE_IS_GKR_KEYRING_COMMANDS (self));
	g_return_if_fail (GTK_IS_ACTION (action));

	view = seahorse_commands_get_view (SEAHORSE_COMMANDS (self));
	keys = seahorse_view_get_selected_matching (view, &keyring_predicate);

	for (l = keys; l; l = g_list_next (l)) {
		g_return_if_fail (SEAHORSE_IS_GKR_KEYRING (l->data));
		gnome_keyring_change_password (seahorse_gkr_keyring_get_name (l->data), NULL, NULL,
		                               on_change_password_done, g_object_ref (self), g_object_unref);
	}
	
	g_list_free (keys);
}

static const GtkActionEntry ENTRIES_KEYRING[] = {
	{ "keyring-lock", NULL, N_("_Lock"), "",
	  N_("Lock the password storage keyring so a master password is required to unlock it."), G_CALLBACK (on_keyring_lock) },
	{ "keyring-unlock", NULL, N_("_Unlock"), "",
	  N_("Unlock the password storage keyring with a master password so it is available for use."), G_CALLBACK (on_keyring_unlock) },
	{ "keyring-default", NULL, N_("_Set as default"), "",
	  N_("Applications usually store new passwords in the default keyring."), G_CALLBACK (on_keyring_default) },
	{ "keyring-password", NULL, N_("Change _Password"), "",
	  N_("Change the unlock password of the password storage keyring"), G_CALLBACK (on_keyring_password) }
};

static void
on_view_selection_changed (SeahorseView *view, gpointer user_data)
{
	SeahorseGkrKeyringCommands *self = user_data;
	GnomeKeyringInfo *info;
	gboolean locked = FALSE;
	gboolean unlocked = FALSE;
	gboolean can_default = FALSE;
	GList *keys, *l;
	
	g_return_if_fail (SEAHORSE_IS_VIEW (view));
	g_return_if_fail (SEAHORSE_IS_GKR_KEYRING_COMMANDS (self));
	
	keys = seahorse_view_get_selected_matching (view, &keyring_predicate);
	for (l = keys; l; l = g_list_next (l)) {
		info = seahorse_gkr_keyring_get_info (l->data);
		if (info != NULL) {
			if (gnome_keyring_info_get_is_locked (info))
				locked = TRUE;
			else 
				unlocked = TRUE;
			if (!seahorse_gkr_keyring_get_is_default (l->data))
				can_default = TRUE;
		}
	}
	
	g_list_free (keys);
	
	gtk_action_set_sensitive (self->pv->action_lock, unlocked);
	gtk_action_set_sensitive (self->pv->action_unlock, locked);
	gtk_action_set_sensitive (self->pv->action_default, can_default);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void 
seahorse_gkr_keyring_commands_show_properties (SeahorseCommands* base, SeahorseObject* object) 
{
	GtkWindow *window;

	g_return_if_fail (SEAHORSE_IS_OBJECT (object));
	g_return_if_fail (seahorse_object_get_tag (object) == SEAHORSE_GKR_TYPE);

	window = seahorse_view_get_window (seahorse_commands_get_view (base));
	if (G_OBJECT_TYPE (object) == SEAHORSE_TYPE_GKR_KEYRING) 
		seahorse_gkr_keyring_properties_show (SEAHORSE_GKR_KEYRING (object), window);
	
	else
		g_return_if_reached ();
}

static void
on_delete_objects_complete (GObject *source, GAsyncResult *result, gpointer user_data)
{
	SeahorseCommands *commands = SEAHORSE_COMMANDS (user_data);
	GError *error = NULL;
	GtkWidget *parent;

	if (!seahorse_gkr_delete_finish (result, &error)) {
		parent = GTK_WIDGET (seahorse_view_get_window (seahorse_commands_get_view (commands)));
		seahorse_util_show_error (parent, _("Couldn't delete keyring"), error->message);
		g_error_free (error);
	}

	g_object_unref (commands);
}

static gboolean
seahorse_gkr_keyring_commands_delete_objects (SeahorseCommands* commands,
                                              GList* objects)
{
	GCancellable *cancellable;
	gchar *prompt;
	gboolean ret;

	if (!objects)
		return TRUE;

	prompt = g_strdup_printf (_ ("Are you sure you want to delete the password keyring '%s'?"), 
	                          seahorse_object_get_label (objects->data));

	ret = seahorse_util_prompt_delete (prompt, GTK_WIDGET (seahorse_commands_get_window (commands)));

	if (ret) {
		cancellable = g_cancellable_new ();
		seahorse_gkr_delete_async (objects, cancellable,
		                           on_delete_objects_complete,
		                           g_object_ref (commands));
		seahorse_progress_show (cancellable, ngettext ("Deleting keyring", "Deleting keyrings",
		                                               g_list_length (objects)), TRUE);
		g_object_unref (cancellable);
	}

	g_free (prompt);
	return ret;
}

static GObject* 
seahorse_gkr_keyring_commands_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	SeahorseGkrKeyringCommands *self = SEAHORSE_GKR_KEYRING_COMMANDS (G_OBJECT_CLASS (seahorse_gkr_keyring_commands_parent_class)->constructor (type, n_props, props));
	GtkActionGroup *actions;
	SeahorseView *view;
	
	g_return_val_if_fail (SEAHORSE_IS_GKR_KEYRING_COMMANDS (self), NULL);
	
	view = seahorse_commands_get_view (SEAHORSE_COMMANDS (self));
	g_return_val_if_fail (view, NULL);
	seahorse_view_register_commands (view, &keyring_predicate, SEAHORSE_COMMANDS (self));

	actions = gtk_action_group_new ("gkr-keyring");
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, ENTRIES_KEYRING, G_N_ELEMENTS (ENTRIES_KEYRING), self);
	seahorse_view_register_ui (view, &keyring_predicate, UI_KEYRING, actions);
	self->pv->action_lock = g_object_ref (gtk_action_group_get_action (actions, "keyring-lock"));
	self->pv->action_unlock = g_object_ref (gtk_action_group_get_action (actions, "keyring-unlock"));
	self->pv->action_default = g_object_ref (gtk_action_group_get_action (actions, "keyring-default"));
	g_object_unref (actions);
	
	/* Watch and wait for selection changes and diddle lock/unlock */ 
	g_signal_connect (view, "selection-changed", G_CALLBACK (on_view_selection_changed), self);

	return G_OBJECT (self);
}

static void
seahorse_gkr_keyring_commands_init (SeahorseGkrKeyringCommands *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_GKR_KEYRING_COMMANDS, SeahorseGkrKeyringCommandsPrivate);
}

static void
seahorse_gkr_keyring_commands_finalize (GObject *obj)
{
	SeahorseGkrKeyringCommands *self = SEAHORSE_GKR_KEYRING_COMMANDS (obj);

	g_object_unref (self->pv->action_lock);
	self->pv->action_lock = NULL;
	
	g_object_unref (self->pv->action_unlock);
	self->pv->action_unlock = NULL;
	
	g_object_unref (self->pv->action_default);
	self->pv->action_default = NULL;
	
	G_OBJECT_CLASS (seahorse_gkr_keyring_commands_parent_class)->finalize (obj);
}

static void
seahorse_gkr_keyring_commands_class_init (SeahorseGkrKeyringCommandsClass *klass)
{
	GtkActionGroup *actions;
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseCommandsClass *cmd_class = SEAHORSE_COMMANDS_CLASS (klass);
	
	seahorse_gkr_keyring_commands_parent_class = g_type_class_peek_parent (klass);

	gobject_class->constructor = seahorse_gkr_keyring_commands_constructor;
	gobject_class->finalize = seahorse_gkr_keyring_commands_finalize;

	cmd_class->show_properties = seahorse_gkr_keyring_commands_show_properties;
	cmd_class->delete_objects = seahorse_gkr_keyring_commands_delete_objects;

	g_type_class_add_private (gobject_class, sizeof (SeahorseGkrKeyringCommandsPrivate));

	/* Setup the predicate for these commands */
	keyring_predicate.type = SEAHORSE_TYPE_GKR_KEYRING;
	
	/* Register this class as a commands */
	seahorse_registry_register_type (seahorse_registry_get (), SEAHORSE_TYPE_GKR_KEYRING_COMMANDS, 
	                                 SEAHORSE_GKR_TYPE_STR, "commands", NULL, NULL);
	
	/* Register this as a generator */
	actions = gtk_action_group_new ("gkr-generate");
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, ENTRIES_NEW, G_N_ELEMENTS (ENTRIES_NEW), NULL);
	seahorse_registry_register_object (NULL, G_OBJECT (actions), SEAHORSE_GKR_TYPE_STR, "generator", NULL);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */


