/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 Christian Persch
 *  Copyright (C) 2006 Jean-François Rameau
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef SEAHORSE_EXTENSION_H
#define SEAHORSE_EXTENSION_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define TYPE_SEAHORSE_EXTENSION		(seahorse_extension_get_type ())
#define SEAHORSE_EXTENSION(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_SEAHORSE_EXTENSION, SeahorseExtension))
#define SEAHORSE_EXTENSION_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), TYPE_SEAHORSE_EXTENSION, SeahorseExtensionClass))
#define IS_SEAHORSE_EXTENSION(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), TYPE_SEAHORSE_EXTENSION))
#define IS_SEAHORSE_EXTENSION_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), TYPE_SEAHORSE_EXTENSION))
#define SEAHORSE_EXTENSION_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_SEAHORSE_EXTENSION, SeahorseExtensionClass))

typedef struct SeahorseExtension	SeahorseExtension;
typedef struct SeahorseExtensionClass	SeahorseExtensionClass;
typedef struct SeahorseExtensionPrivate	SeahorseExtensionPrivate;

struct SeahorseExtensionClass
{
	GObjectClass parent_class;
};

struct SeahorseExtension
{
	GObject parent_instance;

	/*< private >*/
	SeahorseExtensionPrivate *priv;
};

GType	seahorse_extension_get_type	(void);

GType	seahorse_extension_register_type (GTypeModule *module);

G_END_DECLS

#endif
