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

#ifndef __SEAHORSE_KEY_PAIR_H__
#define __SEAHORSE_KEY_PAIR_H__

#include "seahorse-key.h"

#define SEAHORSE_TYPE_KEY_PAIR			(seahorse_key_pair_get_type ())
#define SEAHORSE_KEY_PAIR(obj)			(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_KEY_PAIR, SeahorseKeyPair))
#define SEAHORSE_KEY_PAIR_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEY_PAIR, SeahorseKeyPairClass))
#define SEAHORSE_IS_KEY_PAIR(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_KEY_PAIR))
#define SEAHORSE_IS_KEY_PAIR_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEY_PAIR))
#define SEAHORSE_KEY_PAIR_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_KEY_PAIR, SeahorseKeyPairClass))

typedef struct _SeahorseKeyPair SeahorseKeyPair;
typedef struct _SeahorseKeyPairClass SeahorseKeyPairClass;

struct _SeahorseKeyPair
{
	SeahorseKey		parent;
	
	GpgmeKey		secret;
};

struct _SeahorseKeyPairClass
{
	SeahorseKeyClass	parent_class;
};

SeahorseKey*	seahorse_key_pair_new		(GpgmeKey		key,
						 GpgmeKey		secret);

gboolean	seahorse_key_pair_can_sign	(const SeahorseKeyPair	*skpair);

#endif /* __SEAHORSE_KEY_PAIR_H__ */
