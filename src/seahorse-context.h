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

#ifndef __SEAHORSE_CONTEXT_H__
#define __SEAHORSE_CONTEXT_H__

/* SeahorseContext is a wrapper for a GpgmeContext.  It caches the key ring as a list
 * and has signals for showing an operation's status and for when a key has been added. */

#include <gtk/gtk.h>
#include <gpgme.h>

#include "seahorse-key.h"

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
	
	GpgmeCtx		ctx;
	SeahorseContextPrivate	*priv;
};

struct _SeahorseContextClass
{
	GtkObjectClass		parent_class;
	
	/* Operation status signal */
	void 			(* status)			(const SeahorseContext	*sctx,
								 const gchar		*op);
	
	/* Key has been added to key ring signal */
	void 			(* add)				(SeahorseContext	*sctx,
								 SeahorseKey		*skey);
};

/* Creates a new context */
SeahorseContext*	seahorse_context_new			(void);

/* Emits the destroy signal for context */
void			seahorse_context_destroy		(SeahorseContext	*sctx);

/* Returns the cached key ring */
GList*			seahorse_context_get_keys		(const SeahorseContext	*sctx);

/* Sends the context an operation status message.  Emits the 'status' signal. */
void			seahorse_context_show_status		(const SeahorseContext	*sctx,
								 const gchar		*op,
								 gboolean		success);

/* Tells the context keys might have been added.  Emits the 'add' signal. */
void			seahorse_context_key_added		(SeahorseContext	*sctx);

/* Returns the SeahorseKey corresponding to the GpgmeKey, or NULL if not in key ring. */
SeahorseKey*		seahorse_context_get_key		(const SeahorseContext	*sctx,
								 GpgmeKey		key);

/* Returns TRUE if the key has a secret key */
gboolean		seahorse_context_key_has_secret		(SeahorseContext	*sctx,
								 SeahorseKey		*skey);

/* Sets ascii armor for context */
void			seahorse_context_set_ascii_armor	(SeahorseContext	*sctx,
								 gboolean		ascii_armor);

/* Sets text mode for context */
void			seahorse_context_set_text_mode		(SeahorseContext	*sctx,
								 gboolean		text_mode);

/* Sets default signing key for context */
void			seahorse_context_set_signer		(SeahorseContext	*sctx,
								 SeahorseKey		*signer);

/* Returns default signing key for the context */
SeahorseKey*		seahorse_context_get_last_signer	(const SeahorseContext	*sctx);

#endif /* __SEAHORSE_CONTEXT_H__ */
