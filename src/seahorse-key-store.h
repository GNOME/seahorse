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

#ifndef __SEAHORSE_KEY_STORE_H__
#define __SEAHORSE_KEY_STORE_H__

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
	
struct _SeahorseKeyStore
{
	GtkTreeStore		parent;
	
	SeahorseContext		*sctx;
};

struct _SeahorseKeyStoreClass
{
	GtkTreeStoreClass	parent_class;
	
	/* Virtual method for appending a key. When subclass method finishes,
	 * iter should be set to the parent of the new row.
	 * This is needed since not every key should always be added and
	 * some stores will display subkeys.
	 */
	void			(* append)		(SeahorseKeyStore	*skstore,
							 SeahorseKey		*skey,
							 GtkTreeIter		*iter);
	
	/* Virtual method for setting the key's attributes. Name and KeyID are
	 * set by the key-store, so implementation in subclasses is optional.
	 */
	void			(* set)			(GtkTreeStore		*store,
							 GtkTreeIter		*iter,
							 SeahorseKey		*skey);
	
	/* Optional virtual method for removing a row. Only implement if iter
	 * is a parent and has sub-iters that also need to be removed.
	 */
	void			(* remove)		(SeahorseKeyStore	*skstore,
							 GtkTreeIter		*iter);
	
	/* Virtual method for when the key at iter has changed. Key-store
	 * will already do user ID changes, so implementation is optional
	 * depending on displayed attributes.
	 */
	void			(* changed)		(SeahorseKey		*skey,
							 SeahorseKeyChange	change,
							 SeahorseKeyStore	*skstore,
							 GtkTreeIter		*iter);
};

void		seahorse_key_store_init			(SeahorseKeyStore	*skstore,
							 GtkTreeView		*view,
							 gint			cols,
							 GType			*ccolumns);

void		seahorse_key_store_destroy		(SeahorseKeyStore	*skstore);

void		seahorse_key_store_populate		(SeahorseKeyStore	*skstore);

GtkTreePath*	seahorse_key_store_get_selected_path	(GtkTreeView		*view);

SeahorseKey*	seahorse_key_store_get_selected_key	(GtkTreeView		*view);

SeahorseKey*	seahorse_key_store_get_key_from_path	(GtkTreeView		*view,
							 GtkTreePath		*path);

void		seahorse_key_store_append		(SeahorseKeyStore	*skstore,
							 SeahorseKey		*skey);

void		seahorse_key_store_remove		(SeahorseKeyStore	*skstore,
							 GtkTreePath		*path);

void		seahorse_key_store_append_column	(GtkTreeView		*view,
							 const gchar		*name,
							 const gint		index);

#endif /* __SEAHORSE_KEY_STORE_H__ */
