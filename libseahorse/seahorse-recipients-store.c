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

#include "seahorse-context.h"
#include "seahorse-recipients-store.h"

#define RECIPIENTS_SORT_KEY PGP_SCHEMAS "/recipients/sort_by"

/* Columns in the tree view */
enum {
    KEY_STORE_BASE_COLUMNS,
	VALIDITY_STR,
	VALIDITY,
	COLS
};

static const gchar* col_ids[] = {
    KEY_STORE_BASE_IDS,
    "validity",
    "validity"
};

static const GType col_types[] = {
    KEY_STORE_BASE_TYPES, 
    G_TYPE_STRING, 
    G_TYPE_INT
};

static void	seahorse_recipients_store_class_init	(SeahorseRecipientsStoreClass	*klass);

static gboolean	seahorse_recipients_store_append	(SeahorseKeyStore		*skstore,
                                                     SeahorseKey			*skey,
                                                     guint                  uid,
                                                     GtkTreeIter			*iter);
static void	    seahorse_recipients_store_set		(SeahorseKeyStore		*store,
                                                     SeahorseKey            *skey,
                                                     guint                  uid,
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
	skstore_class->set = seahorse_recipients_store_set;
   
   	/* Class behavior and columns */
    skstore_class->use_check = TRUE;
    skstore_class->n_columns = COLS;
    skstore_class->col_ids = col_ids;
    skstore_class->col_types = col_types;
    skstore_class->gconf_sort_key = RECIPIENTS_SORT_KEY;
}

/* Checks if @skey is a valid recipient before appending */
static gboolean
seahorse_recipients_store_append (SeahorseKeyStore *skstore, SeahorseKey *skey, 
                                  guint uid, GtkTreeIter *iter)
{
	if (seahorse_key_can_encrypt (skey)) {
        gtk_tree_store_append (GTK_TREE_STORE (skstore), iter, NULL);
        parent_class->append (skstore, skey, uid, iter);
        return TRUE;
	}
 
    return FALSE;
}

/* Sets the validity attribute */
static void
seahorse_recipients_store_set (SeahorseKeyStore *skstore, SeahorseKey *skey, 
                               guint uid, GtkTreeIter *iter)
{
	SeahorseValidity validity;
	
	validity = seahorse_key_get_validity (skey);
	
	gtk_tree_store_set (GTK_TREE_STORE (skstore), iter,
		VALIDITY_STR, seahorse_validity_get_string (validity),
		VALIDITY, validity, -1);
	SEAHORSE_KEY_STORE_CLASS (parent_class)->set (skstore, skey, uid, iter);
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
seahorse_recipients_store_new (SeahorseKeySource *sksrc, GtkTreeView *view)
{
	SeahorseKeyStore *skstore;
	GtkTreeViewColumn *column;
	
	skstore = g_object_new (SEAHORSE_TYPE_RECIPIENTS_STORE, "key-source", sksrc, NULL);
	seahorse_key_store_init (skstore, view);
	
	column = seahorse_key_store_append_column (view, _("Validity"), VALIDITY_STR);
	gtk_tree_view_column_set_sort_column_id (column, VALIDITY);

	return skstore;
}
