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

#define SCHEMA_ROOT "/apps/seahorse"

#define KEY_UI SCHEMA_ROOT "/ui"
#define KEY_TOOLBAR_STYLE KEY_UI "/toolbar_style"

#define TOOLBAR_DEFAULT "default"
#define TOOLBAR_BOTH "both"
#define TOOLBAR_BOTH_HORIZ "both_horiz"
#define TOOLBAR_ICONS "icons"
#define TOOLBAR_TEXT "text"

#define LISTING SCHEMA_ROOT "/listing"
#define SHOW_TRUST LISTING "/show_trust"
#define SHOW_TYPE LISTING "/show_type"
#define SHOW_LENGTH LISTING "/show_length"

void		seahorse_preferences_show		(SeahorseContext	*sctx);

#endif /* __SEAHORSE_PREFERENCES_H__ */
