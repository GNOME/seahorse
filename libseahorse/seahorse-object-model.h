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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>

#include "seahorse-common.h"

#define SEAHORSE_TYPE_OBJECT_MODEL (seahorse_object_model_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseObjectModel, seahorse_object_model,
                      SEAHORSE, OBJECT_MODEL,
                      GtkTreeStore)

SeahorseObjectModel*   seahorse_object_model_new                  (gint n_columns,
                                                                   GType *types);

void                seahorse_object_model_set_column_types        (SeahorseObjectModel *self,
                                                                   gint n_columns,
                                                                   GType *types);

void                seahorse_object_model_set_row_object          (SeahorseObjectModel *self,
                                                                   GtkTreeIter *iter,
                                                                   GObject *object);

GObject*            seahorse_object_model_get_row_key             (SeahorseObjectModel *self,
                                                                   GtkTreeIter *iter);

GList*              seahorse_object_model_get_rows_for_object     (SeahorseObjectModel *self,
                                                                   GObject *object);

void                seahorse_object_model_remove_rows_for_object  (SeahorseObjectModel *self,
                                                                   GObject *object);

void                seahorse_object_model_free_rows               (GList *rows);
