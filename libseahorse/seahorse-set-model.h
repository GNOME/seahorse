/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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
 
#ifndef __SEAHORSE_SET_MODEL_H__
#define __SEAHORSE_SET_MODEL_H__

#include <gtk/gtk.h>
#include "seahorse-object.h"
#include "seahorse-set.h"

typedef struct _SeahorseSetModelColumn {
	const gchar *property;
	GType type;
	gpointer data;
} SeahorseSetModelColumn;

#define SEAHORSE_TYPE_SET_MODEL               (seahorse_set_model_get_type ())
#define SEAHORSE_SET_MODEL(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SET_MODEL, SeahorseSetModel))
#define SEAHORSE_SET_MODEL_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SET_MODEL, SeahorseSetModelClass))
#define SEAHORSE_IS_SET_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SET_MODEL))
#define SEAHORSE_IS_SET_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SET_MODEL))
#define SEAHORSE_SET_MODEL_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SET_MODEL, SeahorseSetModelClass))

typedef struct _SeahorseSetModel SeahorseSetModel;
typedef struct _SeahorseSetModelClass SeahorseSetModelClass;

/**
 * SeahorseSetModel:
 * @parent: #GObject #SeahorseSetModel inherits from
 * @set: #SeahorseSet that belongs to the model
 *
 * A GtkTreeModel which represents all objects in a SeahorseSet
 */

struct _SeahorseSetModel {
	GObject parent;
	SeahorseSet *set;
};

struct _SeahorseSetModelClass {
	GObjectClass parent_class;
};

GType               seahorse_set_model_get_type                (void);

SeahorseSetModel*   seahorse_set_model_new                     (SeahorseSet *set,
                                                                ...) G_GNUC_NULL_TERMINATED;

SeahorseSetModel*   seahorse_set_model_new_full                (SeahorseSet *set,
                                                                const SeahorseSetModelColumn *columns,
                                                                guint n_columns);

gint                seahorse_set_model_set_columns             (SeahorseSetModel *smodel, 
                                                                const SeahorseSetModelColumn *columns,
                                                                guint n_columns);

SeahorseObject*     seahorse_set_model_object_for_iter         (SeahorseSetModel *smodel,
                                                                const GtkTreeIter *iter);

gboolean            seahorse_set_model_iter_for_object         (SeahorseSetModel *smodel,
                                                                SeahorseObject *object,
                                                                GtkTreeIter *iter);

#endif /* __SEAHORSE_SET_H__ */
