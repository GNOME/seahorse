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

#ifndef __SEAHORSE_KEY_STORE_H__
#define __SEAHORSE_KEY_STORE_H__

/* SeahorseKeyStore is a GtkTreeStore.  It is an abstract class for storing and managing keys.
 * It does not deal with setting attributes or key changes.  It does remove keys when destroyed
 * and can return selected keys and/or locations.  By default, the store starts empty.
 * Key rows are assumed to be stored in column 0. */

#include <gtk/gtk.h>

#include "seahorse-context.h"

#define SEAHORSE_TYPE_KEY_STORE			(seahorse_key_store_get_type ())
#define SEAHORSE_KEY_STORE(obj)			(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_KEY_STORE, SeahorseKeyStore))
#define SEAHORSE_KEY_STORE_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEY_STORE, SeahorseKeyStoreClass))
#define SEAHORSE_IS_KEY_STORE(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_KEY_STORE))
#define SEAHORSE_IS_KEY_STORE_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEY_STORE))
#define SEAHORSE_KEY_STORE_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_KEY_STORE, SeahorseKeyStoreClass))

typedef struct _SeahorseKeyStore SeahorseKeyStore;
typedef struct _SeahorseKeyStoreClass SeahorseKeyStoreClass;
typedef struct _SeahorseKeyRow SeahorseKeyRow;
	
struct _SeahorseKeyStore
{
	GtkTreeStore		parent;
	
	SeahorseContext		*sctx;
};

struct _SeahorseKeyStoreClass
{
	GtkTreeStoreClass	parent_class;
	
	/* Virtual method for setting row attributes, starting at parent iterator */
	void			(*set)				(SeahorseKeyRow		*skrow,
								 GtkTreeIter		*parent);
	/* Virtual method for when a key has changed */
	void			(*changed)			(SeahorseKey		*skey,
								 SeahorseKeyChange	change,
								 SeahorseKeyRow		*skrow);
};

/* SeahorseKeyRow is a reference to a row in a tree store that contains a key. It is designed to be
 * used as a single object to be passed around by methods in SeahorseKeyStore and its subclasses.
 * It is not opaque so that sub classes can use it to get the key or store. */
struct _SeahorseKeyRow
{
	GtkTreeStore		*store;
	GtkTreeRowReference	*ref;
	SeahorseKey		*skey;
};

/* Transfer key row from current store to given key store, returning the new row */
SeahorseKeyRow*		seahorse_key_row_transfer		(SeahorseKeyRow		*skrow,
								 SeahorseKeyStore	*skstore);

/* Removes all key rows from store */
void			seahorse_key_store_destroy		(SeahorseKeyStore	*skstore);

/* Populates key store with current key ring */
void			seahorse_key_store_populate		(SeahorseKeyStore	*skstore);

/* Sugar method for getting the selected key */
SeahorseKey*		seahorse_key_store_get_selected_key	(GtkTreeView		*view);

/* Sugar method for getting the key at path */
SeahorseKey*		seahorse_key_store_get_key_from_path	(GtkTreeView		*view,
								 GtkTreePath		*path);

/* Sugar method for getting the selected key row */
SeahorseKeyRow*		seahorse_key_store_get_selected_row	(GtkTreeView		*view);

/* Returns the key row at path */
SeahorseKeyRow*		seahorse_key_store_get_row_from_path	(GtkTreeView		*view,
								 GtkTreePath		*path);

#endif /* __SEAHORSE_KEY_STORE_H__ */
