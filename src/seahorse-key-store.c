/*
 * Seahorse
 *
 * Copyright (C) 2002 Jacob Perkins
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

#include <gnome.h>

#include "seahorse-key-store.h"

enum {
	PROP_0,
	PROP_CTX
};

static void		seahorse_key_store_class_init		(SeahorseKeyStoreClass	*klass);
static void		seahorse_key_store_finalize		(GObject		*gobject);
static void		seahorse_key_store_set_property		(GObject		*gobject,
								 guint			prop_id,
								 const GValue		*value,
								 GParamSpec		*pspec);
static void		seahorse_key_store_get_property		(GObject		*gobject,
								 guint			prop_id,
								 GValue			*value,
								 GParamSpec		*pspec);
/* Context signal */
static SeahorseKeyRow*	seahorse_key_store_key_added		(SeahorseContext	*sctx,
								 SeahorseKey		*skey,
								 SeahorseKeyStore	*skstore);
/* Key signal */
static void		seahorse_key_store_key_destroyed	(GtkObject		*object,
								 SeahorseKeyRow		*skrow);
/* Key row */
static SeahorseKeyRow*	seahorse_key_row_new			(GtkTreeStore		*store,
								 GtkTreePath		*path,
								 SeahorseKey		*skey);
static void		seahorse_key_row_remove			(SeahorseKeyRow		*skrow);

static GtkTreeStoreClass	*parent_class	= NULL;

GType
seahorse_key_store_get_type (void)
{
	static GType key_store_type = 0;
	
	if (!key_store_type) {
		static const GTypeInfo key_store_info =
		{
			sizeof (SeahorseKeyStoreClass),
			NULL, NULL,
			(GClassInitFunc) seahorse_key_store_class_init,
			NULL, NULL,
			sizeof (SeahorseKeyStore),
			0, NULL
		};
		
		key_store_type = g_type_register_static (GTK_TYPE_TREE_STORE,
			"SeahorseKeyStore", &key_store_info, 0);
	}
	
	return key_store_type;
}

static void
seahorse_key_store_class_init (SeahorseKeyStoreClass *klass)
{
	GObjectClass *gobject_class;
	
	parent_class = g_type_class_peek_parent (klass);
	gobject_class = G_OBJECT_CLASS (klass);
	
	gobject_class->finalize = seahorse_key_store_finalize;
	gobject_class->set_property = seahorse_key_store_set_property;
	gobject_class->get_property = seahorse_key_store_get_property;
	
	klass->changed = NULL;
	klass->set = NULL;
	
	g_object_class_install_property (gobject_class, PROP_CTX,
		g_param_spec_object ("ctx", _("Seahorse Context"),
				     _("Current Seahorse Context to use"),
				     SEAHORSE_TYPE_CONTEXT, G_PARAM_READWRITE));
}

/* Only release context, which will release keys, which will release key rows */
static void
seahorse_key_store_finalize (GObject *gobject)
{
	SeahorseKeyStore *skstore;
	
	skstore = SEAHORSE_KEY_STORE (gobject);
	g_signal_handlers_disconnect_by_func (skstore->sctx, seahorse_key_store_key_added, skstore);
	g_object_unref (skstore->sctx);
	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
seahorse_key_store_set_property (GObject *gobject, guint prop_id,
				 const GValue *value, GParamSpec *pspec)
{
	SeahorseKeyStore *skstore;
	
	skstore = SEAHORSE_KEY_STORE (gobject);
	
	switch (prop_id) {
		/* Connects to context signals */
		case PROP_CTX:
			skstore->sctx = g_value_get_object (value);
			g_object_ref (skstore->sctx);
			g_signal_connect_after (skstore->sctx, "add",
				G_CALLBACK (seahorse_key_store_key_added), skstore);
			break;
		default:
			break;
	}
}

static void
seahorse_key_store_get_property (GObject *gobject, guint prop_id,
				 GValue *value, GParamSpec *pspec)
{
	SeahorseKeyStore *skstore;
	
	skstore = SEAHORSE_KEY_STORE (gobject);
	
	switch (prop_id) {
		case PROP_CTX:
			g_value_set_object (value, skstore->sctx);
			break;
		default:
			break;
	}
}

/* Called when a key row needs to be added to the store */
static SeahorseKeyRow*
seahorse_key_store_key_added (SeahorseContext *sctx, SeahorseKey *skey, SeahorseKeyStore *skstore)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	SeahorseKeyRow *skrow;
	
	gtk_tree_store_append (GTK_TREE_STORE (skstore), &iter, NULL);
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (skstore), &iter);
	g_return_if_fail (path != NULL);
	
	skrow = seahorse_key_row_new (GTK_TREE_STORE (skstore), path, skey);
	SEAHORSE_KEY_STORE_GET_CLASS (skstore)->set (skrow, &iter);
	
	return skrow;
}

/* Called when a key row needs to be removed from the store */
static void
seahorse_key_store_key_destroyed (GtkObject *object, SeahorseKeyRow *skrow)
{
	seahorse_key_row_remove (skrow);
}

