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

#ifndef __SEAHORSE_VALIDITY_H__
#define __SEAHORSE_VALIDITY_H__

/* These are common functions for working with a GpgmeValidity.
 * They ignore GPGME_VALIDITY_UNDEFINED */

#include <gtk/gtk.h>
#include <gpgme.h>

#include "seahorse-key.h"

/* Gets a string representation of the given validity */
gchar*		seahorse_validity_get_string		(GpgmeValidity		validity);

/* Gets a string of the key's validity */
gchar*		seahorse_validity_get_validity_from_key	(const SeahorseKey	*skey);

/* Gets a string of the key's owner trust */
gchar*		seahorse_validity_get_trust_from_key	(const SeahorseKey	*skey);

/* Loads the option menu with menu items representing the available validities */
void		seahorse_validity_load_menu		(GtkOptionMenu		*option);

/* Gets the index of validity, ignoring UNDEFINED */
guint		seahorse_validity_get_index		(GpgmeValidity		validity);

/* Gets the validity given an index */
GpgmeValidity	seahorse_validity_get_from_index	(guint			index);

#endif /* __SEAHORSE_VALIDITY_H__ */
