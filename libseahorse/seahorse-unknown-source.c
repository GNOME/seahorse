/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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
#include "seahorse-registry.h"
#include "seahorse-unknown.h"

enum {
    PROP_0,
    PROP_SOURCE_TAG,
    PROP_SOURCE_LOCATION,
    PROP_CONSTRUCT_TAG,
};

static void     seahorse_source_iface    (SeahorseSourceIface *iface);

G_DEFINE_TYPE_EXTENDED (SeahorseUnknownSource, seahorse_unknown_source, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_SOURCE, seahorse_source_iface));

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static void 
seahorse_unknown_source_set_property (GObject *object, guint prop_id, const GValue *value, 
                                      GParamSpec *pspec)
{
    SeahorseUnknownSource *usrc = SEAHORSE_UNKNOWN_SOURCE (object);
    
    switch (prop_id) {
    case PROP_CONSTRUCT_TAG:
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
    case PROP_SOURCE_TAG:
        g_value_set_uint (value, usrc->ktype);
        break;
    case PROP_CONSTRUCT_TAG:
        g_value_set_uint (value, usrc->ktype);
        break;
    case PROP_SOURCE_LOCATION:
        g_value_set_enum (value, SEAHORSE_LOCATION_MISSING);
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
    
	g_object_class_override_property (gobject_class, PROP_SOURCE_TAG, "source-tag");
	g_object_class_override_property (gobject_class, PROP_SOURCE_LOCATION, "source-location");
	
	/* 
	 * This is a writable construct only property that lets us construct this  
	 * class with different 'source-tag' property. The 'source-tag' property
	 * is read-only, and so we can't use it directly.
	 */
	g_object_class_install_property (gobject_class, PROP_CONSTRUCT_TAG, 
	         g_param_spec_uint ("construct-tag", "Construct Tag", "Set source-tag during construction of object",
	                            0, G_MAXUINT, 0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    
	seahorse_registry_register_type (NULL, SEAHORSE_TYPE_UNKNOWN_SOURCE, "source", NULL);
}


static void
seahorse_unknown_source_load_async (SeahorseSource *source,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
	GSimpleAsyncResult *res;

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_unknown_source_load_async);

	g_simple_async_result_complete_in_idle (res);
	g_object_unref (res);
}

static gboolean
seahorse_unknown_source_load_finish (SeahorseSource *source,
                                     GAsyncResult *result,
                                     GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_unknown_source_load_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

static void 
seahorse_source_iface (SeahorseSourceIface *iface)
{
	iface->load_async = seahorse_unknown_source_load_async;
	iface->load_finish = seahorse_unknown_source_load_finish;
}

SeahorseUnknownSource*
seahorse_unknown_source_new (GQuark ktype)
{
	return g_object_new (SEAHORSE_TYPE_UNKNOWN_SOURCE,
	                     "construct-tag", ktype,
	                     NULL);
}

static void
on_cancellable_gone (gpointer user_data,
                     GObject *where_the_object_was)
{
	SeahorseObject *object = SEAHORSE_OBJECT (user_data);
	g_object_set (object, "location", SEAHORSE_LOCATION_MISSING, NULL);
}

SeahorseObject *
seahorse_unknown_source_add_object (SeahorseUnknownSource *self,
                                    GQuark id,
                                    GCancellable *cancellable)
{
	SeahorseObject *object;
	SeahorseContext *context;

	g_return_val_if_fail (id != 0, NULL);
	g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

	context = seahorse_context_instance ();
	object = seahorse_context_get_object (context, SEAHORSE_SOURCE (self), id);
	if (!object) {
		object = SEAHORSE_OBJECT (seahorse_unknown_new (self, id, NULL));
		seahorse_context_add_object (context, object);
	}

	if (cancellable) {
		g_object_set (object, "location", SEAHORSE_LOCATION_SEARCHING, NULL);
		g_object_weak_ref (G_OBJECT (cancellable), on_cancellable_gone, object);
	} else {
		g_object_set (object, "location", SEAHORSE_LOCATION_MISSING, NULL);
	}

	return object;
}
