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

#include "seahorse-ssh-commands.h"
#include <glib/gi18n-lib.h>
#include <seahorse-ssh-dialogs.h>
#include <seahorse-ssh-key.h>
#include <seahorse-view.h>
#include <seahorse-util.h>
#include <seahorse-key-source.h>
#include <config.h>
#include <common/seahorse-registry.h>
#include "seahorse-ssh.h"




struct _SeahorseSSHCommandsPrivate {
	GtkActionGroup* _command_actions;
};

#define SEAHORSE_SSH_COMMANDS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_SSH_TYPE_COMMANDS, SeahorseSSHCommandsPrivate))
enum  {
	SEAHORSE_SSH_COMMANDS_DUMMY_PROPERTY,
	SEAHORSE_SSH_COMMANDS_KTYPE,
	SEAHORSE_SSH_COMMANDS_UI_DEFINITION,
	SEAHORSE_SSH_COMMANDS_COMMAND_ACTIONS
};
static void seahorse_ssh_commands_real_show_properties (SeahorseCommands* base, SeahorseKey* key);
static void seahorse_ssh_commands_real_delete_keys (SeahorseCommands* base, GList* keys, GError** error);
static void seahorse_ssh_commands_on_ssh_upload (SeahorseSSHCommands* self, GtkAction* action);
static void seahorse_ssh_commands_on_view_selection_changed (SeahorseSSHCommands* self, SeahorseView* view);
static void _seahorse_ssh_commands_on_ssh_upload_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_ssh_commands_on_view_selection_changed_seahorse_view_selection_changed (SeahorseView* _sender, gpointer self);
static GObject * seahorse_ssh_commands_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties);
static gpointer seahorse_ssh_commands_parent_class = NULL;
static void seahorse_ssh_commands_dispose (GObject * obj);

static const GtkActionEntry SEAHORSE_SSH_COMMANDS_COMMAND_ENTRIES[] = {{"remote-ssh-upload", NULL, N_ ("Configure Key for _Secure Shell..."), "", N_ ("Send public Secure Shell key to another machine, and enable logins using that key."), ((GCallback) (NULL))}};
static const char* SEAHORSE_SSH_COMMANDS_UI_DEF = "\n\t\t\t<ui>\n\t\t\t\n\t\t\t<menubar>\n\t\t\t\t<menu name='Remote' action='remote-menu'>\n\t\t\t\t\t<menuitem action='remote-ssh-upload'/>\n\t\t\t\t</menu>\n\t\t\t</menubar>\n\t\t\t\n\t\t\t<popup name=\"KeyPopup\">\n\t\t\t\t<menuitem action=\"remote-ssh-upload\"/>\n\t\t\t</popup>\n\t\t\t\n\t\t\t</ui>\n\t\t";


static void seahorse_ssh_commands_real_show_properties (SeahorseCommands* base, SeahorseKey* key) {
	SeahorseSSHCommands * self;
	self = SEAHORSE_SSH_COMMANDS (base);
	g_return_if_fail (SEAHORSE_IS_KEY (key));
	g_return_if_fail (seahorse_key_get_ktype (key) == SEAHORSE_SSH_TYPE);
	seahorse_ssh_key_properties_show (SEAHORSE_SSH_KEY (key), seahorse_view_get_window (seahorse_commands_get_view (SEAHORSE_COMMANDS (self))));
}


static void seahorse_ssh_commands_real_delete_keys (SeahorseCommands* base, GList* keys, GError** error) {
	SeahorseSSHCommands * self;
	GError * inner_error;
	guint num;
	char* prompt;
	self = SEAHORSE_SSH_COMMANDS (base);
	g_return_if_fail (keys != NULL);
	inner_error = NULL;
	num = g_list_length (keys);
	if (num == 0) {
		return;
	}
	prompt = NULL;
	if (num == 1) {
		char* _tmp0;
		_tmp0 = NULL;
		prompt = (_tmp0 = g_strdup_printf (_ ("Are you sure you want to delete the secure shell key '%s'?"), seahorse_key_get_display_name (((SeahorseKey*) (SEAHORSE_KEY (((SeahorseSSHKey*) (keys->data))))))), (prompt = (g_free (prompt), NULL)), _tmp0);
	} else {
		char* _tmp1;
		_tmp1 = NULL;
		prompt = (_tmp1 = g_strdup_printf (_ ("Are you sure you want to delete %d secure shell keys?"), num), (prompt = (g_free (prompt), NULL)), _tmp1);
	}
	if (seahorse_util_prompt_delete (prompt)) {
		seahorse_key_source_delete_keys (keys, &inner_error);
		if (inner_error != NULL) {
			g_propagate_error (error, inner_error);
			prompt = (g_free (prompt), NULL);
			return;
		}
	}
	prompt = (g_free (prompt), NULL);
}


