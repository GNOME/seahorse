/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

enum {
	SKROW,
	NAME,
	KEYID,
	COLS
};

/* Internal data stored at 0 in the tree store in order to keep track
 * of the location, key-store and key.
 */
typedef struct
{
	SeahorseKeyStore	*skstore;
	GtkTreeRowReference	*ref;
	SeahorseKey		*skey;
} SeahorseKeyRow;

static void	seahorse_key_store_class_init		(SeahorseKeyStoreClass	*klass);
static void	seahorse_key_store_finalize		(GObject		*gobject);
static void	seahorse_key_store_set_property		(GObject		*gobject,
							 guint			prop_id,
							 const GValue		*value,
							 GParamSpec		*pspec);
static void	seahorse_key_store_get_property		(GObject		*gobject,
							 guint			prop_id,
							 GValue			*value,
							 GParamSpec		*pspec);
/* Virtual methods */
static void	seahorse_key_store_append_key		(SeahorseKeyStore	*skstore,
							 SeahorseKey		*skey,
							 GtkTreeIter		*iter);
static void	seahorse_key_store_set			(GtkTreeStore		*store,
							 GtkTreeIter		*iter,
							 SeahorseKey		*skey);
static void	seahorse_key_store_remove_iter		(SeahorseKeyStore	*skstore,
							 GtkTreeIter		*iter);
static void	seahorse_key_store_changed		(SeahorseKey		*skey,
							 SeahorseKeyChange	change,
							 SeahorseKeyStore	*skstore,
							 GtkTreeIter		*iter);
/* Context signals */
static void	seahorse_key_store_context_destroyed	(GtkObject		*object,
							 SeahorseKeyStore	*skstore);
static void	seahorse_key_store_key_added		(SeahorseContext	*sctx,
							 SeahorseKey		*skey,
							 SeahorseKeyStore	*skstore);
/* Key signals */
static void	seahorse_key_store_key_destroyed	(GtkObject		*object,
							 SeahorseKeyRow		*skrow);
static void	seahorse_key_store_key_changed		(SeahorseKey		*skey,
							 SeahorseKeyChange	change,
							 SeahorseKeyRow		*skrow);
/* Key Row methods */
static void	seahorse_key_row_new			(SeahorseKeyStore	*skstore,
							 GtkTreeIter		*iter,
							 SeahorseKey		*skey);
static void	seahorse_key_row_remove			(SeahorseKeyRow		*skrow);

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
	
	klass->append = seahorse_key_store_append_key;
	klass->set = seahorse_key_store_set;
	klass->remove = seahorse_key_store_remove_iter;
	klass->changed = seahorse_key_store_changed;
	
	g_object_class_install_property (gobject_class, PROP_CTX,
		g_param_spec_object ("ctx", "Seahorse Context",
				     "Current Seahorse Context to use",
				     SEAHORSE_TYPE_CONTEXT, G_PARAM_READWRITE));
}

static void
seahorse_key_store_finalize (GObject *gobject)
{
	SeahorseKeyStore *skstore;
	
	skstore = SEAHORSE_KEY_STORE (gobject);
	
	g_signal_handlers_disconnect_by_func (GTK_OBJECT (skstore->sctx),
		seahorse_key_store_context_destroyed, skstore);
	g_signal_handlers_disconnect_by_func (skstore->sctx,
		seahorse_key_store_key_added, skstore);
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
			g_signal_connect_after (skstore->sctx, "destroy",
				G_CALLBACK (seahorse_key_store_context_destroyed), skstore);
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

/* Sets attributes then appends a new #SeahorseKeyRow */
static void
seahorse_key_store_append_key (SeahorseKeyStore *skstore, SeahorseKey *skey, GtkTreeIter *iter)
{
	SEAHORSE_KEY_STORE_GET_CLASS (skstore)->set (GTK_TREE_STORE (skstore), iter, skey);
	seahorse_key_row_new (skstore, iter, skey);
}

/* Sets Name and KeyID */
static void
seahorse_key_store_set (GtkTreeStore *store, GtkTreeIter *iter, SeahorseKey *skey)
{
    gchar *userid = seahorse_key_get_userid (skey, 0);
	gtk_tree_store_set (store, iter,
		NAME, userid,
		KEYID, seahorse_key_get_keyid (skey, 0), -1);
    g_free (userid);
}

/* Removes row at @iter from store */
static void
seahorse_key_store_remove_iter (SeahorseKeyStore *skstore, GtkTreeIter *iter)
{
	gtk_tree_store_remove (GTK_TREE_STORE (skstore), iter);
}

/* Refreshes key if uids have changed */
static void
seahorse_key_store_changed (SeahorseKey *skey, SeahorseKeyChange change,
			    SeahorseKeyStore *skstore, GtkTreeIter *iter)
{
	switch (change) {
		case SKEY_CHANGE_UIDS:
			SEAHORSE_KEY_STORE_GET_CLASS (skstore)->set (
				GTK_TREE_STORE (skstore), iter, skey);
			break;
		default:
			break;
	}
}

/* Destroys @skstore */
static void
seahorse_key_store_context_destroyed (GtkObject *object, SeahorseKeyStore *skstore)
{
	seahorse_key_store_destroy (skstore);
}

/* Appends @skey */
static void
seahorse_key_store_key_added (SeahorseContext *sctx, SeahorseKey *skey, SeahorseKeyStore *skstore)
{
	GtkTreeIter iter;
	
	SEAHORSE_KEY_STORE_GET_CLASS (skstore)->append (skstore, skey, &iter);
}

/* Removes @skrow */
static void
seahorse_key_store_key_destroyed (GtkObject *object, SeahorseKeyRow *skrow)
{
	seahorse_key_row_remove (skrow);
}

/* Gets location of @skey, then calls virtual changed() */
static void
seahorse_key_store_key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorseKeyRow *skrow)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	path = gtk_tree_row_reference_get_path (skrow->ref);
	g_return_if_fail (gtk_tree_model_get_iter (GTK_TREE_MODEL (skrow->skstore), &iter, path));
	SEAHORSE_KEY_STORE_GET_CLASS (skrow->skstore)->changed (skey, change, skrow->skstore, &iter);
}

