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

#include "seahorse-pgp-commands.h"
#include <glib/gi18n-lib.h>
#include <seahorse-pgp-dialogs.h>
#include <seahorse-pgp-key.h>
#include <seahorse-view.h>
#include <seahorse-pgp-uid.h>
#include <config.h>
#include <common/seahorse-registry.h>
#include "seahorse-pgp.h"




struct _SeahorsePGPCommandsPrivate {
	GtkActionGroup* _command_actions;
};

#define SEAHORSE_PGP_COMMANDS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_PGP_TYPE_COMMANDS, SeahorsePGPCommandsPrivate))
enum  {
	SEAHORSE_PGP_COMMANDS_DUMMY_PROPERTY,
	SEAHORSE_PGP_COMMANDS_KTYPE,
	SEAHORSE_PGP_COMMANDS_UI_DEFINITION,
	SEAHORSE_PGP_COMMANDS_COMMAND_ACTIONS
};
static void seahorse_pgp_commands_real_show_properties (SeahorseCommands* base, SeahorseObject* obj);
static void seahorse_pgp_commands_real_delete_objects (SeahorseCommands* base, GList* objects, GError** error);
static void seahorse_pgp_commands_on_key_sign (SeahorsePGPCommands* self, GtkAction* action);
static void seahorse_pgp_commands_on_view_selection_changed (SeahorsePGPCommands* self, SeahorseView* view);
static void _seahorse_pgp_commands_on_key_sign_gtk_action_activate (GtkAction* _sender, gpointer self);
static void _seahorse_pgp_commands_on_view_selection_changed_seahorse_view_selection_changed (SeahorseView* _sender, gpointer self);
static GObject * seahorse_pgp_commands_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties);
static gpointer seahorse_pgp_commands_parent_class = NULL;
static void seahorse_pgp_commands_dispose (GObject * obj);

static const GtkActionEntry SEAHORSE_PGP_COMMANDS_COMMAND_ENTRIES[] = {{"key-sign", GTK_STOCK_INDEX, N_ ("_Sign Key..."), "", N_ ("Sign public key"), ((GCallback) (NULL))}};
static const char* SEAHORSE_PGP_COMMANDS_UI_DEF = "\n\t\t\t<ui>\n\t\t\t\n\t\t\t<menubar>\n\t\t\t\t<menu name='Key' action='key-menu'>\n\t\t\t\t\t<placeholder name=\"KeyCommands\">\n\t\t\t\t\t\t<menuitem action=\"key-sign\"/>\n\t\t\t\t\t</placeholder>\n\t\t\t\t</menu>\n\t\t\t</menubar>\n\t\t\t\n\t\t\t<toolbar name=\"MainToolbar\">\n\t\t\t\t<placeholder name=\"ToolItems\">\n\t\t\t\t\t<toolitem action=\"key-sign\"/>\n\t\t\t\t</placeholder>\n\t\t\t</toolbar>\n\n\t\t\t<popup name=\"KeyPopup\">\n\t\t\t\t<menuitem action=\"key-sign\"/>\n\t\t\t</popup>\n    \n\t\t\t</ui>\n\t\t";


static void seahorse_pgp_commands_real_show_properties (SeahorseCommands* base, SeahorseObject* obj) {
	SeahorsePGPCommands * self;
	self = SEAHORSE_PGP_COMMANDS (base);
	g_return_if_fail (SEAHORSE_IS_OBJECT (obj));
	g_return_if_fail (seahorse_object_get_tag (obj) == SEAHORSE_PGP_TYPE);
	if (G_TYPE_FROM_INSTANCE (G_OBJECT (obj)) == SEAHORSE_PGP_TYPE_UID) {
		obj = seahorse_object_get_parent (obj);
	}
	g_return_if_fail (G_TYPE_FROM_INSTANCE (G_OBJECT (obj)) == SEAHORSE_PGP_TYPE_KEY);
	seahorse_pgp_key_properties_show (SEAHORSE_PGP_KEY (obj), seahorse_view_get_window (seahorse_commands_get_view (SEAHORSE_COMMANDS (self))));
}


