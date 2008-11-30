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

#include "seahorse-gkr-commands.h"

#include "seahorse-gkr.h"
#include "seahorse-gkr-item.h"
#include "seahorse-gkr-dialogs.h"

#include "common/seahorse-registry.h"

#include "seahorse-source.h"
#include "seahorse-util.h"

#include <glib/gi18n.h>

enum {
	PROP_0,
	PROP_KTYPE,
	PROP_UI_DEFINITION,
	PROP_COMMAND_ACTIONS
};

struct _SeahorseGkrCommandsPrivate {
	GtkActionGroup* command_actions;
};

G_DEFINE_TYPE (SeahorseGkrCommands, seahorse_gkr_commands, SEAHORSE_TYPE_COMMANDS);

#define SEAHORSE_GKR_COMMANDS_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_TYPE_GKR_COMMANDS, SeahorseGkrCommandsPrivate))

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void 
seahorse_gkr_commands_show_properties (SeahorseCommands* base, SeahorseObject* object) 
{
	GtkWindow *window;

	g_return_if_fail (SEAHORSE_IS_OBJECT (object));
	g_return_if_fail (seahorse_object_get_tag (object) == SEAHORSE_GKR_TYPE);

	window = seahorse_view_get_window (seahorse_commands_get_view (base));
	if (G_OBJECT_TYPE (object) == SEAHORSE_TYPE_GKR_ITEM) 
		seahorse_gkr_item_properties_show (SEAHORSE_GKR_ITEM (object), window); 
	
	else if (G_OBJECT_TYPE (object) == SEAHORSE_TYPE_GKR_KEYRING) 
		seahorse_gkr_keyring_properties_show (SEAHORSE_GKR_KEYRING (object), window);
	
	else
		g_return_if_reached ();
}

static SeahorseOperation* 
seahorse_gkr_commands_delete_objects (SeahorseCommands* base, GList* objects) 
{
	gchar *prompt;
	GtkWidget *parent;
	gboolean ret;
	guint num;

	num = g_list_length (objects);
	if (num == 0)
		return NULL;

	if (num == 1) {
		prompt = g_strdup_printf (_ ("Are you sure you want to delete the password '%s'?"), 
		                          seahorse_object_get_label (SEAHORSE_OBJECT (objects->data)));
	} else {
		prompt = g_strdup_printf (ngettext ("Are you sure you want to delete %d password?", 
		                                    "Are you sure you want to delete %d passwords?", 
		                                    num), num);
	}
	
	parent = GTK_WIDGET (seahorse_view_get_window (seahorse_commands_get_view (base)));
	ret = seahorse_util_prompt_delete (prompt, parent);
	g_free (prompt);
	
	if (!ret)
		return NULL;

	return seahorse_source_delete_objects (objects);
}

static GQuark 
seahorse_gkr_commands_get_ktype (SeahorseCommands* base) 
{
	return SEAHORSE_GKR_TYPE;
}


static const gchar* 
seahorse_gkr_commands_get_ui_definition (SeahorseCommands* base) 
{
	return "";
}


static GtkActionGroup* 
seahorse_gkr_commands_get_command_actions (SeahorseCommands* base) 
{
	SeahorseGkrCommandsPrivate* pv = SEAHORSE_GKR_COMMANDS_GET_PRIVATE(base);
	return pv->command_actions;
}

static GObject* 
seahorse_gkr_commands_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	GObject *obj = G_OBJECT_CLASS (seahorse_gkr_commands_parent_class)->constructor (type, n_props, props);
	SeahorseGkrCommands *self = NULL;
	SeahorseGkrCommandsPrivate *pv;
	
	if (obj) {
		pv = SEAHORSE_GKR_COMMANDS_GET_PRIVATE (obj);
		self = SEAHORSE_GKR_COMMANDS (obj);
		
	}
	
	return obj;
}

static void
seahorse_gkr_commands_init (SeahorseGkrCommands *self)
{
	SeahorseGkrCommandsPrivate *pv = SEAHORSE_GKR_COMMANDS_GET_PRIVATE (self);
	pv->command_actions = gtk_action_group_new ("gkr");
}

static void
seahorse_gkr_commands_dispose (GObject *obj)
{
	SeahorseGkrCommands *self = SEAHORSE_GKR_COMMANDS (obj);
	SeahorseGkrCommandsPrivate *pv = SEAHORSE_GKR_COMMANDS_GET_PRIVATE (self);
    
	if (pv->command_actions)
		g_object_unref (pv->command_actions);
	pv->command_actions = NULL;
	
	G_OBJECT_CLASS (seahorse_gkr_commands_parent_class)->dispose (obj);
}

static void
seahorse_gkr_commands_finalize (GObject *obj)
{
	SeahorseGkrCommands *self = SEAHORSE_GKR_COMMANDS (obj);
	SeahorseGkrCommandsPrivate *pv = SEAHORSE_GKR_COMMANDS_GET_PRIVATE (self);

	g_assert (!pv->command_actions);
	
	G_OBJECT_CLASS (seahorse_gkr_commands_parent_class)->finalize (obj);
}

static void
seahorse_gkr_commands_set_property (GObject *obj, guint prop_id, const GValue *value, 
                           GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_gkr_commands_get_property (GObject *obj, guint prop_id, GValue *value, 
                           GParamSpec *pspec)
{
	SeahorseCommands *base = SEAHORSE_COMMANDS (obj);
	
	switch (prop_id) {
	case PROP_KTYPE:
		g_value_set_uint (value, seahorse_commands_get_ktype (base));
		break;
	case PROP_UI_DEFINITION:
		g_value_set_string (value, seahorse_commands_get_ui_definition (base));
		break;
	case PROP_COMMAND_ACTIONS:
		g_value_set_object (value, seahorse_commands_get_command_actions (base));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_gkr_commands_class_init (SeahorseGkrCommandsClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseCommandsClass *cmd_class = SEAHORSE_COMMANDS_CLASS (klass);
	
	seahorse_gkr_commands_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseGkrCommandsPrivate));

	gobject_class->constructor = seahorse_gkr_commands_constructor;
	gobject_class->dispose = seahorse_gkr_commands_dispose;
	gobject_class->finalize = seahorse_gkr_commands_finalize;
	gobject_class->set_property = seahorse_gkr_commands_set_property;
	gobject_class->get_property = seahorse_gkr_commands_get_property;
    
	cmd_class->show_properties = seahorse_gkr_commands_show_properties;
	cmd_class->delete_objects = seahorse_gkr_commands_delete_objects;
	cmd_class->get_ktype = seahorse_gkr_commands_get_ktype;
	cmd_class->get_ui_definition = seahorse_gkr_commands_get_ui_definition;
	cmd_class->get_command_actions = seahorse_gkr_commands_get_command_actions;
	
	g_object_class_override_property (gobject_class, PROP_KTYPE, "ktype");
	g_object_class_override_property (gobject_class, PROP_UI_DEFINITION, "ui-definition");
	g_object_class_override_property (gobject_class, PROP_COMMAND_ACTIONS, "command-actions");

	/* Register this class as a commands */
	seahorse_registry_register_type (seahorse_registry_get (), SEAHORSE_TYPE_GKR_COMMANDS, 
	                                 SEAHORSE_GKR_TYPE_STR, "commands", NULL, NULL);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseGkrCommands*
seahorse_gkr_commands_new (void)
{
	return g_object_new (SEAHORSE_TYPE_GKR_COMMANDS, NULL);
}
