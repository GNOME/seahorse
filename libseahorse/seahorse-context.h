/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Nate Nielsen
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

#ifndef __SEAHORSE_CONTEXT_H__
#define __SEAHORSE_CONTEXT_H__

#include <gtk/gtk.h>
#include <gpgme.h>

#include "seahorse-key.h"
#include "seahorse-key-pair.h"
#include "seahorse-key-source.h"

#define PGP_SCHEMAS "/desktop/pgp"
#define ARMOR_KEY PGP_SCHEMAS "/ascii_armor"
#define TEXTMODE_KEY PGP_SCHEMAS "/text_mode"
#define DEFAULT_KEY PGP_SCHEMAS "/default_key"
#define ENCRYPTSELF_KEY PGP_SCHEMAS "/encrypt_to_self"
#define LASTSIGNER_KEY PGP_SCHEMAS "/last_signer"
#define MULTI_EXTENSION_KEY PGP_SCHEMAS "/package_extension"
#define MULTI_SEPERATE_KEY PGP_SCHEMAS "/multi_seperate"
#define KEYSERVER_KEY PGP_SCHEMAS "/keyservers/all_keyservers"
#define LASTSERVER_KEY PGP_SCHEMAS "/keyservers/keyserver"
#define LASTSEARCH_KEY PGP_SCHEMAS "/keyservers/search_text"

#define SEAHORSE_SCHEMAS "/apps/seahorse"

#define SEAHORSE_TYPE_CONTEXT			(seahorse_context_get_type ())
#define SEAHORSE_CONTEXT(obj)			(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_CONTEXT, SeahorseContext))
#define SEAHORSE_CONTEXT_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_CONTEXT, SeahorseContextClass))
#define SEAHORSE_IS_CONTEXT(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_CONTEXT))
#define SEAHORSE_IS_CONTEXT_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_CONTEXT))
#define SEAHORSE_CONTEXT_GET_CLASS(obj)		(GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_CONTEXT, SeahorseContextClass))

typedef struct _SeahorseContext SeahorseContext;
typedef struct _SeahorseContextClass SeahorseContextClass;

typedef struct _SeahorseContextPrivate SeahorseContextPrivate;
	
struct _SeahorseContext
{
	GtkObject		parent;
	
	/*< private >*/
	SeahorseContextPrivate	*priv;
};

struct _SeahorseContextClass
{
	GtkObjectClass		parent_class;
};

GType                seahorse_context_get_type        (void);

SeahorseContext*     seahorse_context_new             (void);

void                 seahorse_context_destroy         (SeahorseContext    *sctx);

void                 seahorse_context_load_keys       (SeahorseContext    *sctx, 
                                                       gboolean secret_only);

SeahorseKeyPair*     seahorse_context_get_default_key (SeahorseContext    *sctx);

SeahorseKeySource*   seahorse_context_get_key_source  (SeahorseContext    *sctx);

void                 seahorse_context_own_source      (SeahorseContext    *sctx,
                                                       SeahorseKeySource  *sksrc);

#endif /* __SEAHORSE_CONTEXT_H__ */
