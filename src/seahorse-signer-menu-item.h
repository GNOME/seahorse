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

#ifndef __SEAHORSE_SIGNER_MENU_ITEM_H__
#define __SEAHORSE_SIGNER_MENU_ITEM_H__

#include <gtk/gtk.h>

#include "seahorse-key.h"

#define SEAHORSE_TYPE_SIGNER_MENU_ITEM			(seahorse_signer_menu_item_get_type ())
#define SEAHORSE_SIGNER_MENU_ITEM(obj)			(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_SIGNER_MENU_ITEM, SeahorseSignerMenuItem))
#define SEAHORSE_SIGNER_MENU_ITEM_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SIGNER_MENU_ITEM, SeahorseSignerMenuItemClass))
#define SEAHORSE_IS_SIGNER_MENU_ITEM(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_SIGNER_MENU_ITEM))
#define SEAHORSE_IS_SIGNER_MENU_ITEM_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SIGNER_MENU_ITEM))
#define SEAHORSE_SIGNER_MENU_ITEM_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_SIGNER_MENU_ITEM, SeahorseSignerMenuItemClass))

typedef struct _SeahorseSignerMenuItem SeahorseSignerMenuItem;
typedef struct _SeahorseSignerMenuItemClass SeahorseSignerMenuItemClass;

struct _SeahorseSignerMenuItem
{
	GtkMenuItem		parent;
	
	SeahorseKey		*skey;
};

struct _SeahorseSignerMenuItemClass
{
	GtkMenuItemClass	parent_class;
};

GtkWidget*	seahorse_signer_menu_item_new	(SeahorseKey	*skey);

#endif /* __SEAHORSE_SIGNER_MENU_ITEM_H__ */
