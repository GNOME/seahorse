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

#include "seahorse-recipients-store.h"

/* Columns in the tree view */
enum {
	DATA,
	NAME,
	KEYID,
	COLS
};

static void	seahorse_recipients_store_class_init	(SeahorseRecipientsStoreClass	*klass);

static void	seahorse_recipients_store_append	(SeahorseKeyStore		*skstore,
							 SeahorseKey			*skey,
							 GtkTreeIter			*iter);
static void	seahorse_recipients_store_changed	(SeahorseKey			*skey,
							 SeahorseKeyChange		change,
							 SeahorseKeyStore		*skstore,
							 GtkTreeIter			*iter);

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
	
	skstore_class->append = seahorse_recipients_store_append;
	skstore_class->changed = seahorse_recipients_store_changed;
	
	klass->is_recip = seahorse_key_is_valid;
}

/* Checks if @skey is a valid recipient before appending */
static void
seahorse_recipients_store_append (SeahorseKeyStore *skstore,
				  SeahorseKey *skey, GtkTreeIter *iter)
{
	if (SEAHORSE_RECIPIENTS_STORE_GET_CLASS (skstore)->is_recip (skey)) {
		gtk_tree_store_append (GTK_TREE_STORE (skstore), iter, NULL);
		parent_class->append (skstore, skey, iter);
	}
}

/* Removes @skey if has been disabled */
static void
seahorse_recipients_store_changed (SeahorseKey *skey, SeahorseKeyChange change,
				   SeahorseKeyStore *skstore, GtkTreeIter *iter)
{
	switch (change) {
		case SKEY_CHANGE_DISABLE:
			if (!(SEAHORSE_RECIPIENTS_STORE_GET_CLASS (skstore)->is_recip (skey)))
				SEAHORSE_KEY_STORE_GET_CLASS (skstore)->remove (skstore, iter);
			break;
		default:
			parent_class->changed (skey, change, skstore, iter);
			break;
	}
}

/**
 * seahorse_recipients_store_new:
 * @sctx: Current #SeahorseContext
 * @view: #GtkTreeView that will show the new #SeahorseRecipientsStore
 *
 * Creates a new #SeahorseRecipientsStore and embeds in @view.
 * A regular recipients store is appropriate for export recipients.
 * Shown attributes are Name and KeyID.
 *
 * Returns: The new #SeahorseKeyStore
 **/
SeahorseKeyStore*
seahorse_recipients_store_new (SeahorseContext *sctx, GtkTreeView *view)
{
	SeahorseKeyStore *skstore;
	
	GType columns[] = {
	        G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_STRING
	};
	
	skstore = g_object_new (SEAHORSE_TYPE_RECIPIENTS_STORE, "ctx", sctx, NULL);
	seahorse_key_store_init (skstore, view, COLS, columns);

	return skstore;
}
