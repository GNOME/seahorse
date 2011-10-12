/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __SEAHORSE_CONTEXT_H__
#define __SEAHORSE_CONTEXT_H__

#include <gtk/gtk.h>

#include "seahorse-source.h"

#define SEAHORSE_TYPE_CONTEXT			(seahorse_context_get_type ())
#define SEAHORSE_CONTEXT(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_CONTEXT, SeahorseContext))
#define SEAHORSE_CONTEXT_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_CONTEXT, SeahorseContextClass))
#define SEAHORSE_IS_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_CONTEXT))
#define SEAHORSE_IS_CONTEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_CONTEXT))
#define SEAHORSE_CONTEXT_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_CONTEXT, SeahorseContextClass))

typedef struct _SeahorseContext SeahorseContext;
typedef struct _SeahorseContextClass SeahorseContextClass;
typedef struct _SeahorseContextPrivate SeahorseContextPrivate;

/**
 * SeahorseContext:
 * @parent: The parent #GObject
 * @is_daemon: a #gboolean indicating whether the context is being used in a
 *             program that is daemonized
 *
 * This is where all the action in a Seahorse process comes together.
 *
 * - Usually there's only one #SeahorseContext per process created by passing
 *   %SEAHORSE_CONTEXT_APP to seahorse_context_new(), and accessed via
 *   the %SCTX_APP macro.
 *
 * Signals:
 *   destroy: The context was destroyed.
 */

struct _SeahorseContext {
	GObject parent;

	/*< private >*/
	SeahorseContextPrivate  *pv;
};

struct _SeahorseContextClass {
	GObjectClass parent_class;

	void     (*destroy)            (SeahorseContext *self);
};

#define             SCTX_APP()                               (seahorse_context_instance ())

GType               seahorse_context_get_type                (void);

SeahorseContext*    seahorse_context_instance                (void);

void                seahorse_context_create                  (void);

void                seahorse_context_destroy                 (SeahorseContext *self);

GSettings *         seahorse_context_settings                (SeahorseContext *self);

GSettings *         seahorse_context_pgp_settings            (SeahorseContext *self);

#endif /* __SEAHORSE_CONTEXT_H__ */
