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

#include "seahorse-actions.h"

struct _SeahorseActionsPrivate {
	const gchar *definition;
};

G_DEFINE_TYPE (SeahorseActions, seahorse_actions, GTK_TYPE_ACTION_GROUP);

static void
seahorse_actions_real_update (SeahorseActions *self,
                              GList *selected)
{

}

static void
seahorse_actions_init (SeahorseActions *self)
{
	self->pv = (G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_ACTIONS,
	                                         SeahorseActionsPrivate));
}

static void
seahorse_actions_class_init (SeahorseActionsClass *klass)
{
	g_type_class_add_private (klass, sizeof (SeahorseActionsPrivate));

	klass->update = seahorse_actions_real_update;
}

const gchar *
seahorse_actions_get_definition (SeahorseActions* self)
{
	g_return_val_if_fail (SEAHORSE_IS_ACTIONS (self), NULL);
	return self->pv->definition;
}

void
seahorse_actions_update (SeahorseActions *self,
                         GList *selected)
{
	g_return_if_fail (SEAHORSE_IS_ACTIONS (self));
	g_return_if_fail (SEAHORSE_ACTIONS_GET_CLASS (self)->update);
	SEAHORSE_ACTIONS_GET_CLASS (self)->update (self, selected);
}

void
seahorse_actions_register_definition (SeahorseActions *self,
                                      const gchar *definition)
{
	g_return_if_fail (SEAHORSE_IS_ACTIONS (self));
	g_return_if_fail (self->pv->definition == NULL);
	self->pv->definition = definition;
}
