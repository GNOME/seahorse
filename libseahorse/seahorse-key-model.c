/*
 * Seahorse
 *
 * Copyright (C) 2006 Nate Nielsen
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
#include <gnome.h>

#include "seahorse-key-model.h"
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

typedef struct _SeahorseKeyModelPrivate {
    GHashTable *rows;
    guint data_column;
} SeahorseKeyModelPrivate;

G_DEFINE_TYPE (SeahorseKeyModel, seahorse_key_model, GTK_TYPE_TREE_STORE);

#define SEAHORSE_KEY_MODEL_GET_PRIVATE(obj)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SEAHORSE_TYPE_KEY_MODEL, SeahorseKeyModelPrivate))

/* Internal data stored at 0 in the tree store in order to keep track
 * of the location, key-store and key.
 */
typedef struct {
    SeahorseKeyModel    *skmodel;
    GPtrArray           *refs;     /* GtkTreeRowReference pointers */
    SeahorseKey         *skey;     /* The key we're dealing with */
} SeahorseKeyRow;

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static void
key_changed (SeahorseKey *skey, SeahorseKeyChange change, 
             SeahorseKeyModel *skmodel)
{
    SeahorseKeyModelPrivate *pv = SEAHORSE_KEY_MODEL_GET_PRIVATE (skmodel);
    SeahorseKeyRow *skrow;
    GtkTreeIter iter;
    GtkTreePath *path;
    int i;

    skrow = g_hash_table_lookup (pv->rows, skey);
    if (!skrow)
        return;
    
    for (i = 0; i < skrow->refs->len; i++) {
        
        path = gtk_tree_row_reference_get_path (g_ptr_array_index (skrow->refs, i));
        if (path) {
            gtk_tree_model_get_iter (GTK_TREE_MODEL (skmodel), &iter, path);
            g_signal_emit (skmodel, signals[UPDATE_ROW], 0, skey, &iter);
            gtk_tree_path_free (path);
        }
    }
}

static void
key_destroyed (SeahorseKey *skey, SeahorseKeyModel *skmodel)
{
    SeahorseKeyModelPrivate *pv = SEAHORSE_KEY_MODEL_GET_PRIVATE (skmodel);
    g_hash_table_remove (pv->rows, skey);
}


static gboolean
remove_each (SeahorseKey *skey, gchar *path, SeahorseKeyModel *skmodel)
{
    return TRUE;
}

static SeahorseKeyRow*
key_row_new (SeahorseKeyModel *skmodel, SeahorseKey *skey)
{
    SeahorseKeyRow *skrow;
    
    g_assert (SEAHORSE_IS_KEY_MODEL (skmodel));
    g_assert (SEAHORSE_IS_KEY (skey));
    
    skrow = g_new0 (SeahorseKeyRow, 1);
    skrow->refs = g_ptr_array_new ();
    skrow->skmodel = skmodel;
    skrow->skey = skey;
    
    g_signal_connect (skey, "changed", G_CALLBACK (key_changed), skmodel);
    g_signal_connect (skey, "destroy", G_CALLBACK (key_destroyed), skmodel);
    
    return skrow;
}

static void
key_row_free (SeahorseKeyRow *skrow)
{
    SeahorseKeyModel *skmodel = skrow->skmodel;
    SeahorseKeyModelPrivate *pv = SEAHORSE_KEY_MODEL_GET_PRIVATE (skmodel);
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
                gtk_tree_model_get_iter (GTK_TREE_MODEL (skmodel), &iter, path);
                gtk_tree_store_set (GTK_TREE_STORE (skmodel), &iter, 
                                    pv->data_column, NULL, -1);
                gtk_tree_path_free (path);
            }
            gtk_tree_row_reference_free (ref);
        }
        
    }
    
    g_signal_handlers_disconnect_by_func (skrow->skey, key_changed, skrow->skmodel);
    g_signal_handlers_disconnect_by_func (skrow->skey, key_destroyed, skrow->skmodel);

    g_ptr_array_free (skrow->refs, TRUE);
    g_free (skrow);
}

static void
row_inserted (SeahorseKeyModel *skmodel, GtkTreePath *path, GtkTreeIter *iter, 
              gpointer user_data)
{
    SeahorseKeyModelPrivate *pv = SEAHORSE_KEY_MODEL_GET_PRIVATE (skmodel);
    g_return_if_fail (pv->data_column != -1);
    gtk_tree_store_set (GTK_TREE_STORE (skmodel), iter, pv->data_column, NULL, -1);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_key_model_init (SeahorseKeyModel *skmodel)
{
    SeahorseKeyModelPrivate *pv = SEAHORSE_KEY_MODEL_GET_PRIVATE (skmodel);
    pv->rows = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                      NULL, (GDestroyNotify)key_row_free);
    pv->data_column = -1;
    g_signal_connect (skmodel, "row-inserted", G_CALLBACK (row_inserted), NULL);
}

