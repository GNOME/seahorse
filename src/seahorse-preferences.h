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

#ifndef __SEAHORSE_PREFERENCES_H__
#define __SEAHORSE_PREFERENCES_H__

#include "seahorse-context.h"

#define LISTING_SCHEMAS SEAHORSE_SCHEMAS "/listing"
#define SHOW_VALIDITY_KEY LISTING_SCHEMAS "/show_validity"
#define SHOW_EXPIRES_KEY LISTING_SCHEMAS "/show_expires"
#define SHOW_TRUST_KEY LISTING_SCHEMAS "/show_trust"
#define SHOW_LENGTH_KEY LISTING_SCHEMAS "/show_length"
#define SHOW_TYPE_KEY LISTING_SCHEMAS "/show_type"

#define UI_SCHEMAS SEAHORSE_SCHEMAS "/ui"

void		seahorse_preferences_show		(SeahorseContext	*sctx,
                                             guint              tab);

#endif /* __SEAHORSE_PREFERENCES_H__ */
