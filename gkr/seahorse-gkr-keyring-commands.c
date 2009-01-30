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

#include "seahorse-gkr-keyring-commands.h"

#include "seahorse-gkr.h"
#include "seahorse-gkr-keyring.h"
#include "seahorse-gkr-dialogs.h"

#include "common/seahorse-registry.h"

#include "seahorse-source.h"
#include "seahorse-util.h"

#include <glib/gi18n.h>

enum {
	PROP_0
};

G_DEFINE_TYPE (SeahorseGkrKeyringCommands, seahorse_gkr_keyring_commands, SEAHORSE_TYPE_COMMANDS);

static const char* UI_KEYRING = ""\
"<ui>"\
"	<popup name='KeyPopup'>"\
"		<menuitem action='keyring-password'/>"\
"	</popup>"\
"</ui>";

static SeahorseObjectPredicate keyring_predicate = { 0, };

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static void
on_gkr_add_keyring (GtkAction *action, gpointer unused)
{
	g_return_if_fail (GTK_IS_ACTION (action));
	seahorse_gkr_add_keyring_show (NULL);
}

static const GtkActionEntry ENTRIES_NEW[] = {
	{ "gkr-add-keyring", "folder", N_("Password Keyring"), "",
	  N_("Used to store application and network passwords"), G_CALLBACK (on_gkr_add_keyring) }
};

static void
on_change_password_done (GnomeKeyringResult result, gpointer user_data)
{
	SeahorseView *view;

	if (result != GNOME_KEYRING_RESULT_OK &&
	    result != GNOME_KEYRING_RESULT_DENIED &&
	    result != GNOME_KEYRING_RESULT_CANCELLED) {
		view = seahorse_commands_get_view (SEAHORSE_COMMANDS (user_data));
		seahorse_util_show_error (GTK_WIDGET (seahorse_view_get_window (view)),
		                          _("Couldn't change keyring password"),
		                          gnome_keyring_result_to_message (result));
	}
}

static void
on_keyring_password (GtkAction *action, SeahorseGkrKeyringCommands *self)
{
	SeahorseView *view;
	SeahorseGkrKeyring *keyring;
	GList *keys, *l;

	g_return_if_fail (SEAHORSE_IS_GKR_KEYRING_COMMANDS (self));
	g_return_if_fail (GTK_IS_ACTION (action));

	view = seahorse_commands_get_view (SEAHORSE_COMMANDS (self));
	keys = seahorse_view_get_selected_matching (view, &keyring_predicate);

	for (l = keys; l; l = g_list_next (l)) {
		keyring = SEAHORSE_GKR_KEYRING (l->data);
		g_return_if_fail (SEAHORSE_IS_GKR_KEYRING (l->data));
		gnome_keyring_change_password (seahorse_gkr_keyring_get_name (l->data), NULL, NULL,
		                               on_change_password_done, g_object_ref (self), g_object_unref);
	}
}

static const GtkActionEntry ENTRIES_KEYRING[] = {
	{ "keyring-password", NULL, N_ ("Change Password"), "",
	  N_("Change the unlock password of the password storage keyring"), G_CALLBACK (on_keyring_password) }
};

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

static SeahorseOperation* 
seahorse_gkr_keyring_commands_delete_objects (SeahorseCommands* base, GList* objects) 
{
	gchar *prompt;
	
	if (!objects)
		return NULL;

	prompt = g_strdup_printf (_ ("Are you sure you want to delete the password keyring '%s'?"), 
	                          seahorse_object_get_label (objects->data));

	return seahorse_object_delete (objects->data);
}

static GObject* 
seahorse_gkr_keyring_commands_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	GObject *obj = G_OBJECT_CLASS (seahorse_gkr_keyring_commands_parent_class)->constructor (type, n_props, props);
	SeahorseCommands *base = NULL;
	GtkActionGroup *actions;
	SeahorseView *view;
	
	if (obj) {
		base = SEAHORSE_COMMANDS (obj);
		view = seahorse_commands_get_view (base);
		g_return_val_if_fail (view, NULL);
		seahorse_view_register_commands (view, &keyring_predicate, base);

		actions = gtk_action_group_new ("pgp");
		gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
		gtk_action_group_add_actions (actions, ENTRIES_KEYRING, G_N_ELEMENTS (ENTRIES_KEYRING), obj);
		seahorse_view_register_ui (view, &keyring_predicate, UI_KEYRING, actions);
		g_object_unref (actions);
	}

	return obj;
}

static void
seahorse_gkr_keyring_commands_init (SeahorseGkrKeyringCommands *self)
{

}

static void
seahorse_gkr_keyring_commands_class_init (SeahorseGkrKeyringCommandsClass *klass)
{
	GtkActionGroup *actions;
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseCommandsClass *cmd_class = SEAHORSE_COMMANDS_CLASS (klass);
	
	seahorse_gkr_keyring_commands_parent_class = g_type_class_peek_parent (klass);

	gobject_class->constructor = seahorse_gkr_keyring_commands_constructor;

	cmd_class->show_properties = seahorse_gkr_keyring_commands_show_properties;
	cmd_class->delete_objects = seahorse_gkr_keyring_commands_delete_objects;

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


