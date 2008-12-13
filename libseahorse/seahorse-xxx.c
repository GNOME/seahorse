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

#include "config.h"

#include "seahorse-xxx.h"

enum {
	PROP_0,
	PROP_XXX
};

enum {
	SIGNAL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _SeahorseXxxPrivate {
};

G_DEFINE_TYPE (SeahorseXxx, seahorse_xxx, G_TYPE_OBJECT);

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

/* -----------------------------------------------------------------------------
 * OBJECT 
 */


static GObject* 
seahorse_xxx_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	SeahorseXxx *self = SEAHORSE_XXX (G_OBJECT_CLASS (seahorse_xxx_parent_class)->constructor(type, n_props, props));
	g_return_val_if_fail (self, NULL);	


	
	return G_OBJECT (self);
}

static void
seahorse_xxx_init (SeahorseXxx *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_XXX, SeahorseXxxPrivate);

}

static void
seahorse_xxx_dispose (GObject *obj)
{
	SeahorseXxx *self = SEAHORSE_XXX (obj);
    
	G_OBJECT_CLASS (seahorse_xxx_parent_class)->dispose (obj);
}

static void
seahorse_xxx_finalize (GObject *obj)
{
	SeahorseXxx *self = SEAHORSE_XXX (obj);

	G_OBJECT_CLASS (seahorse_xxx_parent_class)->finalize (obj);
}

static void
seahorse_xxx_set_property (GObject *obj, guint prop_id, const GValue *value, 
                           GParamSpec *pspec)
{
	SeahorseXxx *self = SEAHORSE_XXX (obj);
	
	switch (prop_id) {
	case PROP_XXX:
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_xxx_get_property (GObject *obj, guint prop_id, GValue *value, 
                           GParamSpec *pspec)
{
	SeahorseXxx *self = SEAHORSE_XXX (obj);
	
	switch (prop_id) {
	case PROP_XXX:
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_xxx_class_init (SeahorseXxxClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    
	seahorse_xxx_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseXxxPrivate));

	gobject_class->constructor = seahorse_xxx_constructor;
	gobject_class->dispose = seahorse_xxx_dispose;
	gobject_class->finalize = seahorse_xxx_finalize;
	gobject_class->set_property = seahorse_xxx_set_property;
	gobject_class->get_property = seahorse_xxx_get_property;
    
	g_object_class_install_property (gobject_class, PROP_XXX,
	           g_param_spec_pointer ("xxx", "Xxx", "Xxx.", G_PARAM_READWRITE));
    
	signals[SIGNAL] = g_signal_new ("signal", SEAHORSE_TYPE_XXX, 
	                                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseXxxClass, signal),
	                                NULL, NULL, g_cclosure_marshal_VOID__OBJECT, 
	                                G_TYPE_NONE, 0);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseXxx*
seahorse_xxx_new (void)
{
	return g_object_new (SEAHORSE_TYPE_XXX, NULL);
}
