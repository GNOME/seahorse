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

static void	seahorse_key_manager_store_append	(SeahorseKeyStore		*skstore,
							 SeahorseKey			*skey,
							 GtkTreeIter			*iter);
static void	seahorse_key_manager_store_set		(GtkTreeStore			*store,
							 GtkTreeIter			*iter,
							 SeahorseKey			*skey);
static void	seahorse_key_manager_store_remove	(SeahorseKeyStore		*skstore,
							 GtkTreeIter			*iter);
static void	seahorse_key_manager_store_changed	(SeahorseKey			*skey,
							 SeahorseKeyChange		change,
							 SeahorseKeyStore		*skstore,
							 GtkTreeIter			*iter);

static void	seahorse_key_manager_store_set_attribs	(GtkTreeStore			*store,
							 GtkTreeIter			*iter,
							 SeahorseKey			*skey,
							 guint				index);

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
	
	skstore_class->append = seahorse_key_manager_store_append;
	skstore_class->set = seahorse_key_manager_store_set;
	skstore_class->remove = seahorse_key_manager_store_remove;
	skstore_class->changed = seahorse_key_manager_store_changed;
}

static void
seahorse_key_manager_store_append (SeahorseKeyStore *skstore, SeahorseKey *skey, GtkTreeIter *iter)
{
	gtk_tree_store_append (GTK_TREE_STORE (skstore), iter, NULL);
	parent_class->append (skstore, skey, iter);
}

static void
remove_subkeys (GtkTreeStore *store, GtkTreeIter *iter)
{
	GtkTreeIter child;

	while (gtk_tree_model_iter_children (GTK_TREE_MODEL (store), &child, iter))
		gtk_tree_store_remove (store, &child);
}

static void
seahorse_key_manager_store_set (GtkTreeStore *store, GtkTreeIter *iter, SeahorseKey *skey)
{
	GtkTreeIter child;
	gint index = 1, max;
	
	remove_subkeys (store, iter);
	seahorse_key_manager_store_set_attribs (store, iter, skey, 0);
	max = seahorse_key_get_num_subkeys (skey);

	while (index <= max) {
		gtk_tree_store_append (store, &child, iter);
		seahorse_key_manager_store_set_attribs (store, iter, skey, index);
		index++;
	}
}

static void
seahorse_key_manager_store_remove (SeahorseKeyStore *skstore, GtkTreeIter *iter)
{
	remove_subkeys (GTK_TREE_STORE (skstore), iter);
	parent_class->remove (skstore, iter);
}

static void
seahorse_key_manager_store_changed (SeahorseKey *skey, SeahorseKeyChange change,
				    SeahorseKeyStore *skstore, GtkTreeIter *iter)
{
	switch (change) {
		case SKEY_CHANGE_TRUST:
			SEAHORSE_KEY_STORE_GET_CLASS (skstore)->set (
				GTK_TREE_STORE (skstore), iter, skey);
			break;
		default:
			break;
	}

	parent_class->changed (skey, change, skstore, iter);
}

/* Sets attributes for row at iter using key index */
static void
seahorse_key_manager_store_set_attribs (GtkTreeStore *store, GtkTreeIter *iter,
				       SeahorseKey *skey, guint index)
{
	gtk_tree_store_set (store, iter,
		NAME, seahorse_key_get_userid (skey, 0),
		KEYID, seahorse_key_get_keyid (skey, index),
		TRUST, seahorse_validity_get_trust_from_key (skey),
		ALGO, gpgme_key_get_string_attr (skey->key, GPGME_ATTR_ALGO, NULL, index),
		LENGTH, gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_LEN, NULL, index), -1);
}

SeahorseKeyStore*
seahorse_key_manager_store_new (SeahorseContext *sctx, GtkTreeView *view)
{
	SeahorseKeyStore *skstore;

	GType columns[] = {
	        G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT
        };

	skstore = g_object_new (SEAHORSE_TYPE_KEY_MANAGER_STORE, "ctx", sctx, NULL);
	seahorse_key_store_init (skstore, view, COLS, columns);

	seahorse_key_store_append_column (view, _("Trust"), TRUST);
	seahorse_key_store_append_column (view, _("Type"), ALGO);
	seahorse_key_store_append_column (view, _("Length"), LENGTH);
	
	seahorse_key_store_populate (skstore);
	
	return skstore;
}