static void seahorse_ssh_commands_on_ssh_upload (SeahorseSSHCommands* self, GtkAction* action) {
	GList* ssh_keys;
	GList* keys;
	g_return_if_fail (SEAHORSE_SSH_IS_COMMANDS (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	ssh_keys = NULL;
	keys = seahorse_view_get_selected_keys (seahorse_commands_get_view (SEAHORSE_COMMANDS (self)));
	{
		GList* key_collection;
		GList* key_it;
		key_collection = keys;
		for (key_it = key_collection; key_it != NULL; key_it = key_it->next) {
			SeahorseSSHKey* _tmp0;
			SeahorseSSHKey* key;
			_tmp0 = NULL;
			key = (_tmp0 = ((SeahorseSSHKey*) (key_it->data)), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
			{
				if (seahorse_key_get_ktype (SEAHORSE_KEY (key)) == SEAHORSE_SSH_TYPE && seahorse_key_get_etype (SEAHORSE_KEY (key)) == SKEY_PRIVATE) {
					ssh_keys = g_list_append (ssh_keys, key);
				}
				(key == NULL ? NULL : (key = (g_object_unref (key), NULL)));
			}
		}
	}
	seahorse_ssh_upload_prompt (keys, seahorse_view_get_window (seahorse_commands_get_view (SEAHORSE_COMMANDS (self))));
	(ssh_keys == NULL ? NULL : (ssh_keys = (g_list_free (ssh_keys), NULL)));
	(keys == NULL ? NULL : (keys = (g_list_free (keys), NULL)));
}


static void seahorse_ssh_commands_on_view_selection_changed (SeahorseSSHCommands* self, SeahorseView* view) {
	GList* keys;
	gboolean enable;
	g_return_if_fail (SEAHORSE_SSH_IS_COMMANDS (self));
	g_return_if_fail (SEAHORSE_IS_VIEW (view));
	keys = seahorse_view_get_selected_keys (view);
	enable = (keys != NULL);
	{
		GList* key_collection;
		GList* key_it;
		key_collection = keys;
		for (key_it = key_collection; key_it != NULL; key_it = key_it->next) {
			SeahorseSSHKey* _tmp0;
			SeahorseSSHKey* key;
			_tmp0 = NULL;
			key = (_tmp0 = ((SeahorseSSHKey*) (key_it->data)), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
			{
				if (seahorse_key_get_ktype (SEAHORSE_KEY (key)) != SEAHORSE_SSH_TYPE || seahorse_key_get_etype (SEAHORSE_KEY (key)) != SKEY_PRIVATE) {
					enable = FALSE;
					(key == NULL ? NULL : (key = (g_object_unref (key), NULL)));
					break;
				}
				(key == NULL ? NULL : (key = (g_object_unref (key), NULL)));
			}
		}
	}
	gtk_action_group_set_sensitive (self->priv->_command_actions, enable);
	(keys == NULL ? NULL : (keys = (g_list_free (keys), NULL)));
}


SeahorseSSHCommands* seahorse_ssh_commands_new (void) {
	SeahorseSSHCommands * self;
	self = g_object_newv (SEAHORSE_SSH_TYPE_COMMANDS, 0, NULL);
	return self;
}


static GQuark seahorse_ssh_commands_real_get_ktype (SeahorseSSHCommands* self) {
	g_return_val_if_fail (SEAHORSE_SSH_IS_COMMANDS (self), 0U);
	return SEAHORSE_SSH_TYPE;
}


static char* seahorse_ssh_commands_real_get_ui_definition (SeahorseSSHCommands* self) {
	const char* _tmp0;
	g_return_val_if_fail (SEAHORSE_SSH_IS_COMMANDS (self), NULL);
	_tmp0 = NULL;
	return (_tmp0 = SEAHORSE_SSH_COMMANDS_UI_DEF, (_tmp0 == NULL ? NULL : g_strdup (_tmp0)));
}


static void _seahorse_ssh_commands_on_ssh_upload_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_ssh_commands_on_ssh_upload (self, _sender);
}


static GtkActionGroup* seahorse_ssh_commands_real_get_command_actions (SeahorseSSHCommands* self) {
	g_return_val_if_fail (SEAHORSE_SSH_IS_COMMANDS (self), NULL);
	if (self->priv->_command_actions == NULL) {
		GtkActionGroup* _tmp0;
		_tmp0 = NULL;
		self->priv->_command_actions = (_tmp0 = gtk_action_group_new ("ssh"), (self->priv->_command_actions == NULL ? NULL : (self->priv->_command_actions = (g_object_unref (self->priv->_command_actions), NULL))), _tmp0);
		gtk_action_group_set_translation_domain (self->priv->_command_actions, GETTEXT_PACKAGE);
		gtk_action_group_add_actions (self->priv->_command_actions, SEAHORSE_SSH_COMMANDS_COMMAND_ENTRIES, G_N_ELEMENTS (SEAHORSE_SSH_COMMANDS_COMMAND_ENTRIES), self);
		g_signal_connect_object (gtk_action_group_get_action (self->priv->_command_actions, "remote-ssh-upload"), "activate", ((GCallback) (_seahorse_ssh_commands_on_ssh_upload_gtk_action_activate)), self, 0);
	}
	return self->priv->_command_actions;
}


static void _seahorse_ssh_commands_on_view_selection_changed_seahorse_view_selection_changed (SeahorseView* _sender, gpointer self) {
	seahorse_ssh_commands_on_view_selection_changed (self, _sender);
}


static GObject * seahorse_ssh_commands_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties) {
	GObject * obj;
	SeahorseSSHCommandsClass * klass;
	GObjectClass * parent_class;
	SeahorseSSHCommands * self;
	klass = SEAHORSE_SSH_COMMANDS_CLASS (g_type_class_peek (SEAHORSE_SSH_TYPE_COMMANDS));
	parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
	obj = parent_class->constructor (type, n_construct_properties, construct_properties);
	self = SEAHORSE_SSH_COMMANDS (obj);
	{
		g_assert (seahorse_commands_get_view (SEAHORSE_COMMANDS (self)) != NULL);
		g_signal_connect_object (seahorse_commands_get_view (SEAHORSE_COMMANDS (self)), "selection-changed", ((GCallback) (_seahorse_ssh_commands_on_view_selection_changed_seahorse_view_selection_changed)), self, 0);
	}
	return obj;
}


static void seahorse_ssh_commands_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SeahorseSSHCommands * self;
	self = SEAHORSE_SSH_COMMANDS (object);
	switch (property_id) {
		case SEAHORSE_SSH_COMMANDS_KTYPE:
		g_value_set_uint (value, seahorse_ssh_commands_real_get_ktype (self));
		break;
		case SEAHORSE_SSH_COMMANDS_UI_DEFINITION:
		g_value_set_string (value, seahorse_ssh_commands_real_get_ui_definition (self));
		break;
		case SEAHORSE_SSH_COMMANDS_COMMAND_ACTIONS:
		g_value_set_object (value, seahorse_ssh_commands_real_get_command_actions (self));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_ssh_commands_class_init (SeahorseSSHCommandsClass * klass) {
	seahorse_ssh_commands_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseSSHCommandsPrivate));
	G_OBJECT_CLASS (klass)->get_property = seahorse_ssh_commands_get_property;
	G_OBJECT_CLASS (klass)->constructor = seahorse_ssh_commands_constructor;
	G_OBJECT_CLASS (klass)->dispose = seahorse_ssh_commands_dispose;
	SEAHORSE_COMMANDS_CLASS (klass)->show_properties = seahorse_ssh_commands_real_show_properties;
	SEAHORSE_COMMANDS_CLASS (klass)->delete_keys = seahorse_ssh_commands_real_delete_keys;
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_SSH_COMMANDS_KTYPE, "ktype");
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_SSH_COMMANDS_UI_DEFINITION, "ui-definition");
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_SSH_COMMANDS_COMMAND_ACTIONS, "command-actions");
	{
		/* Register this class as a commands */
		seahorse_registry_register_type (seahorse_registry_get (), SEAHORSE_SSH_TYPE_COMMANDS, SEAHORSE_SSH_TYPE_STR, "commands", NULL, NULL);
	}
}


static void seahorse_ssh_commands_instance_init (SeahorseSSHCommands * self) {
	self->priv = SEAHORSE_SSH_COMMANDS_GET_PRIVATE (self);
	self->priv->_command_actions = NULL;
}


static void seahorse_ssh_commands_dispose (GObject * obj) {
	SeahorseSSHCommands * self;
	self = SEAHORSE_SSH_COMMANDS (obj);
	(self->priv->_command_actions == NULL ? NULL : (self->priv->_command_actions = (g_object_unref (self->priv->_command_actions), NULL)));
	G_OBJECT_CLASS (seahorse_ssh_commands_parent_class)->dispose (obj);
}


GType seahorse_ssh_commands_get_type (void) {
	static GType seahorse_ssh_commands_type_id = 0;
	if (G_UNLIKELY (seahorse_ssh_commands_type_id == 0)) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorseSSHCommandsClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_ssh_commands_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorseSSHCommands), 0, (GInstanceInitFunc) seahorse_ssh_commands_instance_init };
		seahorse_ssh_commands_type_id = g_type_register_static (SEAHORSE_TYPE_COMMANDS, "SeahorseSSHCommands", &g_define_type_info, 0);
	}
	return seahorse_ssh_commands_type_id;
}




