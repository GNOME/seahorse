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

#ifndef __SEAHORSE_KEY_MANAGER_STORE_H__
#define __SEAHORSE_KEY_MANAGER_STORE_H__

/* SeahorseKeyManagerStore is a SeahorseKeyStore.
 * It shows key manager specific key attributes. */

#include "seahorse-key-store.h"

#define SEAHORSE_TYPE_KEY_MANAGER_STORE			(seahorse_key_manager_store_get_type ())
#define SEAHORSE_KEY_MANAGER_STORE(obj)			(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_KEY_MANAGER_STORE, SeahorseKeyManagerStore))
#define SEAHORSE_KEY_MANAGER_STORE_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEY_MANAGER_STORE, SeahorseKeyManagerStoreClass))
#define SEAHORSE_IS_KEY_MANAGER_STORE(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_KEY_MANAGER_STORE))
#define SEAHORSE_IS_KEY_MANAGER_STORE_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEY_MANAGER_STORE))
#define SEAHORSE_KEY_MANAGER_STORE_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_KEY_MANAGER_STORE, SeahorseKeyManagerStoreClass))

typedef struct _SeahorseKeyManagerStore SeahorseKeyManagerStore;
typedef struct _SeahorseKeyManagerStoreClass SeahorseKeyManagerStoreClass;
	
struct _SeahorseKeyManagerStore
{
	SeahorseKeyStore	parent;
};

struct _SeahorseKeyManagerStoreClass
{
	SeahorseKeyStoreClass	parent_class;
};

/* Create a new key manager store in the tree view */
SeahorseKeyStore*	seahorse_key_manager_store_new	(SeahorseContext	*sctx,
							 GtkTreeView		*view);

#endif /* __SEAHORSE_KEY_MANAGER_STORE_H__ */
