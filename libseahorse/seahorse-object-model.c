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
 
#include "config.h"

#include <string.h>

#include "seahorse-bind.h"
#include "seahorse-object-model.h"
#include "seahorse-marshal.h"

enum {
    PROP_0,
    PROP_DATA_COLUMN,
};

enum {
    UPDATE_ROW,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct _SeahorseObjectModelPrivate {
    GHashTable *rows;
    gint data_column;
} SeahorseObjectModelPrivate;

G_DEFINE_TYPE (SeahorseObjectModel, seahorse_object_model, GTK_TYPE_TREE_STORE);

#define SEAHORSE_OBJECT_MODEL_GET_PRIVATE(obj)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SEAHORSE_TYPE_OBJECT_MODEL, SeahorseObjectModelPrivate))

/* Internal data stored at 0 in the tree store in order to keep track
 * of the location, key-store and key.
 */
typedef struct {
	SeahorseObjectModel *self;
	GPtrArray           *refs;     /* GtkTreeRowReference pointers */
	GObject             *object;     /* The key we're dealing with */
	gpointer            binding;
} SeahorseObjectRow;

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static void
key_notify (GObject *object,
            SeahorseObjectModel *self)
{
    SeahorseObjectModelPrivate *pv = SEAHORSE_OBJECT_MODEL_GET_PRIVATE (self);
    SeahorseObjectRow *skrow;
    GtkTreeIter iter;
    GtkTreePath *path;
    guint i;

    skrow = g_hash_table_lookup (pv->rows, object);
    if (!skrow)
        return;
    
    for (i = 0; i < skrow->refs->len; i++) {
        
        path = gtk_tree_row_reference_get_path (g_ptr_array_index (skrow->refs, i));
        if (path) {
            gtk_tree_model_get_iter (GTK_TREE_MODEL (self), &iter, path);
            g_signal_emit (self, signals[UPDATE_ROW], 0, object, &iter);
            gtk_tree_path_free (path);
        }
    }
}

static void
key_destroyed (gpointer data, GObject *was)
{
	SeahorseObjectModelPrivate *pv = SEAHORSE_OBJECT_MODEL_GET_PRIVATE (data);
	SeahorseObjectRow *skrow = g_hash_table_lookup (pv->rows, was);
	if (skrow) {
		skrow->object = NULL;
		skrow->binding = NULL;
		g_hash_table_remove (pv->rows, was);
	}
}


static gboolean
remove_each (GObject *object,
             gchar *path,
             SeahorseObjectModel *self)
{
    return TRUE;
}

static SeahorseObjectRow *
key_row_new (SeahorseObjectModel *self,
             GObject *object)
{
    SeahorseObjectRow *skrow;
    
    g_assert (SEAHORSE_IS_OBJECT_MODEL (self));
    g_assert (G_IS_OBJECT (object));

    skrow = g_new0 (SeahorseObjectRow, 1);
    skrow->refs = g_ptr_array_new ();
    skrow->self = self;
    skrow->object = object;
    skrow->binding = seahorse_bind_objects (NULL, object, (SeahorseTransfer)key_notify, self);
    
    g_object_weak_ref (G_OBJECT (object), key_destroyed, self);
    
    return skrow;
}

static void
key_row_free (SeahorseObjectRow *skrow)
{
    SeahorseObjectModel *self = skrow->self;
    SeahorseObjectModelPrivate *pv = SEAHORSE_OBJECT_MODEL_GET_PRIVATE (self);
    GtkTreeRowReference *ref;
    GtkTreePath *path;
    GtkTreeIter iter;
    guint i;
    
    g_return_if_fail (pv->data_column != -1);
    
    for (i = 0; i < skrow->refs->len; i++) {
        
        ref = (GtkTreeRowReference*)g_ptr_array_index (skrow->refs, i);
        if (ref) {
            path = gtk_tree_row_reference_get_path (ref);
            if (path) {
                gtk_tree_model_get_iter (GTK_TREE_MODEL (self), &iter, path);
                gtk_tree_store_set (GTK_TREE_STORE (self), &iter, 
                                    pv->data_column, NULL, -1);
                gtk_tree_path_free (path);
            }
            gtk_tree_row_reference_free (ref);
        }
        
    }
    
    if (skrow->binding)
	    seahorse_bind_disconnect (skrow->binding);
    if (skrow->object)
	    g_object_weak_unref (G_OBJECT (skrow->object), key_destroyed, skrow->self);

    g_ptr_array_free (skrow->refs, TRUE);
    g_free (skrow);
}

static void
row_inserted (SeahorseObjectModel *self, GtkTreePath *path, GtkTreeIter *iter, 
              gpointer user_data)
{
    SeahorseObjectModelPrivate *pv = SEAHORSE_OBJECT_MODEL_GET_PRIVATE (self);
    g_return_if_fail (pv->data_column != -1);
    /* XXX: The following line causes problems with GtkTreeModelFilter */
    /* gtk_tree_store_set (GTK_TREE_STORE (self), iter, pv->data_column, NULL, -1); */
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_object_model_init (SeahorseObjectModel *self)
{
    SeahorseObjectModelPrivate *pv = SEAHORSE_OBJECT_MODEL_GET_PRIVATE (self);
    pv->rows = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                      NULL, (GDestroyNotify)key_row_free);
    pv->data_column = -1;
    g_signal_connect (self, "row-inserted", G_CALLBACK (row_inserted), NULL);
}

static void
seahorse_object_model_set_property (GObject *gobject, guint prop_id,
                                 const GValue *value, GParamSpec *pspec)
{
    SeahorseObjectModel *self = SEAHORSE_OBJECT_MODEL (gobject);
    SeahorseObjectModelPrivate *pv = SEAHORSE_OBJECT_MODEL_GET_PRIVATE (self);

    switch (prop_id) {
    case PROP_DATA_COLUMN:
        g_assert (pv->data_column == -1);
        pv->data_column = g_value_get_uint (value);
        break;
    }
}

static void
seahorse_object_model_get_property (GObject *gobject, guint prop_id,
                                 GValue *value, GParamSpec *pspec)
{
    SeahorseObjectModel *self = SEAHORSE_OBJECT_MODEL (gobject);
    SeahorseObjectModelPrivate *pv = SEAHORSE_OBJECT_MODEL_GET_PRIVATE (self);

    switch (prop_id) {
    case PROP_DATA_COLUMN:
        g_value_set_uint (value, pv->data_column);
        break;
    }
}

static void
seahorse_object_model_dispose (GObject *gobject)
{
    SeahorseObjectModel *self = SEAHORSE_OBJECT_MODEL (gobject);
    SeahorseObjectModelPrivate *pv = SEAHORSE_OBJECT_MODEL_GET_PRIVATE (self);
    
    /* Release all our pointers and stuff */
    g_hash_table_foreach_remove (pv->rows, (GHRFunc)remove_each, self);
    G_OBJECT_CLASS (seahorse_object_model_parent_class)->dispose (gobject);
}

static void
seahorse_object_model_finalize (GObject *gobject)
{
    SeahorseObjectModel *self = SEAHORSE_OBJECT_MODEL (gobject);
    SeahorseObjectModelPrivate *pv = SEAHORSE_OBJECT_MODEL_GET_PRIVATE (self);

    if (pv->rows)
        g_hash_table_destroy (pv->rows);
    pv->rows = NULL;
    
    G_OBJECT_CLASS (seahorse_object_model_parent_class)->finalize (gobject);
}

static void
seahorse_object_model_class_init (SeahorseObjectModelClass *klass)
{
    GObjectClass *gobject_class;
    
    seahorse_object_model_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->dispose = seahorse_object_model_dispose;
    gobject_class->finalize = seahorse_object_model_finalize;
    gobject_class->set_property = seahorse_object_model_set_property;
    gobject_class->get_property = seahorse_object_model_get_property;
    
    g_object_class_install_property (gobject_class, PROP_DATA_COLUMN,
        g_param_spec_uint ("data-column", "Column data is stored", "Column where internal data is stored",
                           0, ~0, 0, G_PARAM_READWRITE | G_PARAM_READWRITE));

    signals[UPDATE_ROW] = g_signal_new ("update-row", SEAHORSE_TYPE_OBJECT_MODEL, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseObjectModelClass, update_row),
                NULL, NULL, seahorse_marshal_VOID__OBJECT_POINTER, G_TYPE_NONE, 2, SEAHORSE_TYPE_OBJECT, G_TYPE_POINTER);
    
