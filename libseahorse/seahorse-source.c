/*
 * Seahorse
 *
 * Copyright (C) 2004,2005 Stefan Walter
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

#include "seahorse-context.h"
#include "seahorse-marshal.h"
#include "seahorse-object.h"
#include "seahorse-source.h"
#include "seahorse-util.h"

#include "common/seahorse-registry.h"

/**
 * SECTION:seahorse-source
 * @short_description: This class stores and handles key sources
 * @include:seahorse-source.h
 *
 **/


/* ---------------------------------------------------------------------------------
 * INTERFACE
 */

/**
* gobject_class: The object class to init
*
* Adds the interfaces "source-tag" and "source-location"
*
**/
static void
seahorse_source_base_init (gpointer gobject_class)
{
	static gboolean initialized = FALSE;
	if (!initialized) {
		
		/* Add properties and signals to the interface */
		g_object_interface_install_property (gobject_class,
		        g_param_spec_uint ("source-tag", "Source Tag", "Tag of objects that come from this source.", 
		                           0, G_MAXUINT, SEAHORSE_TAG_INVALID, G_PARAM_READABLE));

		g_object_interface_install_property (gobject_class, 
		        g_param_spec_enum ("source-location", "Source Location", "Objects in this source are at this location. See SeahorseLocation", 
		                           SEAHORSE_TYPE_LOCATION, SEAHORSE_LOCATION_LOCAL, G_PARAM_READABLE));
		
		initialized = TRUE;
	}
}

/**
 * seahorse_source_get_type:
 *
 * Registers the type of G_TYPE_INTERFACE
 *
 * Returns: the type id
 */
GType
seahorse_source_get_type (void)
{
	static GType type = 0;
	if (!type) {
		static const GTypeInfo info = {
			sizeof (SeahorseSourceIface),
			seahorse_source_base_init,               /* base init */
			NULL,             /* base finalize */
			NULL,             /* class_init */
			NULL,             /* class finalize */
			NULL,             /* class data */
			0,
			0,                /* n_preallocs */
			NULL,             /* instance init */
		};
		type = g_type_register_static (G_TYPE_INTERFACE, "SeahorseSourceIface", &info, 0);
		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}
	
	return type;
}

/* ---------------------------------------------------------------------------------
 * PUBLIC
 */

/**
 * seahorse_source_load:
 * @sksrc: A #SeahorseSource object
 *
 * Refreshes the #SeahorseSource's internal object listing.
 *
 * Returns: the asynchronous refresh operation.
 **/
SeahorseOperation*
seahorse_source_load (SeahorseSource *sksrc)
{
    SeahorseSourceIface *klass;
    
    g_return_val_if_fail (SEAHORSE_IS_SOURCE (sksrc), NULL);
    klass = SEAHORSE_SOURCE_GET_INTERFACE (sksrc);
    g_return_val_if_fail (klass->load != NULL, NULL);
    
    return (*klass->load) (sksrc);
}

/**
 * seahorse_source_load_sync:
 * @sksrc: A #SeahorseSource object
 * 
 * Refreshes the #SeahorseSource's internal object listing, and waits for it to complete.
 **/
void
seahorse_source_load_sync (SeahorseSource *sksrc)
{
    SeahorseOperation *op = seahorse_source_load (sksrc);
    g_return_if_fail (op != NULL);
    seahorse_operation_wait (op);
    g_object_unref (op);
}

/**
 * seahorse_source_load_sync:
 * @sksrc: A #SeahorseSource object
 * 
 * Refreshes the #SeahorseSource's internal object listing. Completes in the background.
 **/
void
seahorse_source_load_async (SeahorseSource *sksrc)
{
    SeahorseOperation *op = seahorse_source_load (sksrc);
    g_return_if_fail (op != NULL);
    g_object_unref (op);
}

/**
 * seahorse_source_search:
 * @sksrc: A #SeahorseSource object
 * @match: Text to search for
 * 
 * Refreshes the #SeahorseSource's internal listing. 
 * 
 * Returns: the asynchronous refresh operation.
 **/
