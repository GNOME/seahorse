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

#ifndef __SEAHORSE_FILE_DIALOGS_H__
#define __SEAHORSE_FILE_DIALOGS_H__

#include <gtk/gtk.h>

#include "seahorse-context.h"

void		seahorse_encrypt_file_new		(SeahorseContext	*sctx);

gboolean	seahorse_encrypt_file			(SeahorseContext	*sctx,
							 const gchar		*file);

void		seahorse_sign_file_new			(SeahorseContext	*sctx);

gboolean	seahorse_sign_file			(SeahorseContext	*sctx,
							 const gchar		*file);

void		seahorse_encrypt_sign_file_new		(SeahorseContext	*sctx);

gboolean	seahorse_encrypt_sign_file		(SeahorseContext	*sctx,
							 const gchar		*file);

void		seahorse_decrypt_file_new		(SeahorseContext	*sctx);

gboolean	seahorse_decrypt_file			(SeahorseContext	*sctx,
							 const gchar		*file);

void		seahorse_verify_file_new		(SeahorseContext	*sctx);

gboolean	seahorse_verify_file			(SeahorseContext	*sctx,
							 const gchar		*file);

void		seahorse_decrypt_verify_file_new	(SeahorseContext	*sctx);

gboolean	seahorse_decrypt_verify_file		(SeahorseContext	*sctx,
							 const gchar		*file);

#endif /* __SEAHORSE_FILE_DIALOGS_H__ */