    g_type_class_add_private (klass, sizeof (SeahorseObjectModelPrivate));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */


SeahorseObjectModel* 
seahorse_object_model_new (gint n_columns, GType *types)
{
    SeahorseObjectModel *model;
    
    model = g_object_new (SEAHORSE_TYPE_OBJECT_MODEL, NULL);
    seahorse_object_model_set_column_types (model, n_columns, types);
    
    return model;
}

void
seahorse_object_model_set_column_types (SeahorseObjectModel *self, gint n_columns,
                                     GType *types)
{
    SeahorseObjectModelPrivate *pv = SEAHORSE_OBJECT_MODEL_GET_PRIVATE (self);
    GType *itypes;
    
    g_return_if_fail (SEAHORSE_IS_OBJECT_MODEL (self));

    itypes = g_new0(GType, n_columns + 1);
    memcpy (itypes, types, n_columns * sizeof (GType));

    itypes[n_columns] = G_TYPE_POINTER;
    pv->data_column = n_columns;
    gtk_tree_store_set_column_types (GTK_TREE_STORE (self), n_columns + 1, itypes);
    
    g_free (itypes);
}

void
seahorse_object_model_set_row_object (SeahorseObjectModel *self,
                                      GtkTreeIter *iter,
                                      GObject *object)
{
    SeahorseObjectModelPrivate *pv = SEAHORSE_OBJECT_MODEL_GET_PRIVATE (self);
    SeahorseObjectRow *skrow;
    GtkTreePath *path;
    GtkTreePath *ipath;
    guint i;
    
    g_return_if_fail (SEAHORSE_IS_OBJECT_MODEL (self));
    g_return_if_fail (G_IS_OBJECT (object) || object == NULL);
    g_return_if_fail (pv->data_column >= 0);
    
    /* Add the row/key association */
    if (object) {
        
        /* Do we already have a row for this key? */
        skrow = (SeahorseObjectRow*)g_hash_table_lookup (pv->rows, object);
        if (!skrow) {
            skrow = key_row_new (self, object);

            /* Put it in our row cache */
            g_hash_table_replace (pv->rows, object, skrow);
        }
        
        path = gtk_tree_model_get_path (GTK_TREE_MODEL (self), iter);
        g_ptr_array_add (skrow->refs, gtk_tree_row_reference_new (GTK_TREE_MODEL (self), path));
        gtk_tree_path_free (path);
        
    /* Remove the row/key association */
    } else {
        
        gtk_tree_model_get (GTK_TREE_MODEL (self), iter, pv->data_column, &skrow, -1);
        if (skrow) {
            
            ipath = gtk_tree_model_get_path (GTK_TREE_MODEL (self), iter);
            g_return_if_fail (ipath != NULL);
            
            for (i = 0; i < skrow->refs->len; i++) {
                
                path = gtk_tree_row_reference_get_path (g_ptr_array_index (skrow->refs, i));
                
                /* Check if they're the same or invalid, remove */
                if (!path || gtk_tree_path_compare (path, ipath) == 0) {
                    gtk_tree_row_reference_free (g_ptr_array_index (skrow->refs, i));
                    g_ptr_array_remove_index_fast (skrow->refs, i);
                    i--;
                }
 
                if (path)
                    gtk_tree_path_free (path);
            }
            
            /* If we no longer have rows associated with this key, then remove */
            if (skrow->refs->len == 0)
                g_hash_table_remove (pv->rows, skrow->object);
        }
    }
    
    gtk_tree_store_set (GTK_TREE_STORE (self), iter, 
                        pv->data_column, object ? skrow : NULL, -1);
    
    if (object)
        key_notify (object, self);
}

GObject *
seahorse_object_model_get_row_key (SeahorseObjectModel *self, GtkTreeIter *iter)
{
    SeahorseObjectModelPrivate *pv = SEAHORSE_OBJECT_MODEL_GET_PRIVATE (self);
    SeahorseObjectRow *skrow;
    
    g_return_val_if_fail (SEAHORSE_IS_OBJECT_MODEL (self), NULL);
    g_return_val_if_fail (pv->data_column >= 0, NULL);
    
    gtk_tree_model_get (GTK_TREE_MODEL (self), iter, pv->data_column, &skrow, -1);
    if (!skrow)
        return NULL;
    g_assert (G_IS_OBJECT (skrow->object));
    return skrow->object;
}

void
seahorse_object_model_remove_rows_for_object (SeahorseObjectModel *self,
                                              GObject *object)
{
    SeahorseObjectModelPrivate *pv = SEAHORSE_OBJECT_MODEL_GET_PRIVATE (self);
    SeahorseObjectRow *skrow;
    GtkTreeIter iter;
    GtkTreePath *path;
    guint i;
    
    g_return_if_fail (SEAHORSE_IS_OBJECT_MODEL (self));
    g_return_if_fail (G_IS_OBJECT (object));
    g_return_if_fail (pv->data_column >= 0);
    
    skrow = (SeahorseObjectRow*)g_hash_table_lookup (pv->rows, object);
    if (!skrow) 
        return;
    
    for (i = 0; i < skrow->refs->len; i++) {
        
        path = gtk_tree_row_reference_get_path (g_ptr_array_index (skrow->refs, i));
        if (path) {
            gtk_tree_model_get_iter (GTK_TREE_MODEL (self), &iter, path);
            gtk_tree_store_remove (GTK_TREE_STORE (self), &iter);
            gtk_tree_path_free (path);
        }
    }
    
    /* We no longer have rows associated with this key, then remove */
    g_hash_table_remove (pv->rows, object);
}

GList*
seahorse_object_model_get_rows_for_object (SeahorseObjectModel *self,
                                           GObject *object)
{
    SeahorseObjectModelPrivate *pv = SEAHORSE_OBJECT_MODEL_GET_PRIVATE (self);
    GList *rows = NULL;
    SeahorseObjectRow *skrow;
    GtkTreeIter *iter;
    GtkTreePath *path;
    guint i;
    
    g_return_val_if_fail (SEAHORSE_IS_OBJECT_MODEL (self), NULL);
    g_return_val_if_fail (G_IS_OBJECT (object), NULL);

    skrow = (SeahorseObjectRow*)g_hash_table_lookup (pv->rows, object);
    if (!skrow) 
        return NULL;
    
    for (i = 0; i < skrow->refs->len; i++) {
        
        path = gtk_tree_row_reference_get_path (g_ptr_array_index (skrow->refs, i));
        if (path) {
            iter = g_new0(GtkTreeIter, 1);
            gtk_tree_model_get_iter (GTK_TREE_MODEL (self), iter, path);
            rows = g_list_prepend (rows, iter);
            gtk_tree_path_free (path);
        }
    }
    
    return rows;
}

void
seahorse_object_model_free_rows (GList *rows)
{
    GList *l;
    for (l = rows; l; l = g_list_next (l))
        g_free (l->data);
    g_list_free (rows);
}
