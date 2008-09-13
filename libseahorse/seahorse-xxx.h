/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
 * 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *  
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#ifndef __SEAHORSE_XXX_H__
#define __SEAHORSE_XXX_H__

#include <glib-object.h>

#define SEAHORSE_TYPE_XXX               (seahorse_xxx_get_type ())
#define SEAHORSE_XXX(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_XXX, SeahorseXxx))
#define SEAHORSE_XXX_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_XXX, SeahorseXxxClass))
#define SEAHORSE_IS_XXX(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_XXX))
#define SEAHORSE_IS_XXX_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_XXX))
#define SEAHORSE_XXX_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_XXX, SeahorseXxxClass))

typedef struct _SeahorseXxx SeahorseXxx;
typedef struct _SeahorseXxxClass SeahorseXxxClass;
typedef struct _SeahorseXxxPrivate SeahorseXxxPrivate;
    
struct _SeahorseXxx {
	GObject parent;
};

struct _SeahorseXxxClass {
	GObjectClass parent_class;
    
	/* signals --------------------------------------------------------- */
    
	void (*signal) (SeahorseXxx *xxx);
};

GType               seahorse_xxx_get_type               (void);

SeahorseXxx*        seahorse_xxx_new                    (void);

#endif /* __SEAHORSE_XXX_H__ */
