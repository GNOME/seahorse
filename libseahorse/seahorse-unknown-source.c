/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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

#include "config.h"

#include <glib/gi18n.h>

#include "seahorse-unknown-source.h"

#include "seahorse-context.h"
#include "seahorse-unknown.h"

#include "common/seahorse-registry.h"

enum {
    PROP_0,
    PROP_KEY_TYPE,
    PROP_KEY_DESC,
    PROP_LOCATION
};

static void seahorse_source_iface (SeahorseSourceIface *iface);

G_DEFINE_TYPE_EXTENDED (SeahorseUnknownSource, seahorse_unknown_source, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_SOURCE, seahorse_source_iface));

/* -----------------------------------------------------------------------------
 * INTERNAL
 */

static void
search_done (SeahorseOperation *op, SeahorseObject *sobj)
{
	g_object_set (sobj, "location", SEAHORSE_LOCATION_MISSING, NULL);
}

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static SeahorseOperation*
seahorse_unknown_source_load (SeahorseSource *src)
{ 
	return seahorse_operation_new_complete (NULL);
}

static void 
seahorse_unknown_source_set_property (GObject *object, guint prop_id, const GValue *value, 
                                      GParamSpec *pspec)
{
    SeahorseUnknownSource *usrc = SEAHORSE_UNKNOWN_SOURCE (object);
    
    switch (prop_id) {
    case PROP_KEY_TYPE:
        usrc->ktype = g_value_get_uint (value);
        break;
    }
}

static void 
seahorse_unknown_source_get_property (GObject *object, guint prop_id, GValue *value, 
                                  GParamSpec *pspec)
{
    SeahorseUnknownSource *usrc = SEAHORSE_UNKNOWN_SOURCE (object);
    
    switch (prop_id) {
    case PROP_KEY_TYPE:
        g_value_set_uint (value, usrc->ktype);
        break;
    case PROP_KEY_DESC:
        g_value_set_string (value, _("Unavailable Key"));
        break;
    case PROP_LOCATION:
        g_value_set_uint (value, SEAHORSE_LOCATION_MISSING);
        break;
    }
}

static void
seahorse_unknown_source_init (SeahorseUnknownSource *ssrc)
{

}

static void
seahorse_unknown_source_class_init (SeahorseUnknownSourceClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
   
	seahorse_unknown_source_parent_class = g_type_class_peek_parent (klass);
    
	gobject_class->set_property = seahorse_unknown_source_set_property;
	gobject_class->get_property = seahorse_unknown_source_get_property;
    
	g_object_class_override_property (gobject_class, PROP_KEY_TYPE, "key-type");
	g_object_class_override_property (gobject_class, PROP_KEY_DESC, "key-desc");
	g_object_class_override_property (gobject_class, PROP_LOCATION, "location");
    
	seahorse_registry_register_type (NULL, SEAHORSE_TYPE_UNKNOWN_SOURCE, "source", NULL);
}

static void 
seahorse_source_iface (SeahorseSourceIface *iface)
{
	iface->load = seahorse_unknown_source_load;
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseUnknownSource*
seahorse_unknown_source_new (GQuark ktype)
{
   return g_object_new (SEAHORSE_TYPE_UNKNOWN_SOURCE, "key-type", ktype, NULL);
}

SeahorseObject*                     
seahorse_unknown_source_add_object (SeahorseUnknownSource *usrc, GQuark id,
                                    SeahorseOperation *search)
{
    SeahorseObject *sobj;

    g_return_val_if_fail (id != 0, NULL);

    sobj = seahorse_context_get_object (SCTX_APP (), SEAHORSE_SOURCE (usrc), id);
    if (!sobj) {
        sobj = SEAHORSE_OBJECT (seahorse_unknown_new (usrc, id, NULL));
        seahorse_context_add_object (SCTX_APP (), sobj);
    }
    
    if (search) {
        g_object_set (sobj, "location", SEAHORSE_LOCATION_SEARCHING, NULL);
        seahorse_operation_watch (search, (SeahorseDoneFunc) search_done, sobj, NULL, NULL);
    }
    
    return sobj;
}