SeahorseOperation*
seahorse_source_search (SeahorseSource *sksrc, const gchar *match)
{
    SeahorseSourceIface *klass;
    
    g_return_val_if_fail (SEAHORSE_IS_SOURCE (sksrc), NULL);
    klass = SEAHORSE_SOURCE_GET_INTERFACE (sksrc);
    g_return_val_if_fail (klass->search != NULL, NULL);
    
    return (*klass->search) (sksrc, match);
}

/**
 * seahorse_source_import:
 * @sksrc: A #SeahorseSource object
 * @input: A stream of data to import
 *
 * Imports data from the stream
 *
 * Returns: the asynchronous import operation
 */
SeahorseOperation* 
seahorse_source_import (SeahorseSource *sksrc, GInputStream *input)
{
	SeahorseSourceIface *klass;
    
	g_return_val_if_fail (G_IS_INPUT_STREAM (input), NULL);
    
	g_return_val_if_fail (SEAHORSE_IS_SOURCE (sksrc), NULL);
	klass = SEAHORSE_SOURCE_GET_INTERFACE (sksrc);   
	g_return_val_if_fail (klass->import != NULL, NULL);
    
	return (*klass->import) (sksrc, input);  
}

/**
 * seahorse_source_import_sync:
 * @sksrc: The #SeahorseSource
 * @input: the input data
 * @err: error
 *
 * Imports data from the stream
 *
 * Returns: Imports the stream, synchronous
 */
gboolean            
seahorse_source_import_sync (SeahorseSource *sksrc, GInputStream *input,
                             GError **err)
{
	SeahorseOperation *op;
    	gboolean ret;

	g_return_val_if_fail (G_IS_INPUT_STREAM (input), FALSE);

	op = seahorse_source_import (sksrc, input);
	g_return_val_if_fail (op != NULL, FALSE);
    
	seahorse_operation_wait (op);
	ret = seahorse_operation_is_successful (op);
	if (!ret)
		seahorse_operation_copy_error (op, err);
    
	g_object_unref (op);
	return ret;    
}

/**
 * seahorse_source_export_objects:
 * @objects: The objects to export
 * @output: The output stream to export the objects to
 *
 * Exports objects. The objects are sorted by source.
 *
 * Returns: The #SeahorseOperation created to export the data
 */
SeahorseOperation*
seahorse_source_export_objects (GList *objects, GOutputStream *output)
{
    SeahorseOperation *op = NULL;
    SeahorseMultiOperation *mop = NULL;
    SeahorseSource *sksrc;
    SeahorseObject *sobj;
    GList *next;
    
	g_return_val_if_fail (G_IS_OUTPUT_STREAM (output), NULL);
	g_object_ref (output);

    /* Sort by object source */
    objects = g_list_copy (objects);
    objects = seahorse_util_objects_sort (objects);
    
    while (objects) {
     
        /* Break off one set (same source) */
        next = seahorse_util_objects_splice (objects);

        g_assert (SEAHORSE_IS_OBJECT (objects->data));
        sobj = SEAHORSE_OBJECT (objects->data);

        /* Export from this object source */        
        sksrc = seahorse_object_get_source (sobj);
        g_return_val_if_fail (sksrc != NULL, FALSE);
        
        if (op != NULL) {
            if (mop == NULL)
                mop = seahorse_multi_operation_new ();
            seahorse_multi_operation_take (mop, op);
        }
        
        /* We pass our own data object, to which data is appended */
        op = seahorse_source_export (sksrc, objects, output);
        g_return_val_if_fail (op != NULL, FALSE);

        g_list_free (objects);
        objects = next;
    }
    
    if (mop) {
        op = SEAHORSE_OPERATION (mop);
        
        /* 
         * Setup the result data properly, as we would if it was a 
         * single export operation.
         */
        seahorse_operation_mark_result (op, output, g_object_unref);
    }
    
    if (!op) 
        op = seahorse_operation_new_complete (NULL);
    
    return op;
}

/**
 * seahorse_source_delete_objects:
 * @objects: A list of objects to delete
 *
 * Deletes a list of objects
 *
 * Returns: The #SeahorseOperation to delete the objects
 */
