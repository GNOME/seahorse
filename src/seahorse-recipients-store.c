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

#include "seahorse-recipients-store.h"

/* Columns in the tree view */
enum {
	SKEY,
	NAME,
	KEYID,
	VALIDITY,
	COLS
};

static void	seahorse_recipients_store_class_init	(SeahorseRecipientsStoreClass	*klass);

/* Virtual methods */
static void	seahorse_recipients_store_set		(SeahorseKeyRow			*skrow,
							 GtkTreeIter			*parent);
static void	seahorse_recipients_store_key_changed	(SeahorseKey			*skey,
							 SeahorseKeyChange		change,
							 SeahorseKeyRow			*skrow);

static SeahorseKeyStoreClass	*parent_class	= NULL;

GType
seahorse_recipients_store_get_type (void)
{
	static GType recipients_store_type = 0;
	
	if (!recipients_store_type) {
		static const GTypeInfo recipients_store_info =
		{
			sizeof (SeahorseRecipientsStoreClass),
			NULL, NULL,
			(GClassInitFunc) seahorse_recipients_store_class_init,
			NULL, NULL,
			sizeof (SeahorseRecipientsStore),
			0, NULL
		};
		
		recipients_store_type = g_type_register_static (SEAHORSE_TYPE_KEY_STORE,
			"SeahorseRecipientsStore", &recipients_store_info, 0);
	}
	
	return recipients_store_type;
}

static void
seahorse_recipients_store_class_init (SeahorseRecipientsStoreClass *klass)
{
	SeahorseKeyStoreClass *skstore_class;
	
	parent_class = g_type_class_peek_parent (klass);
	skstore_class = SEAHORSE_KEY_STORE_CLASS (klass);
	
	skstore_class->set = seahorse_recipients_store_set;
	skstore_class->changed = seahorse_recipients_store_key_changed;
}

/* Sets attributes for row starting at iter */
static void
set_attributes (SeahorseKeyRow *skrow, GtkTreeIter *iter)
{
	gtk_tree_store_set (skrow->store, iter,
		SKEY, skrow,
		NAME, seahorse_key_get_userid (skrow->skey, 0),
		KEYID, seahorse_key_get_keyid (skrow->skey, 0),
		VALIDITY, seahorse_validity_get_validity_from_key (skrow->skey), -1);
}

static void
seahorse_recipients_store_set (SeahorseKeyRow *skrow, GtkTreeIter *parent)
{
	set_attributes (skrow, parent);
}

static void
seahorse_recipients_store_key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorseKeyRow *skrow)
{}

SeahorseKeyStore*
seahorse_recipients_store_new (SeahorseContext *sctx, GtkTreeView *view)
{
	SeahorseKeyStore *skstore;
	GtkTreeStore *store;
	GtkTreeViewColumn *column;
        GtkCellRenderer *renderer;
	static GType recipients_columns[] = {
	        G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING
	};
	
	skstore = g_object_new (SEAHORSE_TYPE_RECIPIENTS_STORE, "ctx", sctx, NULL);
	store = GTK_TREE_STORE (skstore);
	
	/* Initialize the tree view columns */
	gtk_tree_store_set_column_types (store, COLS, recipients_columns);
	
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
	column = gtk_tree_view_column_new_with_attributes (_("Validity"), renderer, "text", VALIDITY, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_append_column (view, column);
	
	return skstore;
}
