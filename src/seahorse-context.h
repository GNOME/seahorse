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
	
	/*< public >*/
	GpgmeCtx		ctx;
	
	/*< private >*/
	SeahorseContextPrivate	*priv;
};

struct _SeahorseContextClass
{
	GtkObjectClass		parent_class;
	
	/* Signal emitted when @skey has been added to @sctx */
	void 			(* add)				(SeahorseContext	*sctx,
								 SeahorseKey		*skey);
	
	void			(* progress)			(SeahorseContext	*sctx,
								 const gchar		*op,
								 gdouble		fract);
};

SeahorseContext*	seahorse_context_new			(void);

void			seahorse_context_destroy		(SeahorseContext	*sctx);

GList*			seahorse_context_get_keys		(SeahorseContext	*sctx);

GList*			seahorse_context_get_key_pairs		(SeahorseContext	*sctx);

void			seahorse_context_show_status		(SeahorseContext	*sctx,
								 const gchar		*op,
								 gboolean		success);

void			seahorse_context_show_progress		(SeahorseContext	*sctx,
								 const gchar		*op,
								 gdouble		fract);

void			seahorse_context_key_added		(SeahorseContext	*sctx);

SeahorseKey*		seahorse_context_get_key		(SeahorseContext	*sctx,
								 GpgmeKey		key);

gboolean		seahorse_context_key_has_secret		(SeahorseContext	*sctx,
								 SeahorseKey		*skey);

void			seahorse_context_set_ascii_armor	(SeahorseContext	*sctx,
								 gboolean		ascii_armor);

void			seahorse_context_set_text_mode		(SeahorseContext	*sctx,
								 gboolean		text_mode);

void			seahorse_context_set_signer		(SeahorseContext	*sctx,
								 SeahorseKey		*signer);

SeahorseKey*		seahorse_context_get_last_signer	(SeahorseContext	*sctx);

#endif /* __SEAHORSE_CONTEXT_H__ */
