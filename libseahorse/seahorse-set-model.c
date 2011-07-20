/*
 * Seahorse
 * Copyright (C) 2006 Stefan Walter
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include "seahorse-set-model.h"

#include <string.h>

enum {
	PROP_0,
	PROP_SET
};

typedef struct _SeahorseSetModelPrivate {
	GHashTable *object_to_node;
	gint last_stamp;
	GNode *root_node;
	gchar **column_names;
	guint n_columns;
	GType *column_types;
} SeahorseSetModelPrivate;

static void remove_object (SeahorseSetModel *smodel, gpointer was_sobj);
static GNode* add_object (SeahorseSetModel *smodel, SeahorseObject *sobj);
static void seahorse_set_model_implement_tree_model (GtkTreeModelIface *iface);

G_DEFINE_TYPE_EXTENDED (SeahorseSetModel, seahorse_set_model, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, seahorse_set_model_implement_tree_model));

#define SEAHORSE_SET_MODEL_GET_PRIVATE(obj)  \
	(G_TYPE_INSTANCE_GET_PRIVATE ((obj), SEAHORSE_TYPE_SET_MODEL, SeahorseSetModelPrivate))

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */
static GNode* 
node_for_iter (SeahorseSetModelPrivate *pv, const GtkTreeIter *iter)
{
	GNode *node;

	g_return_val_if_fail (iter, NULL);
	g_return_val_if_fail (iter->stamp == pv->last_stamp, NULL);
	g_assert (SEAHORSE_IS_OBJECT (iter->user_data));

	node = iter->user_data2;
	g_assert (g_hash_table_lookup (pv->object_to_node, iter->user_data) == node);
	return node;
}

static void
iter_for_node (SeahorseSetModelPrivate *pv, GNode *node, GtkTreeIter *iter)
{
	g_assert (node);
	g_assert (SEAHORSE_IS_OBJECT (node->data));
	g_assert (g_hash_table_lookup (pv->object_to_node, node->data) == node);

	memset (iter, 0, sizeof (*iter));
	iter->stamp = pv->last_stamp;
	iter->user_data = node->data;
	iter->user_data2 = node;
}

static GtkTreePath*
path_for_node (GNode *node)
{
	GtkTreePath *path;
	gint index;

	g_assert (node);
	
	path = gtk_tree_path_new ();
	for (;;) {
		if (!node->parent) 
			break;
		index = g_node_child_position (node->parent, node);
		g_assert (index >= 0);
		gtk_tree_path_prepend_index (path, index);
		node = node->parent;
	}
	
	return path;
}

static void
key_hierarchy (SeahorseObject *sobj, GParamSpec *spec, SeahorseSetModel *smodel)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (smodel);

	g_return_if_fail (SEAHORSE_SET_MODEL (smodel));
	g_return_if_fail (SEAHORSE_IS_OBJECT (sobj));
	g_return_if_fail (g_hash_table_lookup (pv->object_to_node, sobj) != NULL);

	remove_object (smodel, sobj);
	add_object (smodel, sobj);
}

void
gone_object (SeahorseSetModel *smodel, gpointer was_sobj)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (smodel);
	GNode *node;
	
	node = g_hash_table_lookup (pv->object_to_node, was_sobj);
	g_assert (node);
	
	/* Mark this object as gone */
	node->data = NULL;
	
	remove_object (smodel, was_sobj);
}

