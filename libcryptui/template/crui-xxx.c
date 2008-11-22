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

#include "crui-xxx.h"

enum {
	PROP_0,
	PROP_XXX
};

enum {
	SIGNAL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _CruiXxxPrivate {
};

G_DEFINE_TYPE (CruiXxx, crui_xxx, G_TYPE_OBJECT);

#define CRUI_XXX_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), CRUI_TYPE_XXX, CruiXxxPrivate))

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

/* -----------------------------------------------------------------------------
 * OBJECT 
 */


static GObject* 
crui_xxx_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	GObject *obj = G_OBJECT_CLASS (crui_xxx_parent_class)->constructor (type, n_props, props);
	CruiXxx *self = NULL;
	CruiXxxPrivate *pv;
	
	if (obj) {
		pv = CRUI_XXX_GET_PRIVATE (obj);
		self = CRUI_XXX (obj);
		
	}
	
	return obj;
}

static void
crui_xxx_init (CruiXxx *self)
{
	CruiXxxPrivate *pv = CRUI_XXX_GET_PRIVATE (self);

}

static void
crui_xxx_dispose (GObject *obj)
{
	CruiXxx *self = CRUI_XXX (obj);
	CruiXxxPrivate *pv = CRUI_XXX_GET_PRIVATE (self);
    
	G_OBJECT_CLASS (crui_xxx_parent_class)->dispose (obj);
}

static void
crui_xxx_finalize (GObject *obj)
{
	CruiXxx *self = CRUI_XXX (obj);
	CruiXxxPrivate *pv = CRUI_XXX_GET_PRIVATE (self);

	G_OBJECT_CLASS (crui_xxx_parent_class)->finalize (obj);
}

static void
crui_xxx_set_property (GObject *obj, guint prop_id, const GValue *value, 
                           GParamSpec *pspec)
{
	CruiXxx *self = CRUI_XXX (obj);
	CruiXxxPrivate *pv = CRUI_XXX_GET_PRIVATE (self);
	
	switch (prop_id) {
	case PROP_XXX:
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
crui_xxx_get_property (GObject *obj, guint prop_id, GValue *value, 
                           GParamSpec *pspec)
{
	CruiXxx *self = CRUI_XXX (obj);
	CruiXxxPrivate *pv = CRUI_XXX_GET_PRIVATE (self);
	
	switch (prop_id) {
	case PROP_XXX:
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
crui_xxx_class_init (CruiXxxClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    
	crui_xxx_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (CruiXxxPrivate));

	gobject_class->constructor = crui_xxx_constructor;
	gobject_class->dispose = crui_xxx_dispose;
	gobject_class->finalize = crui_xxx_finalize;
	gobject_class->set_property = crui_xxx_set_property;
	gobject_class->get_property = crui_xxx_get_property;
    
	g_object_class_install_property (gobject_class, PROP_XXX,
	           g_param_spec_pointer ("xxx", "Xxx", "Xxx.", G_PARAM_READWRITE));
    
	signals[SIGNAL] = g_signal_new ("signal", CRUI_TYPE_XXX, 
	                                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (CruiXxxClass, signal),
	                                NULL, NULL, g_cclosure_marshal_VOID__OBJECT, 
	                                G_TYPE_NONE, 0);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

CruiXxx*
crui_xxx_new (void)
{
	return g_object_new (CRUI_TYPE_XXX, NULL);
}
