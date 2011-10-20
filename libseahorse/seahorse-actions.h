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

#ifndef __SEAHORSE_ACTIONS_H__
#define __SEAHORSE_ACTIONS_H__

#include <gtk/gtk.h>

#include "seahorse-object.h"
#include "seahorse-viewer.h"

#define SEAHORSE_TYPE_ACTIONS                  (seahorse_actions_get_type ())
#define SEAHORSE_ACTIONS(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_ACTIONS, SeahorseActions))
#define SEAHORSE_ACTIONS_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_ACTIONS, SeahorseActionsClass))
#define SEAHORSE_IS_ACTIONS(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_ACTIONS))
#define SEAHORSE_IS_ACTIONS_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_ACTIONS))
#define SEAHORSE_ACTIONS_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_ACTIONS, SeahorseActionsClass))

typedef struct _SeahorseActions SeahorseActions;
typedef struct _SeahorseActionsClass SeahorseActionsClass;
typedef struct _SeahorseActionsPrivate SeahorseActionsPrivate;

struct _SeahorseActions {
	GtkActionGroup parent_instance;
	SeahorseActionsPrivate *pv;
};

struct _SeahorseActionsClass {
	GtkActionGroupClass parent_class;

	void        (*update)            (SeahorseActions *self,
	                                  GList *selected);
};

GType                 seahorse_actions_get_type                 (void);

GtkActionGroup *      seahorse_actions_new                      (const gchar *name);

const gchar *         seahorse_actions_get_definition           (SeahorseActions *self);

void                  seahorse_actions_register_definition      (SeahorseActions *self,
                                                                 const gchar *definition);

void                  seahorse_actions_update                   (SeahorseActions *actions,
                                                                 GList *selected);

#endif
