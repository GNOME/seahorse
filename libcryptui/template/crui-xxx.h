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

#ifndef __CRUI_XXX_H__
#define __CRUI_XXX_H__

#include <glib-object.h>

#define CRUI_TYPE_XXX               (crui_xxx_get_type ())
#define CRUI_XXX(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CRUI_TYPE_XXX, CruiXxx))
#define CRUI_XXX_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CRUI_TYPE_XXX, CruiXxxClass))
#define CRUI_IS_XXX(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CRUI_TYPE_XXX))
#define CRUI_IS_XXX_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CRUI_TYPE_XXX))
#define CRUI_XXX_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CRUI_TYPE_XXX, CruiXxxClass))

typedef struct _CruiXxx CruiXxx;
typedef struct _CruiXxxClass CruiXxxClass;
typedef struct _CruiXxxPrivate CruiXxxPrivate;
    
struct _CruiXxx {
	GObject parent;
};

struct _CruiXxxClass {
	GObjectClass parent_class;
    
	/* signals --------------------------------------------------------- */
    
	void (*signal) (CruiXxx *xxx);
};

GType               crui_xxx_get_type               (void);

CruiXxx*            crui_xxx_new                    (void);

#endif /* __CRUI_XXX_H__ */
