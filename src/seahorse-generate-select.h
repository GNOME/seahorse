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

#ifndef __SEAHORSE_GENERATE_SELECT_H__
#define __SEAHORSE_GENERATE_SELECT_H__

#include <glib.h>
#include <glib-object.h>

#include "seahorse-widget.h"

#define SEAHORSE_TYPE_GENERATE_SELECT             (seahorse_generate_select_get_type ())
#define SEAHORSE_GENERATE_SELECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GENERATE_SELECT, SeahorseGenerateSelect))
#define SEAHORSE_GENERATE_SELECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GENERATE_SELECT, SeahorseGenerateSelectClass))
#define SEAHORSE_IS_GENERATE_SELECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GENERATE_SELECT))
#define SEAHORSE_IS_GENERATE_SELECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GENERATE_SELECT))
#define SEAHORSE_GENERATE_SELECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GENERATE_SELECT, SeahorseGenerateSelectClass))

typedef struct _SeahorseGenerateSelect SeahorseGenerateSelect;
typedef struct _SeahorseGenerateSelectClass SeahorseGenerateSelectClass;
typedef struct _SeahorseGenerateSelectPrivate SeahorseGenerateSelectPrivate;

struct _SeahorseGenerateSelect {
	SeahorseWidget parent_instance;
	SeahorseGenerateSelectPrivate *pv;
};

struct _SeahorseGenerateSelectClass {
	SeahorseWidgetClass parent_class;
};

GType seahorse_generate_select_get_type (void);

void        seahorse_generate_select_show       (GtkWindow *parent);

G_END_DECLS

#endif