SeahorseOperation*
seahorse_source_delete_objects (GList *objects)
{
	SeahorseOperation *op = NULL;
	SeahorseMultiOperation *mop = NULL;
	SeahorseObject *sobj;
	GList *l;

	for (l = objects; l; l = g_list_next (l)) {
		
		sobj = SEAHORSE_OBJECT (l->data);
		g_return_val_if_fail (SEAHORSE_IS_OBJECT (sobj), NULL);;
		
		if (op != NULL) {
			if (mop == NULL)
				mop = seahorse_multi_operation_new ();
			seahorse_multi_operation_take (mop, op);
		}

		op = seahorse_object_delete (sobj);
		g_return_val_if_fail (op != NULL, NULL);
	}
    
	if (mop) 
		op = SEAHORSE_OPERATION (mop);
    
	if (!op) 
		op = seahorse_operation_new_complete (NULL);
    
	return op;
}

/**
 * seahorse_source_export:
 * @sksrc: The SeahorseSource
 * @objects: The objects to export
 * @output: The resulting output stream
 *
 *
 *
 * Returns: An export Operation (#SeahorseOperation)
 */
SeahorseOperation* 
seahorse_source_export (SeahorseSource *sksrc, GList *objects, GOutputStream *output)
{
	SeahorseSourceIface *klass;
	SeahorseOperation *op;
	GSList *ids = NULL;
	GList *l;
    
	g_return_val_if_fail (SEAHORSE_IS_SOURCE (sksrc), NULL);
	g_return_val_if_fail (G_IS_OUTPUT_STREAM (output), NULL);
	
	klass = SEAHORSE_SOURCE_GET_INTERFACE (sksrc);   
	if (klass->export)
		return (*klass->export) (sksrc, objects, output);

	/* Either export or export_raw must be implemented */
	g_return_val_if_fail (klass->export_raw != NULL, NULL);
    
	for (l = objects; l; l = g_list_next (l)) 
		ids = g_slist_prepend (ids, GUINT_TO_POINTER (seahorse_object_get_id (l->data)));
    
	ids = g_slist_reverse (ids);
	op = (*klass->export_raw) (sksrc, ids, output);	
	g_slist_free (ids);
	return op;
}

/**
 * seahorse_source_export_raw:
 * @sksrc: The SeahorseSource
 * @ids: A list of IDs to export
 * @output: The resulting output stream
 *
 *
 *
 * Returns: An export Operation (#SeahorseOperation)
 */
SeahorseOperation* 
seahorse_source_export_raw (SeahorseSource *sksrc, GSList *ids, GOutputStream *output)
{
	SeahorseSourceIface *klass;
	SeahorseOperation *op;
	SeahorseObject *sobj;
	GList *objects = NULL;
	GSList *l;
    
	g_return_val_if_fail (SEAHORSE_IS_SOURCE (sksrc), NULL);
	g_return_val_if_fail (output == NULL || G_IS_OUTPUT_STREAM (output), NULL);

	klass = SEAHORSE_SOURCE_GET_INTERFACE (sksrc);   
    
	/* Either export or export_raw must be implemented */
	if (klass->export_raw)
		return (*klass->export_raw)(sksrc, ids, output);
    
	g_return_val_if_fail (klass->export != NULL, NULL);
        
	for (l = ids; l; l = g_slist_next (l)) {
		sobj = seahorse_context_get_object (SCTX_APP (), sksrc, GPOINTER_TO_UINT (l->data));
        
		/* TODO: A proper error message here 'not found' */
		if (sobj)
			objects = g_list_prepend (objects, sobj);
	}
    
	objects = g_list_reverse (objects);
	op = (*klass->export) (sksrc, objects, output);
	g_list_free (objects);
	return op;
}

/**
* seahorse_source_get_tag:
* @sksrc: The seahorse source object
*
*
* Returns: The source-tag property of the object. As #GQuark
*/
GQuark              
seahorse_source_get_tag (SeahorseSource *sksrc)
{
    GQuark ktype;
    g_object_get (sksrc, "source-tag", &ktype, NULL);
    return ktype;
}

/**
 * seahorse_source_get_location:
 * @sksrc: The seahorse source object
 *
 *
 *
 * Returns: The location (#SeahorseLocation) of this object
 */
SeahorseLocation   
seahorse_source_get_location (SeahorseSource *sksrc)
{
    SeahorseLocation loc;
    g_object_get (sksrc, "source-location", &loc, NULL);
    return loc;
}
