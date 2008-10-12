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

#include "seahorse-gkr-commands.h"
#include <seahorse-gkr-dialogs.h>
#include <seahorse-gkeyring-item.h>
#include <seahorse-view.h>
#include <glib/gi18n-lib.h>
#include <seahorse-util.h>
#include <seahorse-source.h>
#include <common/seahorse-registry.h>
#include "seahorse-gkr.h"




struct _SeahorseGKeyringCommandsPrivate {
	GtkActionGroup* _actions;
};

#define SEAHORSE_GKEYRING_COMMANDS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_GKEYRING_TYPE_COMMANDS, SeahorseGKeyringCommandsPrivate))
enum  {
	SEAHORSE_GKEYRING_COMMANDS_DUMMY_PROPERTY,
	SEAHORSE_GKEYRING_COMMANDS_KTYPE,
	SEAHORSE_GKEYRING_COMMANDS_UI_DEFINITION,
	SEAHORSE_GKEYRING_COMMANDS_COMMAND_ACTIONS
};
static void seahorse_gkeyring_commands_real_show_properties (SeahorseCommands* base, SeahorseObject* key);
static SeahorseOperation* seahorse_gkeyring_commands_real_delete_objects (SeahorseCommands* base, GList* keys);
static gpointer seahorse_gkeyring_commands_parent_class = NULL;
static void seahorse_gkeyring_commands_finalize (GObject * obj);



static void seahorse_gkeyring_commands_real_show_properties (SeahorseCommands* base, SeahorseObject* key) {
	SeahorseGKeyringCommands * self;
	self = SEAHORSE_GKEYRING_COMMANDS (base);
	g_return_if_fail (SEAHORSE_IS_OBJECT (key));
	g_return_if_fail (seahorse_object_get_tag (key) == SEAHORSE_GKEYRING_TYPE);
	seahorse_gkeyring_item_properties_show (SEAHORSE_GKEYRING_ITEM (key), seahorse_view_get_window (seahorse_commands_get_view (SEAHORSE_COMMANDS (self))));
}


static SeahorseOperation* seahorse_gkeyring_commands_real_delete_objects (SeahorseCommands* base, GList* keys) {
	SeahorseGKeyringCommands * self;
	guint num;
	char* prompt;
	SeahorseOperation* _tmp4;
	self = SEAHORSE_GKEYRING_COMMANDS (base);
	g_return_val_if_fail (keys != NULL, NULL);
	num = g_list_length (keys);
	if (num == 0) {
		return NULL;
	}
	prompt = NULL;
	if (num == 1) {
		char* _tmp1;
		_tmp1 = NULL;
		prompt = (_tmp1 = g_strdup_printf (_ ("Are you sure you want to delete the password '%s'?"), seahorse_object_get_display_name (((SeahorseObject*) (((SeahorseObject*) (keys->data)))))), (prompt = (g_free (prompt), NULL)), _tmp1);
	} else {
		char* _tmp2;
		_tmp2 = NULL;
		prompt = (_tmp2 = g_strdup_printf (_ ("Are you sure you want to delete %d passwords?"), num), (prompt = (g_free (prompt), NULL)), _tmp2);
	}
	if (!seahorse_util_prompt_delete (prompt, GTK_WIDGET (seahorse_view_get_window (seahorse_commands_get_view (SEAHORSE_COMMANDS (self)))))) {
		SeahorseOperation* _tmp3;
		_tmp3 = NULL;
		return (_tmp3 = NULL, (prompt = (g_free (prompt), NULL)), _tmp3);
	}
	_tmp4 = NULL;
	return (_tmp4 = seahorse_source_delete_objects (keys), (prompt = (g_free (prompt), NULL)), _tmp4);
}


SeahorseGKeyringCommands* seahorse_gkeyring_commands_new (void) {
	SeahorseGKeyringCommands * self;
	self = g_object_newv (SEAHORSE_GKEYRING_TYPE_COMMANDS, 0, NULL);
	return self;
}


static GQuark seahorse_gkeyring_commands_real_get_ktype (SeahorseCommands* base) {
	SeahorseGKeyringCommands* self;
	self = SEAHORSE_GKEYRING_COMMANDS (base);
	return SEAHORSE_GKEYRING_TYPE;
}


static const char* seahorse_gkeyring_commands_real_get_ui_definition (SeahorseCommands* base) {
	SeahorseGKeyringCommands* self;
	self = SEAHORSE_GKEYRING_COMMANDS (base);
	return "";
}