static void seahorse_pgp_commands_real_delete_objects (SeahorseCommands* base, GList* objects, GError** error) {
	SeahorsePGPCommands * self;
	guint num;
	self = SEAHORSE_PGP_COMMANDS (base);
	g_return_if_fail (objects != NULL);
	num = g_list_length (objects);
	if (num == 0) {
		return;
	}
	{
		GList* obj_collection;
		GList* obj_it;
		obj_collection = objects;
		for (obj_it = obj_collection; obj_it != NULL; obj_it = obj_it->next) {
			SeahorseObject* _tmp1;
			SeahorseObject* obj;
			_tmp1 = NULL;
			obj = (_tmp1 = ((SeahorseObject*) (obj_it->data)), (_tmp1 == NULL ? NULL : g_object_ref (_tmp1)));
			{
				/* 
				 * Delete all the user ids first, if parent key is 
				 * not on the chopping block already.
				 */
				if (G_TYPE_FROM_INSTANCE (G_OBJECT (obj)) == SEAHORSE_PGP_TYPE_UID) {
					SeahorsePGPUid* _tmp0;
					SeahorsePGPUid* uid;
					_tmp0 = NULL;
					uid = (_tmp0 = SEAHORSE_PGP_UID (obj), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
					if (g_list_find (objects, seahorse_object_get_parent (SEAHORSE_OBJECT (uid))) == NULL) {
						seahorse_pgp_delete_userid_show (SEAHORSE_PGP_KEY (seahorse_object_get_parent (SEAHORSE_OBJECT (uid))), seahorse_pgp_uid_get_index (uid));
					}
					objects = g_list_remove (objects, obj);
					(uid == NULL ? NULL : (uid = (g_object_unref (uid), NULL)));
				} else {
					g_return_if_fail (G_TYPE_FROM_INSTANCE (G_OBJECT (obj)) != SEAHORSE_PGP_TYPE_KEY);
				}
				(obj == NULL ? NULL : (obj = (g_object_unref (obj), NULL)));
			}
		}
	}
	seahorse_pgp_delete_show (objects);
}


static void seahorse_pgp_commands_on_key_sign (SeahorsePGPCommands* self, GtkAction* action) {
	SeahorseObject* _tmp0;
	SeahorseObject* key;
	g_return_if_fail (SEAHORSE_PGP_IS_COMMANDS (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	_tmp0 = NULL;
	key = (_tmp0 = seahorse_view_get_selected (seahorse_commands_get_view (SEAHORSE_COMMANDS (self))), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	/* TODO: Make signing a specific UID work again */
	if (key != NULL && seahorse_object_get_tag (key) == SEAHORSE_PGP_TYPE) {
		seahorse_pgp_sign_prompt (SEAHORSE_PGP_KEY (key), ((guint) (0)), seahorse_view_get_window (seahorse_commands_get_view (SEAHORSE_COMMANDS (self))));
	}
	(key == NULL ? NULL : (key = (g_object_unref (key), NULL)));
}


static void seahorse_pgp_commands_on_view_selection_changed (SeahorsePGPCommands* self, SeahorseView* view) {
	GList* keys;
	gboolean enable;
	g_return_if_fail (SEAHORSE_PGP_IS_COMMANDS (self));
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
				if (seahorse_object_get_tag (key) != SEAHORSE_PGP_TYPE) {
					enable = FALSE;
					break;
				}
			}
		}
	}
	gtk_action_group_set_sensitive (self->priv->_command_actions, enable);
	(keys == NULL ? NULL : (keys = (g_list_free (keys), NULL)));
}


SeahorsePGPCommands* seahorse_pgp_commands_new (void) {
	SeahorsePGPCommands * self;
	self = g_object_newv (SEAHORSE_PGP_TYPE_COMMANDS, 0, NULL);
	return self;
}


static GQuark seahorse_pgp_commands_real_get_ktype (SeahorsePGPCommands* self) {
	g_return_val_if_fail (SEAHORSE_PGP_IS_COMMANDS (self), 0U);
	return SEAHORSE_PGP_TYPE;
}


static char* seahorse_pgp_commands_real_get_ui_definition (SeahorsePGPCommands* self) {
	const char* _tmp0;
	g_return_val_if_fail (SEAHORSE_PGP_IS_COMMANDS (self), NULL);
	_tmp0 = NULL;
	return (_tmp0 = SEAHORSE_PGP_COMMANDS_UI_DEF, (_tmp0 == NULL ? NULL : g_strdup (_tmp0)));
}


static void _seahorse_pgp_commands_on_key_sign_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_pgp_commands_on_key_sign (self, _sender);
}


static GtkActionGroup* seahorse_pgp_commands_real_get_command_actions (SeahorsePGPCommands* self) {
	g_return_val_if_fail (SEAHORSE_PGP_IS_COMMANDS (self), NULL);
	if (self->priv->_command_actions == NULL) {
		GtkActionGroup* _tmp0;
		_tmp0 = NULL;
		self->priv->_command_actions = (_tmp0 = gtk_action_group_new ("pgp"), (self->priv->_command_actions == NULL ? NULL : (self->priv->_command_actions = (g_object_unref (self->priv->_command_actions), NULL))), _tmp0);
		gtk_action_group_set_translation_domain (self->priv->_command_actions, GETTEXT_PACKAGE);
		gtk_action_group_add_actions (self->priv->_command_actions, SEAHORSE_PGP_COMMANDS_COMMAND_ENTRIES, G_N_ELEMENTS (SEAHORSE_PGP_COMMANDS_COMMAND_ENTRIES), self);
		g_signal_connect_object (gtk_action_group_get_action (self->priv->_command_actions, "key-sign"), "activate", ((GCallback) (_seahorse_pgp_commands_on_key_sign_gtk_action_activate)), self, 0);
	}
	return self->priv->_command_actions;
}


