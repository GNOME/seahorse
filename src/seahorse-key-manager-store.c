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

#include "seahorse-key-manager-store.h"
#include "seahorse-validity.h"

/* Tree view columns */
enum {
	SKEY,
	NAME,
	KEYID,
	TRUST,
	ALGO,
	LENGTH,
	COLS
};

static void	seahorse_key_manager_store_class_init	(SeahorseKeyManagerStoreClass	*klass);

/* Virtual */
static void	seahorse_key_manager_store_set		(SeahorseKeyRow			*skrow,
							 GtkTreeIter			*parent);
static void	seahorse_key_manager_store_key_changed	(SeahorseKey			*skey,
							 SeahorseKeyChange		change,
							 SeahorseKeyRow			*skrow);

static SeahorseKeyStoreClass	*parent_class	= NULL;

GType
seahorse_key_manager_store_get_type (void)
{
	static GType key_manager_store_type = 0;
	
	if (!key_manager_store_type) {
		static const GTypeInfo key_manager_store_info =
		{
			sizeof (SeahorseKeyManagerStoreClass),
			NULL, NULL,
			(GClassInitFunc) seahorse_key_manager_store_class_init,
			NULL, NULL,
			sizeof (SeahorseKeyManagerStore),
			0, NULL
		};
		
		key_manager_store_type = g_type_register_static (SEAHORSE_TYPE_KEY_STORE,
			"SeahorseKeyManagerStore", &key_manager_store_info, 0);
	}
	
	return key_manager_store_type;
}

static void
seahorse_key_manager_store_class_init (SeahorseKeyManagerStoreClass *klass)
{
	SeahorseKeyStoreClass *skstore_class;
	
	parent_class = g_type_class_peek_parent (klass);
	skstore_class = SEAHORSE_KEY_STORE_CLASS (klass);
	
	skstore_class->set = seahorse_key_manager_store_set;
	skstore_class->changed = seahorse_key_manager_store_key_changed;
}

/* Sets attributes for row at iter using key index */
static void
set_attributes (SeahorseKeyRow *skrow, GtkTreeIter *iter, guint index)
{
	gtk_tree_store_set (skrow->store, iter,
		SKEY, skrow,
		NAME, seahorse_key_get_userid (skrow->skey, 0),
		KEYID, seahorse_key_get_keyid (skrow->skey, index),
		TRUST, seahorse_validity_get_trust_from_key (skrow->skey),
		ALGO, gpgme_key_get_string_attr (skrow->skey->key, GPGME_ATTR_ALGO, NULL, index),
		LENGTH, gpgme_key_get_ulong_attr (skrow->skey->key, GPGME_ATTR_LEN, NULL, index), -1);
}

static void
seahorse_key_manager_store_set (SeahorseKeyRow *skrow, GtkTreeIter *parent)
{
	GtkTreeIter iter;
	guint index = 1, max;
	
	set_attributes (skrow, parent, 0);
	max = seahorse_key_get_num_subkeys (skrow->skey);
	
	/* Sub keys */
	while (index <= max) {
		gtk_tree_store_append (skrow->store, &iter, parent);
		set_attributes (skrow, &iter, index);
		index++;
	}
}

/* Sets primary attributes */
static void
seahorse_key_manager_store_key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorseKeyRow *skrow)
{
	GtkTreeIter iter;
	
	g_return_if_fail (change == SKEY_CHANGE_TRUST);
	g_return_if_fail (gtk_tree_model_get_iter (GTK_TREE_MODEL (skrow->store), &iter,
		gtk_tree_row_reference_get_path (skrow->ref)));
	set_attributes (skrow, &iter, 0);
}

SeahorseKeyStore*
seahorse_key_manager_store_new (SeahorseContext *sctx, GtkTreeView *view)
{
	GtkTreeStore *store;
	GtkTreeViewColumn *column;
        GtkCellRenderer *renderer;

	static GType key_manager_columns[] = {
	        G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT
						        };
	store = g_object_new (SEAHORSE_TYPE_KEY_MANAGER_STORE, "ctx", sctx, NULL);
	
	/* Do columns */
	gtk_tree_store_set_column_types (store, COLS, key_manager_columns);
	
	gtk_tree_view_set_model (view, GTK_TREE_MODEL (store));	
	g_object_unref (store);
	
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("SeahorseKey", renderer, "text", SKEY, NULL);
	
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Name"), renderer, "text", NAME, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_append_column (view, column);
	
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Key ID"), renderer, "text", KEYID, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_append_column (view, column);
	
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Trust"), renderer, "text", TRUST, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_append_column (view, column);
	
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Type"), renderer, "text", ALGO, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_append_column (view, column);
	
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Length"), renderer, "text", LENGTH, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_append_column (view, column);
	
	seahorse_key_store_populate (SEAHORSE_KEY_STORE (store));
	
	return SEAHORSE_KEY_STORE (store);
}