static GNode*
add_object (SeahorseSetModel *smodel, SeahorseObject *sobj)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (smodel);
	SeahorseObject *parent_obj;
	GNode *parent_node, *node;
	GtkTreeIter iter;
	GtkTreePath *path;
	GList *children, *l;
	gboolean had;

	g_assert (g_hash_table_lookup (pv->object_to_node, sobj) == NULL);

	/* Find the parent of this object */
	parent_obj = seahorse_object_get_parent (sobj);
	g_return_val_if_fail (parent_obj != sobj, NULL);

	/* A root node */
	if (parent_obj == NULL) { 
		parent_node = pv->root_node;
	} else {
		/* Do we have the parent of this node? */
		parent_node = g_hash_table_lookup (pv->object_to_node, parent_obj);

		/* If not, we add it */
		if (!parent_node)
			parent_node = add_object (smodel, parent_obj);
	}

	/* Now that we have a parent, add this node */
	g_return_val_if_fail (parent_node, NULL);
	had = parent_node->children ? TRUE : FALSE;
	node = g_node_append_data (parent_node, sobj);
	g_object_weak_ref (G_OBJECT (sobj), (GWeakNotify)gone_object, smodel);
	g_hash_table_insert (pv->object_to_node, sobj, node);
	g_signal_connect (sobj, "notify::parent", G_CALLBACK (key_hierarchy), smodel);

	pv->last_stamp++;

	/* Fire signal for this added row */
	iter_for_node (pv, node, &iter);
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (smodel), &iter);
	g_return_val_if_fail (path, NULL);
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (smodel), path, &iter);
	pv->last_stamp++;
	gtk_tree_path_free (path);

	/* The first row was added, fire signal on parent */
	if (!had && parent_node != pv->root_node) {
		iter_for_node (pv, parent_node, &iter);
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (smodel), &iter);
		g_return_val_if_fail (path, NULL);
		gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (smodel), path, &iter);
		pv->last_stamp++;
		gtk_tree_path_free (path);
	}
	
	/* Now if this object is in the set, and has children, add each child */
	if (seahorse_set_has_object (smodel->set, sobj)) {
		children = seahorse_object_get_children (sobj);
		for (l = children; l; l = g_list_next (l)) {
			if (!g_hash_table_lookup (pv->object_to_node, l->data))
				add_object (smodel, l->data);
		}
	}

	return node;
}

static gboolean
remove_each_object (GNode *node, SeahorseSetModel *smodel)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (smodel);
	GNode *parent_node;
	GtkTreeIter iter;
	GtkTreePath *path;
	
	/* This happens during dispose */
	if (node == pv->root_node)
		return FALSE;
	
	/* This can be NULL of the object was finalized */
	if (node->data != NULL) {
		g_assert (SEAHORSE_IS_OBJECT (node->data));
		g_assert (g_hash_table_lookup (pv->object_to_node, node->data) == node);
	}

	/* Create the path for firing the event */
	path = path_for_node (node);
	g_return_val_if_fail (path, TRUE);

	/* Remove the actual node */
	parent_node = node->parent;
	if(node->data) {
		g_hash_table_remove (pv->object_to_node, node->data);
		g_signal_handlers_disconnect_by_func (node->data, key_hierarchy, smodel);
		g_object_weak_unref (G_OBJECT (node->data), (GWeakNotify)gone_object, smodel);
	}
	g_node_destroy (node);

	/* Fire signal for this removed row */
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (smodel), path);
	pv->last_stamp++;
	gtk_tree_path_free (path);

	/* If this is the last child, then fire toggled */
	if (!parent_node->children && parent_node != pv->root_node) {
		iter_for_node (pv, parent_node, &iter);
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (smodel), &iter);
		g_return_val_if_fail (path, TRUE);
		gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (smodel), path, &iter);
		pv->last_stamp++;
		gtk_tree_path_free (path);
	}
	
	return FALSE;
}

static void 
remove_object (SeahorseSetModel *smodel, gpointer was_sobj)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (smodel);
	GNode *node;
	GNode *parent;
	SeahorseObject *parent_obj;

	node = g_hash_table_lookup (pv->object_to_node, was_sobj);
	g_assert (node);
	g_assert (node != pv->root_node);
	
	/* 
	 * If the object has already dissappeared, then this will be 
	 * set to null by gone_object().
	 */
	g_assert (node->data == was_sobj || node->data == NULL);
	
	parent = node->parent;
	g_assert (parent);
	
	/* Remove this object and any children */
	g_node_traverse (node, G_POST_ORDER, G_TRAVERSE_ALL, -1, 
	                 (GNodeTraverseFunc)remove_each_object, smodel);

	/* This is needed for objects that went away */
	g_hash_table_remove (pv->object_to_node, was_sobj);

	/* 
	 * Now check if the parent of this object is actually in the set, or was 
	 * just added for the sake of holding the child (see add_object above)
	 */
	if (parent != pv->root_node && g_node_n_children (parent) == 0) {
		parent_obj = SEAHORSE_OBJECT (parent->data);
		g_return_if_fail (parent_obj);
		if (!seahorse_set_has_object (smodel->set, parent_obj))
			remove_object (smodel, parent_obj);
	}
}