static void
seahorse_key_model_set_property (GObject *gobject, guint prop_id,
                                 const GValue *value, GParamSpec *pspec)
{
    SeahorseKeyModel *skmodel = SEAHORSE_KEY_MODEL (gobject);
    SeahorseKeyModelPrivate *pv = SEAHORSE_KEY_MODEL_GET_PRIVATE (skmodel);

    switch (prop_id) {
    case PROP_DATA_COLUMN:
        g_assert (pv->data_column == -1);
        pv->data_column = g_value_get_uint (value);
        break;
    }
}

static void
seahorse_key_model_get_property (GObject *gobject, guint prop_id,
                                 GValue *value, GParamSpec *pspec)
{
    SeahorseKeyModel *skmodel = SEAHORSE_KEY_MODEL (gobject);
    SeahorseKeyModelPrivate *pv = SEAHORSE_KEY_MODEL_GET_PRIVATE (skmodel);

    switch (prop_id) {
    case PROP_DATA_COLUMN:
        g_value_set_uint (value, pv->data_column);
        break;
    }
}

static void
seahorse_key_model_dispose (GObject *gobject)
{
    SeahorseKeyModel *skmodel = SEAHORSE_KEY_MODEL (gobject);
    SeahorseKeyModelPrivate *pv = SEAHORSE_KEY_MODEL_GET_PRIVATE (skmodel);
    
    /* Release all our pointers and stuff */
    g_hash_table_foreach_remove (pv->rows, (GHRFunc)remove_each, skmodel);
    G_OBJECT_CLASS (seahorse_key_model_parent_class)->dispose (gobject);
}

static void
seahorse_key_model_finalize (GObject *gobject)
{
    SeahorseKeyModel *skmodel = SEAHORSE_KEY_MODEL (gobject);
    SeahorseKeyModelPrivate *pv = SEAHORSE_KEY_MODEL_GET_PRIVATE (skmodel);

    if (pv->rows)
        g_hash_table_destroy (pv->rows);
    pv->rows = NULL;
    
    G_OBJECT_CLASS (seahorse_key_model_parent_class)->finalize (gobject);
}

static void
seahorse_key_model_class_init (SeahorseKeyModelClass *klass)
{
    GObjectClass *gobject_class;
    
    seahorse_key_model_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->dispose = seahorse_key_model_dispose;
    gobject_class->finalize = seahorse_key_model_finalize;
    gobject_class->set_property = seahorse_key_model_set_property;
    gobject_class->get_property = seahorse_key_model_get_property;
    
    g_object_class_install_property (gobject_class, PROP_DATA_COLUMN,
        g_param_spec_uint ("data-column", "Column data is stored", "Column where internal data is stored",
                           0, ~0, 0, G_PARAM_READWRITE | G_PARAM_READWRITE));

    signals[UPDATE_ROW] = g_signal_new ("update-row", SEAHORSE_TYPE_KEY_MODEL, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseKeyModelClass, update_row),
                NULL, NULL, seahorse_marshal_VOID__OBJECT_POINTER, G_TYPE_NONE, 2, SEAHORSE_TYPE_KEY, G_TYPE_POINTER);
    
    g_type_class_add_private (klass, sizeof (SeahorseKeyModelPrivate));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */


SeahorseKeyModel* 
seahorse_key_model_new (gint n_columns, GType *types)
{
    SeahorseKeyModel *model;
    
    model = g_object_new (SEAHORSE_TYPE_KEY_MODEL, NULL);
    seahorse_key_model_set_column_types (model, n_columns, types);
    
    return model;
}

void
seahorse_key_model_set_column_types (SeahorseKeyModel *skmodel, gint n_columns,
                                     GType *types)
{
    SeahorseKeyModelPrivate *pv = SEAHORSE_KEY_MODEL_GET_PRIVATE (skmodel);
    GType *itypes;
    
    g_return_if_fail (SEAHORSE_IS_KEY_MODEL (skmodel));

    itypes = g_new0(GType, n_columns + 1);
    memcpy (itypes, types, n_columns * sizeof (GType));

    itypes[n_columns] = G_TYPE_POINTER;
    pv->data_column = n_columns;
    gtk_tree_store_set_column_types (GTK_TREE_STORE (skmodel), n_columns + 1, itypes);
    
    g_free (itypes);
}

