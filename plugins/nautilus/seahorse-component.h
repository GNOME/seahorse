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

#ifndef __SEAHORSE_COMPONENT_H__
#define __SEAHORSE_COMPONENT_H__

#include <bonobo/bonobo-object.h>

#define SEAHORSE_TYPE_COMPONENT			(seahorse_component_get_type ())
#define SEAHORSE_COMPONENT(obj)			(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_COMPONENT, SeahorseComponent))
#define SEAHORSE_COMPONENT_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_COMPONENT, SeahorseComponentClass))
#define SEAHORSE_IS_COMPONENT(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_COMPONENT))
#define SEAHORSE_IS_COMPONENT_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_COMPONENT))

typedef struct {
	BonoboObject parent;
} SeahorseComponent;

typedef struct {
	BonoboObjectClass parent;

	POA_Bonobo_Listener__epv epv;
} SeahorseComponentClass;

GType	seahorse_component_get_type	(void);

#endif /* __SEAHORSE_COMPONENT_H__ */