static void
set_added (SeahorseSet *set, SeahorseObject *sobj, SeahorseSetModel *smodel)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (smodel);

	g_return_if_fail (SEAHORSE_SET_MODEL (smodel));
	g_return_if_fail (SEAHORSE_IS_OBJECT (sobj));

	/* We may have added this as a parent */
	if (g_hash_table_lookup (pv->object_to_node, sobj))
		return;
	/* Add a node to the table, plus all parents */
	add_object (smodel, sobj);
}

typedef struct {
	SeahorseSet *set;
	gboolean found;
} find_node_in_set_args;

static gboolean
find_node_in_set (GNode *node, gpointer user_data)
{
	find_node_in_set_args *args = user_data;
	if (seahorse_set_has_object (args->set, node->data)) {
		args->found = TRUE;
		return TRUE;
	}

	/* Continue traversal */
	return FALSE;
}

static void
set_removed (SeahorseSet *set, SeahorseObject *sobj, SeahorseSetModel *smodel)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (smodel);
	find_node_in_set_args args = { set, FALSE };
	GNode *node;
	
	g_return_if_fail (SEAHORSE_SET_MODEL (smodel));
	g_return_if_fail (SEAHORSE_IS_OBJECT (sobj));

	/* This should always already be added */
	node = g_hash_table_lookup (pv->object_to_node, sobj);
	g_return_if_fail (node != NULL);
	
	/* See if a child is still in the set? */
	g_node_traverse (node, G_IN_ORDER, G_TRAVERSE_ALL, -1, find_node_in_set, &args);
	
	/* It has a child in the set, don't remove it */
	if (args.found != FALSE)
		return;
	
	remove_object (smodel, sobj);
}

static void
set_changed (SeahorseSet *set, SeahorseObject *sobj, SeahorseSetModel *smodel)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (smodel);
	GtkTreeIter iter;
	GtkTreePath *path;
	GNode *node;

	g_return_if_fail (SEAHORSE_SET_MODEL (smodel));

	node = g_hash_table_lookup (pv->object_to_node, sobj);
	g_return_if_fail (node != NULL);

	memset (&iter, 0, sizeof (iter));
	iter_for_node (pv, node, &iter);
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (smodel), &iter);
	g_return_if_fail (path);
	gtk_tree_model_row_changed (GTK_TREE_MODEL (smodel), path, &iter);
	pv->last_stamp++;
	gtk_tree_path_free (path);
}

static void
populate_model (SeahorseSetModel *smodel)
{
	GList *objects, *l;
	objects = seahorse_set_get_objects (smodel->set);
	for (l = objects; l; l = g_list_next (l)) 
		set_added (smodel->set, SEAHORSE_OBJECT (l->data), smodel);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static GtkTreeModelFlags
seahorse_set_model_get_flags (GtkTreeModel *tree_model)
{
	/* TODO: Maybe we can eventually GTK_TREE_MODEL_ITERS_PERSIST */
	return 0;
}

static gint
seahorse_set_model_get_n_columns (GtkTreeModel *tree_model)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (tree_model);
	g_return_val_if_fail (SEAHORSE_SET_MODEL (tree_model), 0);
	return pv->n_columns;
}

static GType
seahorse_set_model_get_column_type (GtkTreeModel *tree_model, gint index)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (tree_model);
	g_return_val_if_fail (SEAHORSE_SET_MODEL (tree_model), 0);
	g_return_val_if_fail (index >= 0 && index < pv->n_columns, 0);
	return pv->column_types[index];
}

static gboolean
seahorse_set_model_get_iter (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (tree_model);
	gint i, count;
	gint *indices;
	GNode *node;

	g_return_val_if_fail (SEAHORSE_SET_MODEL (tree_model), FALSE);

	node = pv->root_node;
	g_assert (node);

	count = gtk_tree_path_get_depth (path);
	g_return_val_if_fail (count > 0, FALSE);
	indices = gtk_tree_path_get_indices (path);

	for (i = 0; i < count; ++i) {
		node = g_node_nth_child (node, indices[i]);
		if (!node)
			return FALSE;
	}

	iter_for_node (pv, node, iter);
	return TRUE;
}

static GtkTreePath*
seahorse_set_model_get_path (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (tree_model);
	GNode *node;

	g_return_val_if_fail (SEAHORSE_SET_MODEL (tree_model), NULL);
	node = node_for_iter (pv, iter);
	g_return_val_if_fail (node != NULL, NULL);
	return path_for_node (node);
}

