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

/**
 * SeahorseObjectModel: A GtkTreeModel that can assign certain rows as 
 *   'key rows' which are updated when a key is updated. 
 *
 * Signals:
 *   update-row: A request to update a row 
 */
 
#ifndef __SEAHORSE_OBJECT_MODEL_H__
#define __SEAHORSE_OBJECT_MODEL_H__

#include <gtk/gtk.h>

#include "seahorse-object.h"

#define SEAHORSE_TYPE_OBJECT_MODEL               (seahorse_object_model_get_type ())
#define SEAHORSE_OBJECT_MODEL(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_OBJECT_MODEL, SeahorseObjectModel))
#define SEAHORSE_OBJECT_MODEL_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_OBJECT_MODEL, SeahorseObjectModelClass))
#define SEAHORSE_IS_OBJECT_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_OBJECT_MODEL))
#define SEAHORSE_IS_OBJECT_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_OBJECT_MODEL))
#define SEAHORSE_OBJECT_MODEL_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_OBJECT_MODEL, SeahorseObjectModelClass))

typedef struct _SeahorseObjectModel SeahorseObjectModel;
typedef struct _SeahorseObjectModelClass SeahorseObjectModelClass;
    
struct _SeahorseObjectModel {
    GtkTreeStore parent;
};

struct _SeahorseObjectModelClass {
    GtkTreeStoreClass parent_class;
    
    /* signals --------------------------------------------------------- */
    
    /* A key was added to this view */
    void (*update_row)   (SeahorseObjectModel *self, SeahorseObject *object, GtkTreeIter *iter);
};

GType               seahorse_object_model_get_type                (void);

SeahorseObjectModel*   seahorse_object_model_new                  (gint n_columns,
                                                                   GType *types);

void                seahorse_object_model_set_column_types        (SeahorseObjectModel *self, 
                                                                   gint n_columns,
                                                                   GType *types);

void                seahorse_object_model_set_row_object          (SeahorseObjectModel *self,
                                                                   GtkTreeIter *iter,
                                                                   SeahorseObject *object);

SeahorseObject*     seahorse_object_model_get_row_key             (SeahorseObjectModel *self,
                                                                   GtkTreeIter *iter);

GSList*             seahorse_object_model_get_rows_for_object     (SeahorseObjectModel *self,
                                                                   SeahorseObject *object);

void                seahorse_object_model_remove_rows_for_object  (SeahorseObjectModel *self,
                                                                   SeahorseObject *object);

void                seahorse_object_model_free_rows               (GSList *rows);

#endif /* __SEAHORSE_OBJECT_MODEL_H__ */