/* Creates a new key row from a tree store, a new path, and a key */
static SeahorseKeyRow*
seahorse_key_row_new (GtkTreeStore *store, GtkTreePath *path, SeahorseKey *skey)
{
	SeahorseKeyRow *skrow;
	
	skrow = g_new0 (SeahorseKeyRow, 1);
	skrow->store = store;
	skrow->ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (store), path);
	skrow->skey = skey;
	
	/* Ref key */
	g_object_ref (skey);
	g_signal_connect_after (GTK_OBJECT (skrow->skey), "destroy", G_CALLBACK (seahorse_key_store_key_destroyed), skrow);
	g_signal_connect_after (skrow->skey, "changed", G_CALLBACK (SEAHORSE_KEY_STORE_GET_CLASS (store)->changed), skrow);
	
	return skrow;
}

/* Removes row's path from its store, frees path, unrefs key, then frees itself */
static void
seahorse_key_row_remove (SeahorseKeyRow *skrow)
{
	GtkTreeIter parent;
	GtkTreeIter iter;
	
	/* Remove itself and all children */
	if (skrow->store != NULL && gtk_tree_model_get_iter (GTK_TREE_MODEL (skrow->store), &parent,
	    gtk_tree_row_reference_get_path (skrow->ref))) {
		while (gtk_tree_model_iter_children (GTK_TREE_MODEL (skrow->store), &iter, &parent))
			gtk_tree_store_remove (skrow->store, &iter);
		gtk_tree_store_remove (skrow->store, &parent);
	}
	
	/* Unref key */
	g_signal_handlers_disconnect_by_func (G_OBJECT (skrow->skey), seahorse_key_store_key_destroyed, skrow);
	g_signal_handlers_disconnect_by_func (skrow->skey, SEAHORSE_KEY_STORE_GET_CLASS (skrow->store)->changed, skrow);
	g_object_unref (skrow->skey);
	
	gtk_tree_row_reference_free (skrow->ref);
	g_free (skrow);
}

SeahorseKeyRow*
seahorse_key_row_transfer (SeahorseKeyRow *lhs, SeahorseKeyStore *skstore)
{
	SeahorseKeyRow *rhs;
	
	g_return_if_fail (lhs != NULL);
	g_return_if_fail (SEAHORSE_IS_KEY (lhs->skey));
	g_return_if_fail (SEAHORSE_IS_KEY_STORE (skstore));
	
	rhs = seahorse_key_store_key_added (skstore->sctx, lhs->skey, skstore);
	seahorse_key_row_remove (lhs);
	
	return rhs;
}

void
seahorse_key_store_destroy (SeahorseKeyStore *skstore)
{
	SeahorseKeyRow *skrow;
	GtkTreeIter iter;
	
	g_return_if_fail (SEAHORSE_IS_KEY_STORE (skstore));
	
	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (skstore), &iter)) {
		do {
			gtk_tree_model_get (GTK_TREE_MODEL (skstore), &iter, 0, &skrow, -1);
			seahorse_key_row_remove (skrow);
		} while (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (skstore), &iter));
	}
}

void
seahorse_key_store_populate (SeahorseKeyStore *skstore)
{
	GList *list = NULL;
	SeahorseKey *skey;
	
	g_return_if_fail (SEAHORSE_IS_KEY_STORE (skstore));
	
	list = seahorse_context_get_keys (skstore->sctx);
	while (list != NULL && (skey = list->data) != NULL) {
		seahorse_key_store_key_added (skstore->sctx, skey, skstore);
		list = g_list_next (list);
	}
}

/* Returns the selected path */
static GtkTreePath*
get_selected_path (GtkTreeView *view)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
	
	selection = gtk_tree_view_get_selection (view);
	g_return_val_if_fail (gtk_tree_selection_get_selected (selection, &model, &iter), NULL);
	return gtk_tree_model_get_path (model, &iter);
}

SeahorseKey*
seahorse_key_store_get_selected_key (GtkTreeView *view)
{
	GtkTreePath *path;
	
	path = get_selected_path (view);
	return seahorse_key_store_get_key_from_path (view, path);
}

SeahorseKey*
seahorse_key_store_get_key_from_path (GtkTreeView *view, GtkTreePath *path)
{
	SeahorseKeyRow *skrow;
	
	skrow = seahorse_key_store_get_row_from_path (view, path);
	g_return_val_if_fail (skrow != NULL, NULL);
	
	return skrow->skey;
}

SeahorseKeyRow*
seahorse_key_store_get_selected_row (GtkTreeView *view)
{
	GtkTreePath *path;
	
	path = get_selected_path (view);
	return seahorse_key_store_get_row_from_path (view, path);
}

SeahorseKeyRow*
seahorse_key_store_get_row_from_path (GtkTreeView *view, GtkTreePath *path)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	SeahorseKeyRow *skrow;
	
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
	g_return_val_if_fail (path != NULL, NULL);
	
	model = gtk_tree_view_get_model (view);
	g_return_val_if_fail (gtk_tree_model_get_iter (model, &iter, path), NULL);
	gtk_tree_model_get (model, &iter, 0, &skrow, -1);
	
	return skrow;
}