/* Creates a new #SeahorseKeyRow for listening to key signals */
static void
seahorse_key_row_new (SeahorseKeyStore *skstore, GtkTreeIter *iter, SeahorseKey *skey)
{
	SeahorseKeyRow *skrow;
	GtkTreePath *path;

	skrow = g_new0 (SeahorseKeyRow, 1);
	skrow->skstore = skstore;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (skstore), iter);
	skrow->ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (skstore), path);

	skrow->skey = skey;
	g_object_ref (skey);
	g_signal_connect_after (GTK_OBJECT (skrow->skey), "destroy",
		G_CALLBACK (seahorse_key_store_key_destroyed), skrow);
	g_signal_connect_after (skrow->skey, "changed",
		G_CALLBACK (seahorse_key_store_key_changed), skrow);

	gtk_tree_store_set (GTK_TREE_STORE (skstore), iter, SKROW, skrow, -1);
}

/* Calls virtual remove() for @skrow's location, disconnects and unrefs
 * key, then frees itself.
 */
static void
seahorse_key_row_remove (SeahorseKeyRow *skrow)
{
	GtkTreeIter iter;
	GtkTreePath *path;

	path = gtk_tree_row_reference_get_path (skrow->ref);
	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (skrow->skstore), &iter, path))
		SEAHORSE_KEY_STORE_GET_CLASS (skrow->skstore)->remove (skrow->skstore, &iter);
	
	/* Unref key */
	g_signal_handlers_disconnect_by_func (G_OBJECT (skrow->skey),
		seahorse_key_store_key_destroyed, skrow);
	g_signal_handlers_disconnect_by_func (skrow->skey,
		seahorse_key_store_key_changed, skrow);
	g_object_unref (skrow->skey);
	
	gtk_tree_row_reference_free (skrow->ref);
	g_free (skrow);
}

/**
 * seahorse_key_store_init:
 * @skstore: #SeahorseKeyStore to initialize
 * @view: #GtkTreeView that will show @skstore
 * @cols: Number of columns to be in @view
 * @columns: Array of column types for @skstore
 *
 * Initializes @skstore with default columns and embeds in @view.
 * This must be called after creating a new #SeahorseKeyStore.
 **/
void
seahorse_key_store_init (SeahorseKeyStore *skstore, GtkTreeView *view,
			 gint cols, GType *columns)
{
	GtkTreeViewColumn *col;
	
	gtk_tree_store_set_column_types (GTK_TREE_STORE (skstore),
		cols, columns);
	gtk_tree_view_set_model (view, GTK_TREE_MODEL (skstore));
	
	col = seahorse_key_store_append_column (view, _("Name"), NAME);
	gtk_tree_view_column_set_sort_column_id (col, NAME);
	
	seahorse_key_store_append_column (view, _("Key ID"), KEYID);
}

/**
 * seahorse_key_store_destroy:
 * @skstore: #SeahorseKeyStore to destroy
 *
 * Empties @skstore in order to unref any keys it contains.
 * Also unrefs @skstore.  Call this method if @skstore does not exist
 * for the life of the program.
 **/
void
seahorse_key_store_destroy (SeahorseKeyStore *skstore)
{
	SeahorseKeyRow *skrow;
	GtkTreeIter iter;
	
	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (skstore), &iter)) {
		do {
			gtk_tree_model_get (GTK_TREE_MODEL (skstore), &iter, SKROW, &skrow, -1);
			seahorse_key_row_remove (skrow);
		} while (GTK_IS_TREE_MODEL (skstore) &&
			 gtk_tree_model_get_iter_first (GTK_TREE_MODEL (skstore), &iter));
	}
	g_object_unref (skstore);
}

/**
 * seahorse_key_store_populate:
 * @skstore: #SeahorseKeyStore to populate with #SeahorseKeys
 *
 * A new #SeahorseKeyStore is initially empty. Call this method to populate
 * the #SeahorseKeyStore with #SeahorseKeys.
 **/
