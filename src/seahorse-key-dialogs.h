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

#ifndef __SEAHORSE_KEY_DIALOGS__
#define __SEAHORSE_KEY_DIALOGS__

#include <gtk/gtk.h>

#include "seahorse-context.h"

void		seahorse_key_properties_new	(SeahorseContext	*sctx,
						 SeahorseKey		*skey);

void		seahorse_add_revoker_new	(SeahorseContext	*sctx,
						 SeahorseKey		*skey);

void		seahorse_add_subkey_new		(SeahorseContext	*sctx,
						 SeahorseKey		*skey);

void		seahorse_add_uid_new		(SeahorseContext	*sctx,
						 SeahorseKey		*skey);

void		seahorse_delete_subkey_new	(SeahorseContext	*sctx,
						 SeahorseKey		*skey,
						 const guint		index);

void		seahorse_revoke_new		(SeahorseContext	*sctx,
						 SeahorseKey		*skey,
						 const guint		index);

const gchar*	seahorse_change_passphrase_get	(SeahorseContext	*sctx,
						 const gchar		*desc,
						 gpointer		*data);

#endif /* __SEAHORSE_KEY_DIALOGS__ */