void
seahorse_key_model_set_row_key (SeahorseKeyModel *skmodel, GtkTreeIter *iter,
                                SeahorseKey *skey)
{
    SeahorseKeyModelPrivate *pv = SEAHORSE_KEY_MODEL_GET_PRIVATE (skmodel);
    SeahorseKeyRow *skrow;
    GtkTreePath *path;
    GtkTreePath *ipath;
    int i;
    
    g_return_if_fail (SEAHORSE_IS_KEY_MODEL (skmodel));
    g_return_if_fail (SEAHORSE_IS_KEY (skey) || skey == NULL);
    g_return_if_fail (pv->data_column >= 0);
    
    /* Add the row/key association */
    if (skey) {
        
        /* Do we already have a row for this key? */
        skrow = (SeahorseKeyRow*)g_hash_table_lookup (pv->rows, skey);
        if (!skrow) {
            skrow = key_row_new (skmodel, skey);

            /* Put it in our row cache */
            g_hash_table_replace (pv->rows, skey, skrow);
        }
        
        path = gtk_tree_model_get_path (GTK_TREE_MODEL (skmodel), iter);
        g_ptr_array_add (skrow->refs, gtk_tree_row_reference_new (GTK_TREE_MODEL (skmodel), path));
        gtk_tree_path_free (path);
        
    /* Remove the row/key association */
    } else {
        
        gtk_tree_model_get (GTK_TREE_MODEL (skmodel), iter, pv->data_column, &skrow, -1);
        if (skrow) {
            
            ipath = gtk_tree_model_get_path (GTK_TREE_MODEL (skmodel), iter);
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
                g_hash_table_remove (pv->rows, skrow->skey);
        }
    }
    
    gtk_tree_store_set (GTK_TREE_STORE (skmodel), iter, 
                        pv->data_column, skey ? skrow : NULL, -1);
    
    if (skey)
        key_changed (skey, 0, skmodel);
}

SeahorseKey*
seahorse_key_model_get_row_key (SeahorseKeyModel *skmodel, GtkTreeIter *iter)
{
    SeahorseKeyModelPrivate *pv = SEAHORSE_KEY_MODEL_GET_PRIVATE (skmodel);
    SeahorseKeyRow *skrow;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_MODEL (skmodel), NULL);
    g_return_val_if_fail (pv->data_column >= 0, NULL);
    
    gtk_tree_model_get (GTK_TREE_MODEL (skmodel), iter, pv->data_column, &skrow, -1);
    if (!skrow)
        return NULL;
    g_assert (SEAHORSE_IS_KEY (skrow->skey));
    return skrow->skey;
}

void
seahorse_key_model_remove_rows_for_key (SeahorseKeyModel *skmodel, SeahorseKey *skey)
{
    SeahorseKeyModelPrivate *pv = SEAHORSE_KEY_MODEL_GET_PRIVATE (skmodel);
    SeahorseKeyRow *skrow;
    GtkTreeIter iter;
    GtkTreePath *path;
    int i;
    
    g_return_if_fail (SEAHORSE_IS_KEY_MODEL (skmodel));
    g_return_if_fail (SEAHORSE_IS_KEY (skey));
    g_return_if_fail (pv->data_column >= 0);
    
    skrow = (SeahorseKeyRow*)g_hash_table_lookup (pv->rows, skey);
    if (!skrow) 
        return;
    
    for (i = 0; i < skrow->refs->len; i++) {
        
        path = gtk_tree_row_reference_get_path (g_ptr_array_index (skrow->refs, i));
        if (path) {
            gtk_tree_model_get_iter (GTK_TREE_MODEL (skmodel), &iter, path);
            gtk_tree_store_remove (GTK_TREE_STORE (skmodel), &iter);
            gtk_tree_path_free (path);
        }
    }
    
    /* We no longer have rows associated with this key, then remove */
    g_hash_table_remove (pv->rows, skey);
}

GSList*
seahorse_key_model_get_rows_for_key (SeahorseKeyModel *skmodel, SeahorseKey *skey)
{
    SeahorseKeyModelPrivate *pv = SEAHORSE_KEY_MODEL_GET_PRIVATE (skmodel);
    GSList *rows = NULL;
    SeahorseKeyRow *skrow;
    GtkTreeIter *iter;
    GtkTreePath *path;
    int i;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_MODEL (skmodel), NULL);
    g_return_val_if_fail (SEAHORSE_IS_KEY (skey), NULL);
    
    skrow = (SeahorseKeyRow*)g_hash_table_lookup (pv->rows, skey);
    if (!skrow) 
        return NULL;
    
    for (i = 0; i < skrow->refs->len; i++) {
        
        path = gtk_tree_row_reference_get_path (g_ptr_array_index (skrow->refs, i));
        if (path) {
            iter = g_new0(GtkTreeIter, 1);
            gtk_tree_model_get_iter (GTK_TREE_MODEL (skmodel), iter, path);
            rows = g_slist_prepend (rows, iter);
            gtk_tree_path_free (path);
        }
    }
    
    return rows;
}

void
seahorse_key_model_free_rows (GSList *rows)
{
    GSList *l;
    for (l = rows; l; l = g_slist_next (l))
        g_free (l->data);
    g_slist_free (rows);
}
