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
 
#ifndef __SEAHORSE_SIGNER_MENU_H__
#define __SEAHORSE_SIGNER_MENU_H__
 
#include <gtk/gtk.h>
 
#include "seahorse-context.h"
 
#define SEAHORSE_TYPE_SIGNER_MENU		(seahorse_signer_menu_get_type ())
#define SEAHORSE_SIGNER_MENU(obj)		(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_SIGNER_MENU, SeahorseSignerMenu))
#define SEAHORSE_SIGNER_MENU_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SIGNER_MENU, SeahorseSignerMenuClass))
#define SEAHORSE_IS_SIGNER_MENU(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_SIGNER_MENU))
#define SEAHORSE_IS_SIGNER_MENU_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SIGNER_MENU))
#define SEAHORSE_SIGNER_MENU_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_SIGNER_MENU, SeahorseSignerMenuClass))

typedef struct _SeahorseSignerMenu SeahorseSignerMenu;
typedef struct _SeahorseSignerMenuClass SeahorseSignerMenuClass;

struct _SeahorseSignerMenu
{
	GtkMenu		parent;
	
	SeahorseContext	*sctx;
};

struct _SeahorseSignerMenuClass
{
	GtkMenuClass	parent_class;
};

void	seahorse_signer_menu_new	(SeahorseContext	*sctx,
					 GtkOptionMenu		*optionmenu);
 
#endif /* __SEAHORSE_SIGNER_MENU_H__ */
 
