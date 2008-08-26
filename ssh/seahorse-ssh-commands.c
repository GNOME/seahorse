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
#include <seahorse-source.h>
#include <seahorse-types.h>
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
static void seahorse_ssh_commands_real_show_properties (SeahorseCommands* base, SeahorseObject* key);
static SeahorseOperation* seahorse_ssh_commands_real_delete_objects (SeahorseCommands* base, GList* keys);
static void seahorse_ssh_commands_on_ssh_upload (SeahorseSSHCommands* self, GtkAction* action);
static void seahorse_ssh_commands_on_view_selection_changed (SeahorseSSHCommands* self, SeahorseView* view);
static void _seahorse_ssh_commands_on_ssh_upload_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_ssh_commands_on_view_selection_changed_seahorse_view_selection_changed (SeahorseView* _sender, gpointer self);
static GObject * seahorse_ssh_commands_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties);
static gpointer seahorse_ssh_commands_parent_class = NULL;
static void seahorse_ssh_commands_finalize (GObject * obj);

static const GtkActionEntry SEAHORSE_SSH_COMMANDS_COMMAND_ENTRIES[] = {{"remote-ssh-upload", NULL, N_ ("Configure Key for _Secure Shell..."), "", N_ ("Send public Secure Shell key to another machine, and enable logins using that key."), ((GCallback) (NULL))}};
static const char* SEAHORSE_SSH_COMMANDS_UI_DEF = "\n\t\t\t<ui>\n\t\t\t\n\t\t\t<menubar>\n\t\t\t\t<menu name='Remote' action='remote-menu'>\n\t\t\t\t\t<menuitem action='remote-ssh-upload'/>\n\t\t\t\t</menu>\n\t\t\t</menubar>\n\t\t\t\n\t\t\t<popup name=\"KeyPopup\">\n\t\t\t\t<menuitem action=\"remote-ssh-upload\"/>\n\t\t\t</popup>\n\t\t\t\n\t\t\t</ui>\n\t\t";


static void seahorse_ssh_commands_real_show_properties (SeahorseCommands* base, SeahorseObject* key) {
	SeahorseSSHCommands * self;
	self = SEAHORSE_SSH_COMMANDS (base);
	g_return_if_fail (SEAHORSE_IS_OBJECT (key));
	g_return_if_fail (seahorse_object_get_tag (key) == SEAHORSE_SSH_TYPE);
	seahorse_ssh_key_properties_show (SEAHORSE_SSH_KEY (key), seahorse_view_get_window (seahorse_commands_get_view (SEAHORSE_COMMANDS (self))));
}


static SeahorseOperation* seahorse_ssh_commands_real_delete_objects (SeahorseCommands* base, GList* keys) {
	SeahorseSSHCommands * self;
	guint num;
	char* prompt;
	SeahorseOperation* _tmp4;
	self = SEAHORSE_SSH_COMMANDS (base);
	g_return_val_if_fail (keys != NULL, NULL);
	num = g_list_length (keys);
	if (num == 0) {
		return NULL;
	}
	prompt = NULL;
	if (num == 1) {
		char* _tmp1;
		_tmp1 = NULL;
		prompt = (_tmp1 = g_strdup_printf (_ ("Are you sure you want to delete the secure shell key '%s'?"), seahorse_object_get_display_name (((SeahorseObject*) (((SeahorseObject*) (keys->data)))))), (prompt = (g_free (prompt), NULL)), _tmp1);
	} else {
		char* _tmp2;
		_tmp2 = NULL;
		prompt = (_tmp2 = g_strdup_printf (_ ("Are you sure you want to delete %d secure shell keys?"), num), (prompt = (g_free (prompt), NULL)), _tmp2);
	}
	if (!seahorse_util_prompt_delete (prompt, NULL)) {
		SeahorseOperation* _tmp3;
		_tmp3 = NULL;
		return (_tmp3 = NULL, (prompt = (g_free (prompt), NULL)), _tmp3);
	}
	_tmp4 = NULL;
	return (_tmp4 = seahorse_source_delete_objects (keys), (prompt = (g_free (prompt), NULL)), _tmp4);
}


