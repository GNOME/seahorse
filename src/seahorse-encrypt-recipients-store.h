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

#ifndef __SEAHORSE_ENCRYPT_RECIPIENTS_STORE_H__
#define __SEAHORSE_ENCRYPT_RECIPIENTS_STORE_H__

#include "seahorse-recipients-store.h"

#define SEAHORSE_TYPE_ENCRYPT_RECIPIENTS_STORE			(seahorse_encrypt_recipients_store_get_type ())
#define SEAHORSE_ENCRYPT_RECIPIENTS_STORE(obj)			(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_ENCRYPT_RECIPIENTS_STORE, SeahorseEncryptRecipientsStore))
#define SEAHORSE_ENCRYPT_RECIPIENTS_STORE_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_ENCRYPT_RECIPIENTS_STORE, SeahorseEncryptRecipientsStoreClass))
#define SEAHORSE_IS_ENCRYPT_RECIPIENTS_STORE(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_ENCRYPT_RECIPIENTS_STORE))
#define SEAHORSE_IS_ENCRYPT_RECIPIENTS_STORE_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_ENCRYPT_RECIPIENTS_STORE))
#define SEAHORSE_ENCRYPT_RECIPIENTS_STORE_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_ENCRYPT_RECIPIENTS_STORE, SeahorseEncryptRecipientsStoreClass))

typedef struct _SeahorseEncryptRecipientsStore SeahorseEncryptRecipientsStore;
typedef struct _SeahorseEncryptRecipientsStoreClass SeahorseEncryptRecipientsStoreClass;
	
struct _SeahorseEncryptRecipientsStore
{
	SeahorseRecipientsStore		parent;
};

struct _SeahorseEncryptRecipientsStoreClass
{
	SeahorseRecipientsStoreClass	parent_class;
};

SeahorseKeyStore*	seahorse_encrypt_recipients_store_new	(SeahorseContext	*sctx,
								 GtkTreeView		*view);

#endif /* __SEAHORSE_RECIPIENTS_STORE_H__ */
