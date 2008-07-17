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

#include "seahorse-pgp-generator.h"
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <string.h>
#include <seahorse-context.h>
#include <seahorse-types.h>
#include <seahorse-source.h>
#include <seahorse-pgp-dialogs.h>
#include <seahorse-pgp-source.h>
#include <config.h>
#include <common/seahorse-registry.h>
#include "seahorse-pgp.h"




struct _SeahorsePGPGeneratorPrivate {
	GtkActionGroup* _generate_actions;
};

#define SEAHORSE_PGP_GENERATOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_PGP_TYPE_GENERATOR, SeahorsePGPGeneratorPrivate))
enum  {
	SEAHORSE_PGP_GENERATOR_DUMMY_PROPERTY,
	SEAHORSE_PGP_GENERATOR_ACTIONS
};
static void seahorse_pgp_generator_on_pgp_generate (SeahorsePGPGenerator* self, GtkAction* action);
static void _seahorse_pgp_generator_on_pgp_generate_gtk_action_activate (GtkAction* _sender, gpointer self);
static gpointer seahorse_pgp_generator_parent_class = NULL;
static void seahorse_pgp_generator_dispose (GObject * obj);

static const GtkActionEntry SEAHORSE_PGP_GENERATOR_GENERATE_ENTRIES[] = {{"pgp-generate-key", SEAHORSE_PGP_STOCK_ICON, N_ ("PGP Key"), "", N_ ("Used to encrypt email and files"), ((GCallback) (NULL))}};


static void seahorse_pgp_generator_on_pgp_generate (SeahorsePGPGenerator* self, GtkAction* action) {
	SeahorseSource* _tmp0;
	SeahorseSource* sksrc;
	g_return_if_fail (SEAHORSE_PGP_IS_GENERATOR (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	_tmp0 = NULL;
	sksrc = (_tmp0 = seahorse_context_find_source (seahorse_context_for_app (), SEAHORSE_PGP_TYPE, SEAHORSE_LOCATION_LOCAL), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	g_return_if_fail (sksrc != NULL);
	seahorse_pgp_generate_show (SEAHORSE_PGP_SOURCE (sksrc), NULL);
	(sksrc == NULL ? NULL : (sksrc = (g_object_unref (sksrc), NULL)));
}


SeahorsePGPGenerator* seahorse_pgp_generator_new (void) {
	SeahorsePGPGenerator * self;
	self = g_object_newv (SEAHORSE_PGP_TYPE_GENERATOR, 0, NULL);
	return self;
}


static void _seahorse_pgp_generator_on_pgp_generate_gtk_action_activate (GtkAction* _sender, gpointer self) {
	seahorse_pgp_generator_on_pgp_generate (self, _sender);
}


static GtkActionGroup* seahorse_pgp_generator_real_get_actions (SeahorsePGPGenerator* self) {
	g_return_val_if_fail (SEAHORSE_PGP_IS_GENERATOR (self), NULL);
	if (self->priv->_generate_actions == NULL) {
		GtkActionGroup* _tmp0;
		_tmp0 = NULL;
		self->priv->_generate_actions = (_tmp0 = gtk_action_group_new ("pgp-generate"), (self->priv->_generate_actions == NULL ? NULL : (self->priv->_generate_actions = (g_object_unref (self->priv->_generate_actions), NULL))), _tmp0);
		gtk_action_group_set_translation_domain (self->priv->_generate_actions, GETTEXT_PACKAGE);
		gtk_action_group_add_actions (self->priv->_generate_actions, SEAHORSE_PGP_GENERATOR_GENERATE_ENTRIES, G_N_ELEMENTS (SEAHORSE_PGP_GENERATOR_GENERATE_ENTRIES), self);
		g_signal_connect_object (gtk_action_group_get_action (self->priv->_generate_actions, "pgp-generate-key"), "activate", ((GCallback) (_seahorse_pgp_generator_on_pgp_generate_gtk_action_activate)), self, 0);
	}
	return self->priv->_generate_actions;
}


static void seahorse_pgp_generator_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SeahorsePGPGenerator * self;
	self = SEAHORSE_PGP_GENERATOR (object);
	switch (property_id) {
		case SEAHORSE_PGP_GENERATOR_ACTIONS:
		g_value_set_object (value, seahorse_pgp_generator_real_get_actions (self));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_pgp_generator_class_init (SeahorsePGPGeneratorClass * klass) {
	seahorse_pgp_generator_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorsePGPGeneratorPrivate));
	G_OBJECT_CLASS (klass)->get_property = seahorse_pgp_generator_get_property;
	G_OBJECT_CLASS (klass)->dispose = seahorse_pgp_generator_dispose;
	g_object_class_override_property (G_OBJECT_CLASS (klass), SEAHORSE_PGP_GENERATOR_ACTIONS, "actions");
	{
		/* Register this class as a commands */
		seahorse_registry_register_type (seahorse_registry_get (), SEAHORSE_PGP_TYPE_GENERATOR, SEAHORSE_PGP_TYPE_STR, "generator", NULL, NULL);
	}
}


static void seahorse_pgp_generator_instance_init (SeahorsePGPGenerator * self) {
	self->priv = SEAHORSE_PGP_GENERATOR_GET_PRIVATE (self);
	self->priv->_generate_actions = NULL;
}


static void seahorse_pgp_generator_dispose (GObject * obj) {
	SeahorsePGPGenerator * self;
	self = SEAHORSE_PGP_GENERATOR (obj);
	(self->priv->_generate_actions == NULL ? NULL : (self->priv->_generate_actions = (g_object_unref (self->priv->_generate_actions), NULL)));
	G_OBJECT_CLASS (seahorse_pgp_generator_parent_class)->dispose (obj);
}


GType seahorse_pgp_generator_get_type (void) {
	static GType seahorse_pgp_generator_type_id = 0;
	if (G_UNLIKELY (seahorse_pgp_generator_type_id == 0)) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorsePGPGeneratorClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_pgp_generator_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorsePGPGenerator), 0, (GInstanceInitFunc) seahorse_pgp_generator_instance_init };
		seahorse_pgp_generator_type_id = g_type_register_static (SEAHORSE_TYPE_GENERATOR, "SeahorsePGPGenerator", &g_define_type_info, 0);
	}
	return seahorse_pgp_generator_type_id;
}