static void
seahorse_set_model_get_value (GtkTreeModel *tree_model, GtkTreeIter *iter,
                              gint column, GValue *value)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (tree_model);
	SeahorseObject *sobj;
	const gchar *property;
	GParamSpec *spec;
	GValue original;
	GType type;

	g_return_if_fail (SEAHORSE_SET_MODEL (tree_model));
	sobj = seahorse_set_model_object_for_iter (SEAHORSE_SET_MODEL (tree_model), iter);
	g_return_if_fail (SEAHORSE_IS_OBJECT (sobj));
	g_return_if_fail (column >= 0 && column < pv->n_columns);

	/* Figure out which property */
	type = pv->column_types[column];
	property = pv->column_names[column];
	g_assert (property);
	g_value_init (value, type);

	/* Lookup the property on the object */
	spec = g_object_class_find_property (G_OBJECT_GET_CLASS (sobj), property);
	if (spec) {
		
		/* Simple, no transformation necessary */
		if (spec->value_type == type) {
			g_object_get_property (G_OBJECT (sobj), property, value);

		/* Not the same type, try to transform */
		} else {

			memset (&original, 0, sizeof (original));
			g_value_init (&original, spec->value_type);
			
			g_object_get_property (G_OBJECT (sobj), property, &original);
			if (!g_value_transform (&original, value)) { 
				g_warning ("%s property of %s class was of type %s instead of type %s"
				           " and cannot be converted", property, G_OBJECT_TYPE_NAME (sobj), 
				           g_type_name (spec->value_type), g_type_name (type));
				spec = NULL;
			}
		}

	
	} 
	
	/* No property present */
	if (spec == NULL) {
		
		/* All the number types have sane defaults */
		if (type == G_TYPE_STRING)
			g_value_set_string (value, "");
	}
}

static gboolean 
seahorse_set_model_iter_next (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (tree_model);
	GNode *node;

	g_return_val_if_fail (SEAHORSE_SET_MODEL (tree_model), FALSE);

	node = node_for_iter (pv, iter);
	g_return_val_if_fail (node != NULL, FALSE);
	node = g_node_next_sibling (node);
	if (node == NULL)
		return FALSE;

	iter_for_node (pv, node, iter);
	return TRUE;
}

static gboolean
seahorse_set_model_iter_children (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (tree_model);
	GNode *node;

	g_return_val_if_fail (SEAHORSE_SET_MODEL (tree_model), FALSE);
	if (parent == NULL)
		node = pv->root_node;
	else
		node = node_for_iter (pv, parent);
	g_return_val_if_fail (node != NULL, FALSE);
	node = node->children;
	if (node == NULL)
		return FALSE;

	iter_for_node (pv, node, iter);
	return TRUE;
}

static gboolean
seahorse_set_model_iter_has_child (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (tree_model);
	GNode *node;

	g_return_val_if_fail (SEAHORSE_SET_MODEL (tree_model), FALSE);
	node = node_for_iter (pv, iter);
	g_return_val_if_fail (node != NULL, FALSE);
	return node->children != NULL ? TRUE : FALSE;
}

static gint
seahorse_set_model_iter_n_children (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (tree_model);
	GNode *node;

	g_return_val_if_fail (SEAHORSE_SET_MODEL (tree_model), FALSE);
	if (iter == NULL)
		node = pv->root_node;
	else
		node = node_for_iter (pv, iter);
	g_return_val_if_fail (node != NULL, FALSE);
	return g_node_n_children (node);
}

static gboolean
seahorse_set_model_iter_nth_child (GtkTreeModel *tree_model, GtkTreeIter *iter,
                                   GtkTreeIter *parent, gint n)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (tree_model);
	GNode *node;

	g_return_val_if_fail (SEAHORSE_SET_MODEL (tree_model), FALSE);
	node = node_for_iter (pv, parent);
	g_return_val_if_fail (node != NULL, FALSE);
	node = g_node_nth_child (node, n);
	if (node == NULL)
		return FALSE;

	iter_for_node (pv, node, iter);
	return TRUE;
}

