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

#ifndef __SEAHORSE_PASSPHRASE_H__
#define __SEAHORSE_PASSPHRASE_H__

/* Passphrase callback dialog for gpgme */

#include <glib.h>

#include "seahorse-context.h"
#include "seahorse-key.h"

/* Returns the entered passphrase, or NULL */
const gchar*	seahorse_passphrase_get	(gpointer		ctx,
					 const gchar		*desc,
					 gpointer		*r_hd);

#endif /* __SEAHORSE_PASSPHRASE_H__ */
