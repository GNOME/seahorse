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

#ifndef __SEAHORSE_KEY_MENU_ITEM_H__
#define __SEAHORSE_KEY_MENU_ITEM_H__

/* SeahorseKeyMenuItem is a GtkMenuItem that contains a SeahorseKey.
 * It connects to any relevant key signals. */

#include <gtk/gtk.h>

#include "seahorse-key.h"

#define SEAHORSE_TYPE_KEY_MENU_ITEM		(seahorse_key_menu_item_get_type ())
#define SEAHORSE_KEY_MENU_ITEM(obj)		(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_KEY_MENU_ITEM, SeahorseKeyMenuItem))
#define SEAHORSE_KEY_MENU_ITEM_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEY_MENU_ITEM, SeahorseKeyMenuItemClass))
#define SEAHORSE_IS_KEY_MENU_ITEM(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_KEY_MENU_ITEM))
#define SEAHORSE_IS_KEY_MENU_ITEM_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEY_MENU_ITEM))
#define SEAHORSE_KEY_MENU_ITEM_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_KEY_MENU_ITEM, SeahorseKeyMenuItemClass))

typedef struct _SeahorseKeyMenuItem SeahorseKeyMenuItem;
typedef struct _SeahorseKeyMenuItemClass SeahorseKeyMenuItemClass;

struct _SeahorseKeyMenuItem
{
	GtkMenuItem		parent;
	
	SeahorseKey		*skey;
};

struct _SeahorseKeyMenuItemClass
{
	GtkMenuItemClass	parent_class;
};

/* Creates a new key menu item */
GtkWidget*	seahorse_key_menu_item_new (SeahorseKey *skey);

#endif /* __SEAHORSE_KEY_MENU_H__ */