static void _seahorse_pgp_commands_on_view_selection_changed_seahorse_view_selection_changed (SeahorseView* _sender, gpointer self) {
	seahorse_pgp_commands_on_view_selection_changed (self, _sender);
}


static GObject * seahorse_pgp_commands_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties) {
	GObject * obj;
	SeahorsePGPCommandsClass * klass;
	GObjectClass * parent_class;
	SeahorsePGPCommands * self;
	klass = SEAHORSE_PGP_COMMANDS_CLASS (g_type_class_peek (SEAHORSE_PGP_TYPE_COMMANDS));
	parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
	obj = parent_class->constructor (type, n_construct_properties, construct_properties);
	self = SEAHORSE_PGP_COMMANDS (obj);
	{
		g_assert (seahorse_commands_get_view (SEAHORSE_COMMANDS (self)) != NULL);
		g_signal_connect_object (seahorse_commands_get_view (SEAHORSE_COMMANDS (self)), "selection-changed", ((GCallback) (_seahorse_pgp_commands_on_view_selection_changed_seahorse_view_selection_changed)), self, 0);
	}
	return obj;
}


static void seahorse_pgp_commands_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SeahorsePGPCommands * self;
	self = SEAHORSE_PGP_COMMANDS (object);
	switch (property_id) {
		case SEAHORSE_PGP_COMMANDS_KTYPE:
		g_value_set_uint (value, seahorse_pgp_commands_real_get_ktype (self));
		break;
		case SEAHORSE_PGP_COMMANDS_UI_DEFINITION:
		g_value_set_string (value, seahorse_pgp_commands_real_get_ui_definition (self));
		break;
		case SEAHORSE_PGP_COMMANDS_COMMAND_ACTIONS:
		g_value_set_object (value, seahorse_pgp_commands_real_get_command_actions (self));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_pgp_commands_class_init (SeahorsePGPCommandsClass * klass) {
	seahorse_pgp_commands_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorsePGPCommandsPrivate));
	G_OBJECT_CLASS (klass)->get_property = seahorse_pgp_commands_get_property;
	G_OBJECT_CLASS (klass)->constructor = seahorse_pgp_commands_constructor;
	G_OBJECT_CLASS (klass)->dispose = seahorse_pgp_commands_dispose;
	SEAHORSE_COMMANDS_CLASS (klass)->show_properties = seahorse_pgp_commands_real_show_properties;
	SEAHORSE_COMMANDS_CLASS (klass)->delete_objects = seahorse_pgp_commands_real_delete_objects;
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_PGP_COMMANDS_KTYPE, "ktype");
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_PGP_COMMANDS_UI_DEFINITION, "ui-definition");
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_PGP_COMMANDS_COMMAND_ACTIONS, "command-actions");
	{
		/* Register this class as a commands */
		seahorse_registry_register_type (seahorse_registry_get (), SEAHORSE_PGP_TYPE_COMMANDS, SEAHORSE_PGP_TYPE_STR, "commands", NULL, NULL);
	}
}


static void seahorse_pgp_commands_instance_init (SeahorsePGPCommands * self) {
	self->priv = SEAHORSE_PGP_COMMANDS_GET_PRIVATE (self);
	self->priv->_command_actions = NULL;
}


static void seahorse_pgp_commands_dispose (GObject * obj) {
	SeahorsePGPCommands * self;
	self = SEAHORSE_PGP_COMMANDS (obj);
	(self->priv->_command_actions == NULL ? NULL : (self->priv->_command_actions = (g_object_unref (self->priv->_command_actions), NULL)));
	G_OBJECT_CLASS (seahorse_pgp_commands_parent_class)->dispose (obj);
}


GType seahorse_pgp_commands_get_type (void) {
	static GType seahorse_pgp_commands_type_id = 0;
	if (G_UNLIKELY (seahorse_pgp_commands_type_id == 0)) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorsePGPCommandsClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_pgp_commands_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorsePGPCommands), 0, (GInstanceInitFunc) seahorse_pgp_commands_instance_init };
		seahorse_pgp_commands_type_id = g_type_register_static (SEAHORSE_TYPE_COMMANDS, "SeahorsePGPCommands", &g_define_type_info, 0);
	}
	return seahorse_pgp_commands_type_id;
}




