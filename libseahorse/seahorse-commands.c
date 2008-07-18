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

#include <seahorse-commands.h>




struct _SeahorseCommandsPrivate {
	SeahorseView* _view;
};

#define SEAHORSE_COMMANDS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_TYPE_COMMANDS, SeahorseCommandsPrivate))
enum  {
	SEAHORSE_COMMANDS_DUMMY_PROPERTY,
	SEAHORSE_COMMANDS_VIEW,
	SEAHORSE_COMMANDS_KTYPE,
	SEAHORSE_COMMANDS_COMMAND_ACTIONS,
	SEAHORSE_COMMANDS_UI_DEFINITION
};
static void seahorse_commands_real_show_properties (SeahorseCommands* self, SeahorseObject* obj);
static void seahorse_commands_real_delete_objects (SeahorseCommands* self, GList* obj, GError** error);
static void seahorse_commands_set_view (SeahorseCommands* self, SeahorseView* value);
static gpointer seahorse_commands_parent_class = NULL;
static void seahorse_commands_dispose (GObject * obj);



static void seahorse_commands_real_show_properties (SeahorseCommands* self, SeahorseObject* obj) {
	g_return_if_fail (SEAHORSE_IS_COMMANDS (self));
	g_critical ("Type `%s' does not implement abstract method `seahorse_commands_show_properties'", g_type_name (G_TYPE_FROM_INSTANCE (self)));
	return;
}


void seahorse_commands_show_properties (SeahorseCommands* self, SeahorseObject* obj) {
	SEAHORSE_COMMANDS_GET_CLASS (self)->show_properties (self, obj);
}


static void seahorse_commands_real_delete_objects (SeahorseCommands* self, GList* obj, GError** error) {
	g_return_if_fail (SEAHORSE_IS_COMMANDS (self));
	g_critical ("Type `%s' does not implement abstract method `seahorse_commands_delete_objects'", g_type_name (G_TYPE_FROM_INSTANCE (self)));
	return;
}


void seahorse_commands_delete_objects (SeahorseCommands* self, GList* obj, GError** error) {
	SEAHORSE_COMMANDS_GET_CLASS (self)->delete_objects (self, obj, error);
}


SeahorseView* seahorse_commands_get_view (SeahorseCommands* self) {
	g_return_val_if_fail (SEAHORSE_IS_COMMANDS (self), NULL);
	return self->priv->_view;
}


static void seahorse_commands_set_view (SeahorseCommands* self, SeahorseView* value) {
	SeahorseView* _tmp2;
	SeahorseView* _tmp1;
	g_return_if_fail (SEAHORSE_IS_COMMANDS (self));
	_tmp2 = NULL;
	_tmp1 = NULL;
	self->priv->_view = (_tmp2 = (_tmp1 = value, (_tmp1 == NULL ? NULL : g_object_ref (_tmp1))), (self->priv->_view == NULL ? NULL : (self->priv->_view = (g_object_unref (self->priv->_view), NULL))), _tmp2);
	g_object_notify (((GObject *) (self)), "view");
}


GQuark seahorse_commands_get_ktype (SeahorseCommands* self) {
	GQuark value;
	g_object_get (G_OBJECT (self), "ktype", &value, NULL);
	return value;
}


GtkActionGroup* seahorse_commands_get_command_actions (SeahorseCommands* self) {
	GtkActionGroup* value;
	g_object_get (G_OBJECT (self), "command-actions", &value, NULL);
	if (value != NULL) {
		g_object_unref (value);
	}
	return value;
}


char* seahorse_commands_get_ui_definition (SeahorseCommands* self) {
	char* value;
	g_object_get (G_OBJECT (self), "ui-definition", &value, NULL);
	return value;
}


static void seahorse_commands_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SeahorseCommands * self;
	self = SEAHORSE_COMMANDS (object);
	switch (property_id) {
		case SEAHORSE_COMMANDS_VIEW:
		g_value_set_object (value, seahorse_commands_get_view (self));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_commands_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
	SeahorseCommands * self;
	self = SEAHORSE_COMMANDS (object);
	switch (property_id) {
		case SEAHORSE_COMMANDS_VIEW:
		seahorse_commands_set_view (self, g_value_get_object (value));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_commands_class_init (SeahorseCommandsClass * klass) {
	seahorse_commands_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseCommandsPrivate));
	G_OBJECT_CLASS (klass)->get_property = seahorse_commands_get_property;
	G_OBJECT_CLASS (klass)->set_property = seahorse_commands_set_property;
	G_OBJECT_CLASS (klass)->dispose = seahorse_commands_dispose;
	SEAHORSE_COMMANDS_CLASS (klass)->show_properties = seahorse_commands_real_show_properties;
	SEAHORSE_COMMANDS_CLASS (klass)->delete_objects = seahorse_commands_real_delete_objects;
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_COMMANDS_VIEW, g_param_spec_object ("view", "view", "view", SEAHORSE_TYPE_VIEW, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_COMMANDS_KTYPE, g_param_spec_uint ("ktype", "ktype", "ktype", 0, G_MAXUINT, 0U, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_COMMANDS_COMMAND_ACTIONS, g_param_spec_object ("command-actions", "command-actions", "command-actions", GTK_TYPE_ACTION_GROUP, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_COMMANDS_UI_DEFINITION, g_param_spec_string ("ui-definition", "ui-definition", "ui-definition", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
}


static void seahorse_commands_instance_init (SeahorseCommands * self) {
	self->priv = SEAHORSE_COMMANDS_GET_PRIVATE (self);
}


static void seahorse_commands_dispose (GObject * obj) {
	SeahorseCommands * self;
	self = SEAHORSE_COMMANDS (obj);
	(self->priv->_view == NULL ? NULL : (self->priv->_view = (g_object_unref (self->priv->_view), NULL)));
	G_OBJECT_CLASS (seahorse_commands_parent_class)->dispose (obj);
}


GType seahorse_commands_get_type (void) {
	static GType seahorse_commands_type_id = 0;
	if (G_UNLIKELY (seahorse_commands_type_id == 0)) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorseCommandsClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_commands_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorseCommands), 0, (GInstanceInitFunc) seahorse_commands_instance_init };
		seahorse_commands_type_id = g_type_register_static (G_TYPE_OBJECT, "SeahorseCommands", &g_define_type_info, G_TYPE_FLAG_ABSTRACT);
	}
	return seahorse_commands_type_id;
}




