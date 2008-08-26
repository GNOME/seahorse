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

#include "seahorse-ssh-generator.h"
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <string.h>
#include <seahorse-context.h>
#include <seahorse-types.h>
#include <seahorse-source.h>
#include <seahorse-ssh-dialogs.h>
#include <seahorse-ssh-source.h>
#include <config.h>
#include <common/seahorse-registry.h>
#include "seahorse-ssh.h"




struct _SeahorseSSHGeneratorPrivate {
	GtkActionGroup* _generate_actions;
};

#define SEAHORSE_SSH_GENERATOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_SSH_TYPE_GENERATOR, SeahorseSSHGeneratorPrivate))
enum  {
	SEAHORSE_SSH_GENERATOR_DUMMY_PROPERTY,
	SEAHORSE_SSH_GENERATOR_ACTIONS
};
static void seahorse_ssh_generator_on_ssh_generate (SeahorseSSHGenerator* self, GtkAction* action);
static void _seahorse_ssh_generator_on_ssh_generate_gtk_action_activate (GtkAction* _sender, gpointer self);
static gpointer seahorse_ssh_generator_parent_class = NULL;
static void seahorse_ssh_generator_finalize (GObject * obj);

static const GtkActionEntry SEAHORSE_SSH_GENERATOR_GENERATE_ENTRIES[] = {{"ssh-generate-key", SEAHORSE_SSH_STOCK_ICON, N_ ("Secure Shell Key"), NULL, N_ ("Used to access other computers (eg: via a terminal)"), ((GCallback) (NULL))}};


static void seahorse_ssh_generator_on_ssh_generate (SeahorseSSHGenerator* self, GtkAction* action) {
	SeahorseSource* _tmp0;
	SeahorseSource* sksrc;
	g_return_if_fail (SEAHORSE_SSH_IS_GENERATOR (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	_tmp0 = NULL;
	sksrc = (_tmp0 = seahorse_context_find_source (seahorse_context_for_app (), SEAHORSE_SSH_TYPE, SEAHORSE_LOCATION_LOCAL), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	g_return_if_fail (sksrc != NULL);
	seahorse_ssh_generate_show (SEAHORSE_SSH_SOURCE (sksrc), NULL);
	(sksrc == NULL ? NULL : (sksrc = (g_object_unref (sksrc), NULL)));
}


SeahorseSSHGenerator* seahorse_ssh_generator_new (void) {
	SeahorseSSHGenerator * self;
	self = g_object_newv (SEAHORSE_SSH_TYPE_GENERATOR, 0, NULL);
	return self;
}


static void _seahorse_ssh_generator_on_ssh_generate_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_ssh_generator_on_ssh_generate (self, _sender);
}


static GtkActionGroup* seahorse_ssh_generator_real_get_actions (SeahorseGenerator* base) {
	SeahorseSSHGenerator* self;
	self = SEAHORSE_SSH_GENERATOR (base);
	if (self->priv->_generate_actions == NULL) {
		GtkActionGroup* _tmp0;
		_tmp0 = NULL;
		self->priv->_generate_actions = (_tmp0 = gtk_action_group_new ("ssh-generate"), (self->priv->_generate_actions == NULL ? NULL : (self->priv->_generate_actions = (g_object_unref (self->priv->_generate_actions), NULL))), _tmp0);
		gtk_action_group_set_translation_domain (self->priv->_generate_actions, GETTEXT_PACKAGE);
		gtk_action_group_add_actions (self->priv->_generate_actions, SEAHORSE_SSH_GENERATOR_GENERATE_ENTRIES, G_N_ELEMENTS (SEAHORSE_SSH_GENERATOR_GENERATE_ENTRIES), self);
		g_signal_connect_object (gtk_action_group_get_action (self->priv->_generate_actions, "ssh-generate-key"), "activate", ((GCallback) (_seahorse_ssh_generator_on_ssh_generate_gtk_action_activate)), self, 0);
	}
	return self->priv->_generate_actions;
}


static void seahorse_ssh_generator_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SeahorseSSHGenerator * self;
	self = SEAHORSE_SSH_GENERATOR (object);
	switch (property_id) {
		case SEAHORSE_SSH_GENERATOR_ACTIONS:
		g_value_set_object (value, seahorse_generator_get_actions (SEAHORSE_GENERATOR (self)));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_ssh_generator_class_init (SeahorseSSHGeneratorClass * klass) {
	seahorse_ssh_generator_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseSSHGeneratorPrivate));
	G_OBJECT_CLASS (klass)->get_property = seahorse_ssh_generator_get_property;
	G_OBJECT_CLASS (klass)->finalize = seahorse_ssh_generator_finalize;
	SEAHORSE_GENERATOR_CLASS (klass)->get_actions = seahorse_ssh_generator_real_get_actions;
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_SSH_GENERATOR_ACTIONS, "actions");
	{
		/* Register this class as a generator */
		seahorse_registry_register_type (seahorse_registry_get (), SEAHORSE_SSH_TYPE_GENERATOR, SEAHORSE_SSH_TYPE_STR, "generator", NULL, NULL);
	}
}


static void seahorse_ssh_generator_instance_init (SeahorseSSHGenerator * self) {
	self->priv = SEAHORSE_SSH_GENERATOR_GET_PRIVATE (self);
	self->priv->_generate_actions = NULL;
}


static void seahorse_ssh_generator_finalize (GObject * obj) {
	SeahorseSSHGenerator * self;
	self = SEAHORSE_SSH_GENERATOR (obj);
	(self->priv->_generate_actions == NULL ? NULL : (self->priv->_generate_actions = (g_object_unref (self->priv->_generate_actions), NULL)));
	G_OBJECT_CLASS (seahorse_ssh_generator_parent_class)->finalize (obj);
}


GType seahorse_ssh_generator_get_type (void) {
	static GType seahorse_ssh_generator_type_id = 0;
	if (seahorse_ssh_generator_type_id == 0) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorseSSHGeneratorClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_ssh_generator_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorseSSHGenerator), 0, (GInstanceInitFunc) seahorse_ssh_generator_instance_init };
		seahorse_ssh_generator_type_id = g_type_register_static (SEAHORSE_TYPE_GENERATOR, "SeahorseSSHGenerator", &g_define_type_info, 0);
	}
	return seahorse_ssh_generator_type_id;
}