static gboolean 
seahorse_set_model_iter_parent (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *child)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (tree_model);
	GNode *node;

	g_return_val_if_fail (SEAHORSE_SET_MODEL (tree_model), FALSE);

	node = node_for_iter (pv, child);
	g_return_val_if_fail (node != NULL, FALSE);
	node = node->parent;
	if (!node || G_NODE_IS_ROOT (node)) 
		return FALSE;

	iter_for_node (pv, node, iter);
	return TRUE;
}

static void
seahorse_set_model_ref_node (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	/* Nothing to do */
}

static void
seahorse_set_model_unref_node (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	/* Nothing to do */
}

static void
seahorse_set_model_implement_tree_model (GtkTreeModelIface *iface)
{
	iface->get_flags = seahorse_set_model_get_flags;
	iface->get_n_columns = seahorse_set_model_get_n_columns;
	iface->get_column_type = seahorse_set_model_get_column_type;
	iface->get_iter = seahorse_set_model_get_iter;
	iface->get_path = seahorse_set_model_get_path;
	iface->get_value = seahorse_set_model_get_value;
	iface->iter_next = seahorse_set_model_iter_next;
	iface->iter_children = seahorse_set_model_iter_children;
	iface->iter_has_child = seahorse_set_model_iter_has_child;
	iface->iter_n_children = seahorse_set_model_iter_n_children;
	iface->iter_nth_child = seahorse_set_model_iter_nth_child;
	iface->iter_parent = seahorse_set_model_iter_parent;
	iface->ref_node = seahorse_set_model_ref_node;
	iface->unref_node = seahorse_set_model_unref_node;
}

static void
seahorse_set_model_init (SeahorseSetModel *smodel)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (smodel);
	pv->object_to_node = g_hash_table_new (g_direct_hash, g_direct_equal);
	pv->root_node = g_node_new (NULL);
	pv->column_names = NULL;
	pv->n_columns = 0;
	pv->column_types = NULL;
	pv->last_stamp = 0x1000;
}

static void
seahorse_set_model_set_property (GObject *gobject, guint prop_id,
                                 const GValue *value, GParamSpec *pspec)
{
	SeahorseSetModel *smodel = SEAHORSE_SET_MODEL (gobject);

	switch (prop_id) {
	case PROP_SET:
		g_return_if_fail (smodel->set == NULL);
		smodel->set = g_value_dup_object (value);
		if (smodel->set) {
			g_signal_connect_after (smodel->set, "added", G_CALLBACK (set_added), smodel);
			g_signal_connect_after (smodel->set, "removed", G_CALLBACK (set_removed), smodel);
			g_signal_connect_after (smodel->set, "changed", G_CALLBACK (set_changed), smodel);
			populate_model (smodel);
		}
		break;
	}
}

static void
seahorse_set_model_get_property (GObject *gobject, guint prop_id,
                                 GValue *value, GParamSpec *pspec)
{
	SeahorseSetModel *smodel = SEAHORSE_SET_MODEL (gobject);

	switch (prop_id) {
	case PROP_SET:
		g_value_set_object (value, smodel->set);
		break;
	}
}

static void
seahorse_set_model_dispose (GObject *gobject)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (gobject);
	SeahorseSetModel *smodel = SEAHORSE_SET_MODEL (gobject);

	/* Remove all rows */
	if (pv->root_node) {
		g_node_traverse (pv->root_node, G_POST_ORDER, G_TRAVERSE_ALL, -1, 
				 (GNodeTraverseFunc)remove_each_object, smodel);
		g_assert (g_hash_table_size (pv->object_to_node) == 0);
	}

	/* Disconnect from the set */
	if (smodel->set) {
		g_signal_handlers_disconnect_by_func (smodel->set, set_added, smodel);
		g_signal_handlers_disconnect_by_func (smodel->set, set_removed, smodel);
		g_signal_handlers_disconnect_by_func (smodel->set, set_changed, smodel);
		g_object_unref (smodel->set);	
		smodel->set = NULL;
	}

	G_OBJECT_CLASS (seahorse_set_model_parent_class)->dispose (gobject);
}

