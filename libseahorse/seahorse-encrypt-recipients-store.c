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

#include "seahorse-encrypt-recipients-store.h"

/* Columns in the tree view */
enum {
	DATA,
	NAME,
	KEYID,
	VALIDITY_STR,
	VALIDITY,
	COLS
};

static void	seahorse_encrypt_recipients_store_class_init	(SeahorseEncryptRecipientsStoreClass	*klass);

static void	seahorse_encrypt_recipients_store_set		(GtkTreeStore				*store,
								 GtkTreeIter				*iter,
								 SeahorseKey				*skey);
static void	seahorse_encrypt_recipients_store_changed	(SeahorseKey				*skey,
								 SeahorseKeyChange			change,
								 SeahorseKeyStore			*skstore,
								 GtkTreeIter				*iter);

static SeahorseRecipientsStoreClass	*parent_class	= NULL;

GType
seahorse_encrypt_recipients_store_get_type (void)
{
	static GType encrypt_recipients_store_type = 0;
	
	if (!encrypt_recipients_store_type) {
		static const GTypeInfo encrypt_recipients_store_info =
		{
			sizeof (SeahorseEncryptRecipientsStoreClass),
			NULL, NULL,
			(GClassInitFunc) seahorse_encrypt_recipients_store_class_init,
			NULL, NULL,
			sizeof (SeahorseEncryptRecipientsStore),
			0, NULL
		};
		
		encrypt_recipients_store_type = g_type_register_static (SEAHORSE_TYPE_RECIPIENTS_STORE,
			"SeahorseEncryptRecipientsStore", &encrypt_recipients_store_info, 0);
	}
	
	return encrypt_recipients_store_type;
}

static void
seahorse_encrypt_recipients_store_class_init (SeahorseEncryptRecipientsStoreClass *klass)
{
	SeahorseKeyStoreClass *skstore_class;
	SeahorseRecipientsStoreClass *recipients_store_class;
	
	parent_class = g_type_class_peek_parent (klass);
	skstore_class = SEAHORSE_KEY_STORE_CLASS (klass);
	recipients_store_class = SEAHORSE_RECIPIENTS_STORE_CLASS (klass);
	
	skstore_class->set = seahorse_encrypt_recipients_store_set;
	skstore_class->changed = seahorse_encrypt_recipients_store_changed;
	
	recipients_store_class->is_recip = seahorse_key_can_encrypt;
}

/* Sets the validity attribute */
static void
seahorse_encrypt_recipients_store_set (GtkTreeStore *store, GtkTreeIter *iter, SeahorseKey *skey)
{
	SeahorseValidity validity;
	
	validity = seahorse_key_get_validity (skey);
	
	gtk_tree_store_set (store, iter,
		VALIDITY_STR, seahorse_validity_get_string (validity),
		VALIDITY, validity, -1);
	SEAHORSE_KEY_STORE_CLASS (parent_class)->set (store, iter, skey);
}

/* Refreshed @skey if trust has changed */
static void
seahorse_encrypt_recipients_store_changed (SeahorseKey *skey, SeahorseKeyChange change,
					   SeahorseKeyStore *skstore, GtkTreeIter *iter)
{
	switch (change) {
		case SKEY_CHANGE_TRUST:
			SEAHORSE_KEY_STORE_GET_CLASS (skstore)->set (
				GTK_TREE_STORE (skstore), iter, skey);
			break;
		default:
			SEAHORSE_KEY_STORE_CLASS (parent_class)->changed (
				skey, change, skstore, iter);
			break;
	}
}

/**
 * seahorse_encrypt_recipients_store_new:
 * @sctx: Current #SeahorseContext
 * @view: #GtkTreeView that will contain the new #SeahorseEncryptRecipientsStore
 *
 * Creates a new #SeahorseEncryptRecipientsStore and embeds it in @view.
 * #SeahorseEncryptRecipientsStore should be used for encrypt recipients.
 * Shown attributes are Name, KeyID, and Validity.
 *
 * Returns: The new #SeahorseKeyStore
 **/
SeahorseKeyStore*
seahorse_encrypt_recipients_store_new (SeahorseContext *sctx, GtkTreeView *view)
{
	SeahorseKeyStore *skstore;
	GtkTreeViewColumn *column;
	
	GType columns[] = {
	        G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT
	};
	
	skstore = g_object_new (SEAHORSE_TYPE_ENCRYPT_RECIPIENTS_STORE, "ctx", sctx, NULL);
	seahorse_key_store_init (skstore, view, COLS, columns);

	column = seahorse_key_store_append_column (view, _("Validity"), VALIDITY_STR);
	gtk_tree_view_column_set_sort_column_id (column, VALIDITY);
	
	return skstore;
}

