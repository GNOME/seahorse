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

#ifndef __SEAHORSE_KEY_H__
#define __SEAHORSE_KEY_H__

/* SeahorseKey is a wrapper for a GpgmeKey that contains some extra data and signals.
 * SeahorseKey is a GtkObject in order to emit the 'destroy' signal, along with the 'changed' signal.
 * Any object or widget that contains a key should connect to these signals. */

#include <gtk/gtk.h>
#include <gpgme.h>

#define SEAHORSE_TYPE_KEY		(seahorse_key_get_type ())
#define SEAHORSE_KEY(obj)		(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_KEY, SeahorseKey))
#define SEAHORSE_KEY_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEY, SeahorseKeyClass))
#define SEAHORSE_IS_KEY(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_KEY))
#define SEAHORSE_IS_KEY_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEY))
#define SEAHORSE_KEY_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_KEY, SeahorseKeyClass))

/* Possible key changes */
typedef enum {
	SKEY_CHANGE_TRUST,
	SKEY_CHANGE_EXPIRE,
	SKEY_CHANGE_DISABLE,
	SKEY_CHANGE_PASS,
	SKEY_CHANGE_UID
} SeahorseKeyChange;

typedef struct _SeahorseKey SeahorseKey;
typedef struct _SeahorseKeyClass SeahorseKeyClass;	
typedef struct _SeahorseKeyPrivate SeahorseKeyPrivate;
	
struct _SeahorseKey
{
	GtkObject		parent;
	
	GpgmeKey		key;
	SeahorseKeyPrivate	*priv;
};

struct _SeahorseKeyClass
{
	GtkObjectClass		parent_class;
	
	/* Signal emitted when one of the key's attributes has changed */
	void 			(* changed)	(SeahorseKey		*skey,
						 SeahorseKeyChange	change);
};

/* Constructs a new SeahorseKey */
SeahorseKey*	seahorse_key_new		(GpgmeKey		key);

/* Emits the 'destroy' signal. */
void		seahorse_key_destroy		(SeahorseKey		*skey);

/* Called by key edit operations.  Emits the 'changed' signal. */
void		seahorse_key_changed		(SeahorseKey		*skey,
						 SeahorseKeyChange	change);

/* Returns the number of user ids */
const gint	seahorse_key_get_num_uids	(const SeahorseKey	*skey);

/* Returns the number of sub keys */
const gint	seahorse_key_get_num_subkeys	(const SeahorseKey	*skey);

/* Returns the last 8 characters of the keyid */
const gchar*	seahorse_key_get_keyid		(const SeahorseKey	*skey,
						 const guint		index);

/* Returns the formatted, utf8 valid, userid at index in key */
const gchar*	seahorse_key_get_userid		(const SeahorseKey	*skey,
						 const guint		index);

#endif /* __SEAHORSE_KEY_H__ */