static void seahorse_ssh_commands_on_ssh_upload (SeahorseSSHCommands* self, GtkAction* action) {
	GList* ssh_keys;
	GList* keys;
	g_return_if_fail (SEAHORSE_SSH_IS_COMMANDS (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	ssh_keys = NULL;
	keys = seahorse_view_get_selected_objects (seahorse_commands_get_view (SEAHORSE_COMMANDS (self)));
	{
		GList* key_collection;
		GList* key_it;
		key_collection = keys;
		for (key_it = key_collection; key_it != NULL; key_it = key_it->next) {
			SeahorseObject* key;
			key = ((SeahorseObject*) (key_it->data));
			{
				if (seahorse_object_get_tag (key) == SEAHORSE_SSH_TYPE && seahorse_object_get_usage (key) == SEAHORSE_USAGE_PRIVATE_KEY) {
					ssh_keys = g_list_append (ssh_keys, SEAHORSE_SSH_KEY (key));
				}
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
	keys = seahorse_view_get_selected_objects (view);
	enable = (keys != NULL);
	{
		GList* key_collection;
		GList* key_it;
		key_collection = keys;
		for (key_it = key_collection; key_it != NULL; key_it = key_it->next) {
			SeahorseObject* key;
			key = ((SeahorseObject*) (key_it->data));
			{
				if (seahorse_object_get_tag (key) != SEAHORSE_SSH_TYPE || seahorse_object_get_usage (key) != SEAHORSE_USAGE_PRIVATE_KEY) {
					enable = FALSE;
					break;
				}
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


static GQuark seahorse_ssh_commands_real_get_ktype (SeahorseCommands* base) {
	SeahorseSSHCommands* self;
	self = SEAHORSE_SSH_COMMANDS (base);
	return SEAHORSE_SSH_TYPE;
}


static char* seahorse_ssh_commands_real_get_ui_definition (SeahorseCommands* base) {
	SeahorseSSHCommands* self;
	const char* _tmp0;
	self = SEAHORSE_SSH_COMMANDS (base);
	_tmp0 = NULL;
	return (_tmp0 = SEAHORSE_SSH_COMMANDS_UI_DEF, (_tmp0 == NULL ? NULL : g_strdup (_tmp0)));
}


static void _seahorse_ssh_commands_on_ssh_upload_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_ssh_commands_on_ssh_upload (self, _sender);
}


static GtkActionGroup* seahorse_ssh_commands_real_get_command_actions (SeahorseCommands* base) {
	SeahorseSSHCommands* self;
	self = SEAHORSE_SSH_COMMANDS (base);
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
		g_value_set_uint (value, seahorse_commands_get_ktype (SEAHORSE_COMMANDS (self)));
		break;
		case SEAHORSE_SSH_COMMANDS_UI_DEFINITION:
		g_value_set_string (value, seahorse_commands_get_ui_definition (SEAHORSE_COMMANDS (self)));
		break;
		case SEAHORSE_SSH_COMMANDS_COMMAND_ACTIONS:
		g_value_set_object (value, seahorse_commands_get_command_actions (SEAHORSE_COMMANDS (self)));
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
	G_OBJECT_CLASS (klass)->finalize = seahorse_ssh_commands_finalize;
	SEAHORSE_COMMANDS_CLASS (klass)->show_properties = seahorse_ssh_commands_real_show_properties;
	SEAHORSE_COMMANDS_CLASS (klass)->delete_objects = seahorse_ssh_commands_real_delete_objects;
	SEAHORSE_COMMANDS_CLASS (klass)->get_ktype = seahorse_ssh_commands_real_get_ktype;
	SEAHORSE_COMMANDS_CLASS (klass)->get_ui_definition = seahorse_ssh_commands_real_get_ui_definition;
	SEAHORSE_COMMANDS_CLASS (klass)->get_command_actions = seahorse_ssh_commands_real_get_command_actions;
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


static void seahorse_ssh_commands_finalize (GObject * obj) {
	SeahorseSSHCommands * self;
	self = SEAHORSE_SSH_COMMANDS (obj);
	(self->priv->_command_actions == NULL ? NULL : (self->priv->_command_actions = (g_object_unref (self->priv->_command_actions), NULL)));
	G_OBJECT_CLASS (seahorse_ssh_commands_parent_class)->finalize (obj);
}


GType seahorse_ssh_commands_get_type (void) {
	static GType seahorse_ssh_commands_type_id = 0;
	if (seahorse_ssh_commands_type_id == 0) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorseSSHCommandsClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_ssh_commands_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorseSSHCommands), 0, (GInstanceInitFunc) seahorse_ssh_commands_instance_init };
		seahorse_ssh_commands_type_id = g_type_register_static (SEAHORSE_TYPE_COMMANDS, "SeahorseSSHCommands", &g_define_type_info, 0);
	}
	return seahorse_ssh_commands_type_id;
}




