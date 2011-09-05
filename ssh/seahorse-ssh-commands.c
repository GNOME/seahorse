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

#include <glib/gi18n.h>

#include "seahorse-ssh.h"
#include "seahorse-ssh-commands.h"
#include "seahorse-ssh-dialogs.h"
#include "seahorse-ssh-operation.h"

#include "seahorse-object.h"
#include "seahorse-registry.h"
#include "seahorse-util.h"

enum {
	PROP_0
};

struct _SeahorseSshCommandsPrivate {
	GtkActionGroup* command_actions;
};

G_DEFINE_TYPE (SeahorseSshCommands, seahorse_ssh_commands, SEAHORSE_TYPE_COMMANDS);

static const char* UI_DEFINITION = ""\
"<ui>"\
"	<menubar>"\
"		<menu name='Remote' action='remote-menu'>"\
"			<menuitem action='remote-ssh-upload'/>"\
"		</menu>"\
"	</menubar>"\
"	<popup name='KeyPopup'>"\
"		<menuitem action='remote-ssh-upload'/>"\
"	</popup>"\
"</ui>";

static SeahorsePredicate commands_predicate = { 0, };

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static void 
on_ssh_upload (GtkAction* action, SeahorseSshCommands* self) 
{
	SeahorseView *view;
	GList* ssh_keys;

	g_return_if_fail (SEAHORSE_IS_SSH_COMMANDS (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	
	view = seahorse_commands_get_view (SEAHORSE_COMMANDS (self));
	ssh_keys = seahorse_view_get_selected_matching (view, &commands_predicate);
	if (ssh_keys == NULL)
		return;
	
	/* Indicate what we're actually going to operate on */
	seahorse_view_set_selected_objects (view, ssh_keys);
	
	seahorse_ssh_upload_prompt (ssh_keys, seahorse_commands_get_window (SEAHORSE_COMMANDS (self)));
	g_list_free (ssh_keys);
}

static const GtkActionEntry COMMAND_ENTRIES[] = {
	{ "remote-ssh-upload", NULL, N_ ("Configure Key for _Secure Shell..."), "", 
		N_ ("Send public Secure Shell key to another machine, and enable logins using that key."), 
		G_CALLBACK (on_ssh_upload) }
};

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void 
seahorse_ssh_commands_show_properties (SeahorseCommands* base, SeahorseObject* obj) 
{
	g_return_if_fail (SEAHORSE_IS_OBJECT (obj));
	g_return_if_fail (seahorse_object_get_tag (obj) == SEAHORSE_SSH_TYPE);
	g_return_if_fail (G_TYPE_FROM_INSTANCE (G_OBJECT (obj)) == SEAHORSE_SSH_TYPE_KEY);
	
	seahorse_ssh_key_properties_show (SEAHORSE_SSH_KEY (obj), seahorse_commands_get_window (base));
}

static gboolean
seahorse_ssh_commands_delete_objects (SeahorseCommands* base, GList* objects) 
{
	guint num;
	gchar* prompt;
	GList *l;
	GtkWidget *parent;
	GError *error = NULL;

	num = g_list_length (objects);
	if (num == 0) {
		return TRUE;
	} else if (num == 1) {
		prompt = g_strdup_printf (_("Are you sure you want to delete the secure shell key '%s'?"), 
		                          seahorse_object_get_label (objects->data));
	} else {
		prompt = g_strdup_printf (_("Are you sure you want to delete %d secure shell keys?"), num);
	}

	parent = GTK_WIDGET (seahorse_commands_get_window (base));
	if (!seahorse_util_prompt_delete (prompt, NULL)) {
		g_free (prompt);
		return FALSE;
	}

	g_free (prompt);
	for (l = objects; l != NULL; l = g_list_next (l)) {
		if (!seahorse_ssh_op_delete_sync (l->data, &error)) {
			seahorse_util_handle_error (&error, parent, _("Couldn't delete key"));
			return FALSE;
		}
	}

	return TRUE;
}

static GObject* 
seahorse_ssh_commands_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	GObject *obj = G_OBJECT_CLASS (seahorse_ssh_commands_parent_class)->constructor (type, n_props, props);
	SeahorseSshCommands *self = NULL;
	SeahorseCommands *base;
	SeahorseView *view;
	
	if (obj) {
		self = SEAHORSE_SSH_COMMANDS (obj);
		base = SEAHORSE_COMMANDS (obj);
	
		view = seahorse_commands_get_view (SEAHORSE_COMMANDS (self));
		g_return_val_if_fail (view, NULL);
		
		self->pv->command_actions = gtk_action_group_new ("ssh");
		gtk_action_group_set_translation_domain (self->pv->command_actions, GETTEXT_PACKAGE);
		gtk_action_group_add_actions (self->pv->command_actions, COMMAND_ENTRIES, 
		                              G_N_ELEMENTS (COMMAND_ENTRIES), self);
		
		seahorse_view_register_commands (view, &commands_predicate, base);
		seahorse_view_register_ui (view, &commands_predicate, UI_DEFINITION, self->pv->command_actions);
	}
	
	return obj;
}

static void
seahorse_ssh_commands_init (SeahorseSshCommands *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_SSH_COMMANDS, SeahorseSshCommandsPrivate);

}

static void
seahorse_ssh_commands_dispose (GObject *obj)
{
	SeahorseSshCommands *self = SEAHORSE_SSH_COMMANDS (obj);
    
	if (self->pv->command_actions)
		g_object_unref (self->pv->command_actions);
	self->pv->command_actions = NULL;
	
	G_OBJECT_CLASS (seahorse_ssh_commands_parent_class)->dispose (obj);
}

static void
seahorse_ssh_commands_finalize (GObject *obj)
{
	SeahorseSshCommands *self = SEAHORSE_SSH_COMMANDS (obj);

	g_assert (!self->pv->command_actions);
	
	G_OBJECT_CLASS (seahorse_ssh_commands_parent_class)->finalize (obj);
}

static void
seahorse_ssh_commands_set_property (GObject *obj, guint prop_id, const GValue *value, 
                           GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_ssh_commands_get_property (GObject *obj, guint prop_id, GValue *value, 
                           GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_ssh_commands_class_init (SeahorseSshCommandsClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    
	seahorse_ssh_commands_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseSshCommandsPrivate));

	gobject_class->constructor = seahorse_ssh_commands_constructor;
	gobject_class->dispose = seahorse_ssh_commands_dispose;
	gobject_class->finalize = seahorse_ssh_commands_finalize;
	gobject_class->set_property = seahorse_ssh_commands_set_property;
	gobject_class->get_property = seahorse_ssh_commands_get_property;
    
	SEAHORSE_COMMANDS_CLASS (klass)->show_properties = seahorse_ssh_commands_show_properties;
	SEAHORSE_COMMANDS_CLASS (klass)->delete_objects = seahorse_ssh_commands_delete_objects;

	commands_predicate.type = SEAHORSE_TYPE_SSH_KEY;
	commands_predicate.usage = SEAHORSE_USAGE_PRIVATE_KEY;
	
	/* Register this class as a commands */
	seahorse_registry_register_type (seahorse_registry_get (), SEAHORSE_TYPE_SSH_COMMANDS, 
	                                 SEAHORSE_SSH_TYPE_STR, "commands", NULL, NULL);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseSshCommands*
seahorse_ssh_commands_new (void)
{
	return g_object_new (SEAHORSE_TYPE_SSH_COMMANDS, NULL);
}
