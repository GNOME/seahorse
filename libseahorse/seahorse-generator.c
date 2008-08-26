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

#include <seahorse-generator.h>




enum  {
	SEAHORSE_GENERATOR_DUMMY_PROPERTY,
	SEAHORSE_GENERATOR_ACTIONS
};
static gpointer seahorse_generator_parent_class = NULL;



GtkActionGroup* seahorse_generator_get_actions (SeahorseGenerator* self) {
	return SEAHORSE_GENERATOR_GET_CLASS (self)->get_actions (self);
}


static void seahorse_generator_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SeahorseGenerator * self;
	self = SEAHORSE_GENERATOR (object);
	switch (property_id) {
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_generator_class_init (SeahorseGeneratorClass * klass) {
	seahorse_generator_parent_class = g_type_class_peek_parent (klass);
	G_OBJECT_CLASS (klass)->get_property = seahorse_generator_get_property;
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_GENERATOR_ACTIONS, g_param_spec_object ("actions", "actions", "actions", GTK_TYPE_ACTION_GROUP, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
}


static void seahorse_generator_instance_init (SeahorseGenerator * self) {
}


GType seahorse_generator_get_type (void) {
	static GType seahorse_generator_type_id = 0;
	if (seahorse_generator_type_id == 0) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorseGeneratorClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_generator_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorseGenerator), 0, (GInstanceInitFunc) seahorse_generator_instance_init };
		seahorse_generator_type_id = g_type_register_static (G_TYPE_OBJECT, "SeahorseGenerator", &g_define_type_info, G_TYPE_FLAG_ABSTRACT);
	}
	return seahorse_generator_type_id;
}