static GtkActionGroup* seahorse_gkeyring_commands_real_get_command_actions (SeahorseCommands* base) {
	SeahorseGKeyringCommands* self;
	self = SEAHORSE_GKEYRING_COMMANDS (base);
	if (self->priv->_actions == NULL) {
		GtkActionGroup* _tmp0;
		_tmp0 = NULL;
		self->priv->_actions = (_tmp0 = gtk_action_group_new ("gkr"), (self->priv->_actions == NULL ? NULL : (self->priv->_actions = (g_object_unref (self->priv->_actions), NULL))), _tmp0);
	}
	return self->priv->_actions;
}


static void seahorse_gkeyring_commands_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SeahorseGKeyringCommands * self;
	self = SEAHORSE_GKEYRING_COMMANDS (object);
	switch (property_id) {
		case SEAHORSE_GKEYRING_COMMANDS_KTYPE:
		g_value_set_uint (value, seahorse_commands_get_ktype (SEAHORSE_COMMANDS (self)));
		break;
		case SEAHORSE_GKEYRING_COMMANDS_UI_DEFINITION:
		g_value_set_string (value, seahorse_commands_get_ui_definition (SEAHORSE_COMMANDS (self)));
		break;
		case SEAHORSE_GKEYRING_COMMANDS_COMMAND_ACTIONS:
		g_value_set_object (value, seahorse_commands_get_command_actions (SEAHORSE_COMMANDS (self)));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_gkeyring_commands_class_init (SeahorseGKeyringCommandsClass * klass) {
	seahorse_gkeyring_commands_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseGKeyringCommandsPrivate));
	G_OBJECT_CLASS (klass)->get_property = seahorse_gkeyring_commands_get_property;
	G_OBJECT_CLASS (klass)->finalize = seahorse_gkeyring_commands_finalize;
	SEAHORSE_COMMANDS_CLASS (klass)->show_properties = seahorse_gkeyring_commands_real_show_properties;
	SEAHORSE_COMMANDS_CLASS (klass)->delete_objects = seahorse_gkeyring_commands_real_delete_objects;
	SEAHORSE_COMMANDS_CLASS (klass)->get_ktype = seahorse_gkeyring_commands_real_get_ktype;
	SEAHORSE_COMMANDS_CLASS (klass)->get_ui_definition = seahorse_gkeyring_commands_real_get_ui_definition;
	SEAHORSE_COMMANDS_CLASS (klass)->get_command_actions = seahorse_gkeyring_commands_real_get_command_actions;
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_GKEYRING_COMMANDS_KTYPE, "ktype");
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_GKEYRING_COMMANDS_UI_DEFINITION, "ui-definition");
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_GKEYRING_COMMANDS_COMMAND_ACTIONS, "command-actions");
	{
		/* Register this class as a commands */
		seahorse_registry_register_type (seahorse_registry_get (), SEAHORSE_GKEYRING_TYPE_COMMANDS, SEAHORSE_GKEYRING_TYPE_STR, "commands", NULL, NULL);
	}
}


static void seahorse_gkeyring_commands_instance_init (SeahorseGKeyringCommands * self) {
	self->priv = SEAHORSE_GKEYRING_COMMANDS_GET_PRIVATE (self);
}


static void seahorse_gkeyring_commands_finalize (GObject * obj) {
	SeahorseGKeyringCommands * self;
	self = SEAHORSE_GKEYRING_COMMANDS (obj);
	(self->priv->_actions == NULL ? NULL : (self->priv->_actions = (g_object_unref (self->priv->_actions), NULL)));
	G_OBJECT_CLASS (seahorse_gkeyring_commands_parent_class)->finalize (obj);
}


GType seahorse_gkeyring_commands_get_type (void) {
	static GType seahorse_gkeyring_commands_type_id = 0;
	if (seahorse_gkeyring_commands_type_id == 0) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorseGKeyringCommandsClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_gkeyring_commands_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorseGKeyringCommands), 0, (GInstanceInitFunc) seahorse_gkeyring_commands_instance_init };
		seahorse_gkeyring_commands_type_id = g_type_register_static (SEAHORSE_TYPE_COMMANDS, "SeahorseGKeyringCommands", &g_define_type_info, 0);
	}
	return seahorse_gkeyring_commands_type_id;
}