void
seahorse_key_store_populate (SeahorseKeyStore *skstore)
{
	GList *list = NULL;
	SeahorseKey *skey;
	guint count = 1;
	gdouble length;
	GtkTreeIter iter;
	
	g_return_if_fail (SEAHORSE_IS_KEY_STORE (skstore));
	
	list = seahorse_context_get_keys (skstore->sctx);
	length = g_list_length (list);
	
	while (list != NULL && (skey = list->data) != NULL) {
		SEAHORSE_KEY_STORE_GET_CLASS (skstore)->append (skstore, skey, &iter);
		list = g_list_next (list);
		count++;
	}
	
	seahorse_context_show_progress (skstore->sctx,
		g_strdup_printf (_("Listed %d keys"), count), -1);
}

/**
 * seahorse_key_store_get_key_from_path:
 * @view: #GtkTreeView containing @path
 * @path: #GtkTreePath containing a #SeahorseKey
 *
 * Gets the #SeahorseKey at @path in @view.
 *
 * Returns: The #SeahorseKey at @path in @view
 **/
SeahorseKey*
seahorse_key_store_get_key_from_path (GtkTreeView *view, GtkTreePath *path)
{
	SeahorseKeyRow *skrow;
	GtkTreeModel *model;
	GtkTreeIter iter, parent;
	
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
	g_return_val_if_fail (path != NULL, NULL);
	
	model = gtk_tree_view_get_model (view);
	g_return_val_if_fail (gtk_tree_model_get_iter (model, &iter, path), NULL);
	if (gtk_tree_model_iter_parent (model, &parent, &iter))
		iter = parent;
	gtk_tree_model_get (model, &iter, SKROW, &skrow, -1);
	
	return skrow->skey;
}

/**
 * seahorse_key_store_append_column:
 * @view: #GtkTreeView to append column to
 * @name: Title of new column
 * @index: Index of new column
 *
 * Creates a new #GtkTreeViewColumn with @name as the title,
 * and appends it to @view at @index.
 *
 * Returns: The created column
 **/
GtkTreeViewColumn*
seahorse_key_store_append_column (GtkTreeView *view, const gchar *name, const gint index)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (name, renderer, "text", index, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_append_column (view, column);
	
	return column;
}

/**
 * seahorse_key_store_get_selected_recips:
 * @view: #GtkTreeView with selection
 *
 * Gets a recipient list of the selected keys.
 *
 * Returns: A recipient list of the selected keys
 **/
gpgme_key_t *
seahorse_key_store_get_selected_recips (GtkTreeView *view)
{
	GList *list = NULL, *keys;
	gpgme_key_t * recips;
	gint n;
	
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
	
	list = seahorse_key_store_get_selected_keys (view);
	g_return_val_if_fail (list != NULL, NULL);

	/* do recipients list */
	recips = g_new0(gpgme_key_t, g_list_length (list) + 1);
	n = 0;
	for (keys = list; keys != NULL; keys = g_list_next (keys)) {
	        gpgme_key_ref (SEAHORSE_KEY (keys->data)->key);
		recips[n++] = SEAHORSE_KEY (keys->data)->key;
	}

	/* free list, return */
	g_list_free (list);
	return recips;
}

/**
 * seahorse_key_store_get_selected_keys:
 * @view: #GtkTreeView with selection
 *
 * Gets a list of the selected #SeahorseKeys.
 *
 * Returns: The selected #SeahorseKeys
 **/
GList*
seahorse_key_store_get_selected_keys (GtkTreeView *view)
{
	GtkTreeSelection *selection;
	GList *paths = NULL, *list, *keys = NULL;
	
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
	
	selection = gtk_tree_view_get_selection (view);
	paths = gtk_tree_selection_get_selected_rows (selection, NULL);
	g_return_val_if_fail (paths != NULL && g_list_length (paths) > 0, NULL);
	
	/* make key list */
	for (list = paths; list != NULL; list = g_list_next (list))
		keys = g_list_append (keys, seahorse_key_store_get_key_from_path (view, list->data));
	/* free selected paths */
	g_list_foreach (paths, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (paths);
	
	return keys;
}

/**
 * seahorse_key_store_get_selected_key:
 * @view: #GtkTreeView with selection
 *
 * Sugar method for getting the selected #SeahorseKey from @view.
 *
 * Returns: The selected #SeahorseKey, or NULL there are no selections or
 * more than one selection.
 **/
SeahorseKey*
seahorse_key_store_get_selected_key (GtkTreeView *view)
{
	GList *list = NULL;
	SeahorseKey *skey;
	
	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
	
	/* get selected keys */
	list = seahorse_key_store_get_selected_keys (view);
	g_return_val_if_fail (list != NULL, NULL);
	g_return_val_if_fail (g_list_length (list) == 1, NULL);
	
	/* get first key, free list */
	skey = list->data;
	g_list_free (list);
	
	return skey;
}
