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
	SeahorseCatalog *catalog;
	guint catalog_sig;
};

G_DEFINE_TYPE (SeahorseActions, seahorse_actions, GTK_TYPE_ACTION_GROUP);

static void
seahorse_actions_real_update (SeahorseActions *self,
                              SeahorseCatalog *catalog)
{
	/* Nothing to do */
}

static void
seahorse_actions_init (SeahorseActions *self)
{
	self->pv = (G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_ACTIONS,
	                                         SeahorseActionsPrivate));
}

static void
seahorse_actions_finalize (GObject *obj)
{
	SeahorseActions *actions = SEAHORSE_ACTIONS (obj);

	seahorse_actions_set_catalog (actions, NULL);

	G_OBJECT_CLASS (seahorse_actions_parent_class)->finalize (obj);
}

static void
seahorse_actions_class_init (SeahorseActionsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = seahorse_actions_finalize;

	klass->update = seahorse_actions_real_update;

	g_type_class_add_private (klass, sizeof (SeahorseActionsPrivate));
}

GtkActionGroup *
seahorse_actions_new (const gchar *name)
{
	g_return_val_if_fail (name != NULL, NULL);
	return g_object_new (SEAHORSE_TYPE_ACTIONS,
	                     "name", name,
	                     NULL);
}

SeahorseCatalog *
seahorse_actions_get_catalog (SeahorseActions *self)
{
	g_return_val_if_fail (SEAHORSE_IS_ACTIONS (self), NULL);
	return self->pv->catalog;
}

static void
on_catalog_gone (gpointer data,
                 GObject *where_the_object_was)
{
	SeahorseActions *self = SEAHORSE_ACTIONS (data);

	self->pv->catalog = NULL;
	self->pv->catalog_sig = 0;
}

static void
on_catalog_selection (SeahorseCatalog *catalog,
                      gpointer user_data)
{
	SeahorseActions *self = SEAHORSE_ACTIONS (user_data);
	seahorse_actions_update (GTK_ACTION_GROUP (self), catalog);
}

void
seahorse_actions_set_catalog (SeahorseActions *self,
                              SeahorseCatalog *catalog)
{
	g_return_if_fail (SEAHORSE_IS_ACTIONS (self));
	g_return_if_fail (catalog == NULL || SEAHORSE_IS_CATALOG (catalog));

	if (self->pv->catalog) {
		g_signal_handler_disconnect (self->pv->catalog,
		                             self->pv->catalog_sig);
		g_object_weak_unref (G_OBJECT (self->pv->catalog),
		                     on_catalog_gone, self);
	}

	self->pv->catalog = catalog;
	if (self->pv->catalog) {
		self->pv->catalog_sig = g_signal_connect (self->pv->catalog,
		                                          "selection-changed",
		                                          G_CALLBACK (on_catalog_selection),
		                                          self);
		g_object_weak_ref (G_OBJECT (self->pv->catalog),
		                   on_catalog_gone, self);
	}
}

const gchar *
seahorse_actions_get_definition (SeahorseActions* self)
{
	g_return_val_if_fail (SEAHORSE_IS_ACTIONS (self), NULL);
	return self->pv->definition;
}

void
seahorse_actions_register_definition (SeahorseActions *self,
                                      const gchar *definition)
{
	g_return_if_fail (SEAHORSE_IS_ACTIONS (self));
	g_return_if_fail (self->pv->definition == NULL);
	self->pv->definition = definition;
}

void
seahorse_actions_update (GtkActionGroup *actions,
                         SeahorseCatalog *catalog)
{
	SeahorseActionsClass *klass;

	g_return_if_fail (GTK_IS_ACTION_GROUP (actions));

	if (!SEAHORSE_IS_ACTIONS (actions))
		return;

	klass = SEAHORSE_ACTIONS_GET_CLASS (actions);
	g_assert (klass->update != NULL);
	(klass->update) (SEAHORSE_ACTIONS (actions), catalog);
}
