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

#ifndef __SEAHORSE_VALIDITY_H__
#define __SEAHORSE_VALIDITY_H__

#include <gtk/gtk.h>

typedef enum {
	SEAHORSE_VALIDITY_UNKNOWN = 1,
	SEAHORSE_VALIDITY_NEVER = 2,
	SEAHORSE_VALIDITY_MARGINAL = 3,
	SEAHORSE_VALIDITY_FULL = 4,
	SEAHORSE_VALIDITY_ULTIMATE = 5
} SeahorseValidity;

const gchar*	seahorse_validity_get_string	(SeahorseValidity	validity);

gint		seahorse_validity_compare	(GtkTreeModel		*model,
						 GtkTreeIter		*a,
						 GtkTreeIter		*b,
						 guint			index);

#endif /* __SEAHORSE_VALIDITY_H__ */