static void
seahorse_set_model_finalize (GObject *gobject)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (gobject);
	SeahorseSetModel *smodel = SEAHORSE_SET_MODEL (gobject);
	g_assert (!smodel->set);

	if (pv->object_to_node) {
		g_assert (g_hash_table_size (pv->object_to_node) == 0);
		g_hash_table_unref (pv->object_to_node);
		pv->object_to_node = NULL;
	}

	if (pv->root_node) {
		g_assert (!g_node_first_child (pv->root_node));
		g_node_destroy (pv->root_node);
		pv->root_node = NULL;
	}

	if (pv->column_names) {
		g_strfreev (pv->column_names);
		pv->column_names = NULL;
		pv->n_columns = 0;
	}

	if (pv->column_types) {
		g_free (pv->column_types);
		pv->column_types = NULL;
	}
    
	G_OBJECT_CLASS (seahorse_set_model_parent_class)->finalize (gobject);
}

static void
seahorse_set_model_class_init (SeahorseSetModelClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	seahorse_set_model_parent_class = g_type_class_peek_parent (klass);
	gobject_class->dispose = seahorse_set_model_dispose;
	gobject_class->finalize = seahorse_set_model_finalize;
	gobject_class->set_property = seahorse_set_model_set_property;
	gobject_class->get_property = seahorse_set_model_get_property;
    
	g_object_class_install_property (gobject_class, PROP_SET,
		g_param_spec_object ("set", "Object Set", "Set to get objects from",
				     SEAHORSE_TYPE_SET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_type_class_add_private (klass, sizeof (SeahorseSetModelPrivate));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseSetModel*
seahorse_set_model_new (SeahorseSet *set, ...)
{
	GArray *array = g_array_new (TRUE, TRUE, sizeof (SeahorseSetModelColumn));
	SeahorseSetModelColumn column;
	SeahorseSetModel *smodel;
	const gchar *arg;
	va_list va;

	va_start (va, set);
	while ((arg = va_arg (va, const gchar*)) != NULL) {
		column.property = arg;
		column.type = va_arg (va, GType);
		column.data = NULL;
		g_array_append_val (array, column);
	}
	va_end (va);

	smodel = seahorse_set_model_new_full (set, (SeahorseSetModelColumn*)array->data, array->len);
	g_array_free (array, TRUE);
	return smodel;
}

SeahorseSetModel*
seahorse_set_model_new_full (SeahorseSet *set, const SeahorseSetModelColumn *columns, guint n_columns)
{
	SeahorseSetModel *smodel = g_object_new (SEAHORSE_TYPE_SET, NULL);
	seahorse_set_model_set_columns (smodel, columns, n_columns);
	return smodel;
}

gint
seahorse_set_model_set_columns (SeahorseSetModel *smodel, const SeahorseSetModelColumn *columns,
                                guint n_columns)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (smodel);
	guint i;
	
	g_return_val_if_fail (SEAHORSE_IS_SET_MODEL (smodel), -1);
	g_return_val_if_fail (pv->n_columns == 0, -1);
	
	pv->column_names = g_new0 (gchar*, n_columns + 1);
	pv->column_types = g_new0 (GType, n_columns + 1);
	pv->n_columns = n_columns;
	
	for (i = 0; i < n_columns; ++i) {
		pv->column_names[i] = g_strdup (columns[i].property);
		pv->column_types[i] = columns[i].type;
	}
	
	return n_columns - 1;
}

SeahorseObject*
seahorse_set_model_object_for_iter (SeahorseSetModel *smodel, const GtkTreeIter *iter)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (smodel);
	SeahorseObject *sobj;

	g_return_val_if_fail (SEAHORSE_IS_SET_MODEL (smodel), NULL);
	g_return_val_if_fail (iter, NULL);
	g_return_val_if_fail (iter->stamp == pv->last_stamp, NULL);
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (iter->user_data), NULL);

	sobj = SEAHORSE_OBJECT (iter->user_data);
	g_return_val_if_fail (g_hash_table_lookup (pv->object_to_node, sobj) == iter->user_data2, NULL);
	return sobj;
}

gboolean
seahorse_set_model_iter_for_object (SeahorseSetModel *smodel, SeahorseObject *sobj,
				    GtkTreeIter *iter)
{
	SeahorseSetModelPrivate *pv = SEAHORSE_SET_MODEL_GET_PRIVATE (smodel);
	GNode *node;

	g_return_val_if_fail (SEAHORSE_IS_SET_MODEL (smodel), FALSE);
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (sobj), FALSE);
	g_return_val_if_fail (iter, FALSE);

	node = g_hash_table_lookup (pv->object_to_node, sobj);
	if (node == NULL)
		return FALSE;

	iter_for_node (pv, node, iter);
	return TRUE;
}

