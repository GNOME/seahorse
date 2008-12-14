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

G_DEFINE_TYPE (SeahorseSource, seahorse_source, G_TYPE_OBJECT);

/* GObject handlers */
static void seahorse_source_dispose    (GObject *gobject);
static void seahorse_source_finalize   (GObject *gobject);

static GObjectClass *parent_class = NULL;

static void
seahorse_source_class_init (SeahorseSourceClass *klass)
{
    GObjectClass *gobject_class;
   
    parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->dispose = seahorse_source_dispose;
    gobject_class->finalize = seahorse_source_finalize;
}

/* Initialize the object */
static void
seahorse_source_init (SeahorseSource *sksrc)
{

}

/* dispose of all our internal references */
static void
seahorse_source_dispose (GObject *gobject)
{
    G_OBJECT_CLASS (parent_class)->dispose (gobject);
}

/* free private vars */
static void
seahorse_source_finalize (GObject *gobject)
{
    G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

/**
 * seahorse_source_load
 * @sksrc: A #SeahorseSource object
 * 
 * Refreshes the #SeahorseSource's internal object listing. 
 * 
 * Returns the asynchronous refresh operation. 
 **/   
SeahorseOperation*
seahorse_source_load (SeahorseSource *sksrc)
{
    SeahorseSourceClass *klass;
    
    g_return_val_if_fail (SEAHORSE_IS_SOURCE (sksrc), NULL);
    klass = SEAHORSE_SOURCE_GET_CLASS (sksrc);
    g_return_val_if_fail (klass->load != NULL, NULL);
    
    return (*klass->load) (sksrc);
}

/**
 * seahorse_source_load_sync
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
 * seahorse_source_load_sync
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
 * seahorse_source_search
 * @sksrc: A #SeahorseSource object
 * @match: Text to search for
 * 
 * Refreshes the #SeahorseSource's internal listing. 
 * 
 * Returns the asynchronous refresh operation. 
 **/   
SeahorseOperation*
seahorse_source_search (SeahorseSource *sksrc, const gchar *match)
{
    SeahorseSourceClass *klass;
    
    g_return_val_if_fail (SEAHORSE_IS_SOURCE (sksrc), NULL);
    klass = SEAHORSE_SOURCE_GET_CLASS (sksrc);
    g_return_val_if_fail (klass->search != NULL, NULL);
    
    return (*klass->search) (sksrc, match);
}

SeahorseOperation* 
seahorse_source_import (SeahorseSource *sksrc, GInputStream *input)
{
	SeahorseSourceClass *klass;
    
	g_return_val_if_fail (G_IS_INPUT_STREAM (input), NULL);
    
	g_return_val_if_fail (SEAHORSE_IS_SOURCE (sksrc), NULL);
	klass = SEAHORSE_SOURCE_GET_CLASS (sksrc);   
	g_return_val_if_fail (klass->import != NULL, NULL);
    
	return (*klass->import) (sksrc, input);  
}

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

SeahorseOperation* 
seahorse_source_export (SeahorseSource *sksrc, GList *objects, GOutputStream *output)
{
	SeahorseSourceClass *klass;
	SeahorseOperation *op;
	GSList *ids = NULL;
	GList *l;
    
	g_return_val_if_fail (SEAHORSE_IS_SOURCE (sksrc), NULL);
	g_return_val_if_fail (G_IS_OUTPUT_STREAM (output), NULL);
	
	klass = SEAHORSE_SOURCE_GET_CLASS (sksrc);   
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

SeahorseOperation* 
seahorse_source_export_raw (SeahorseSource *sksrc, GSList *ids, GOutputStream *output)
{
	SeahorseSourceClass *klass;
	SeahorseOperation *op;
	SeahorseObject *sobj;
	GList *objects = NULL;
	GSList *l;
    
	g_return_val_if_fail (SEAHORSE_IS_SOURCE (sksrc), NULL);
	g_return_val_if_fail (output == NULL || G_IS_OUTPUT_STREAM (output), NULL);

	klass = SEAHORSE_SOURCE_GET_CLASS (sksrc);   
    
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
                                     
GQuark              
seahorse_source_get_ktype (SeahorseSource *sksrc)
{
    GQuark ktype;
    g_object_get (sksrc, "key-type", &ktype, NULL);
    return ktype;
}

SeahorseLocation   
seahorse_source_get_location (SeahorseSource *sksrc)
{
    SeahorseLocation loc;
    g_object_get (sksrc, "location", &loc, NULL);
    return loc;
}

/* -----------------------------------------------------------------------------
 * CANONICAL IDS 
 */

GQuark
seahorse_source_canonize_id (GQuark ktype, const gchar *id)
{
	SeahorseSourceClass *klass;
	GType type;

	g_return_val_if_fail (id != NULL, 0);
    
	type = seahorse_registry_object_type (NULL, "source", g_quark_to_string (ktype), "local", NULL);
	g_return_val_if_fail (type, 0);
	
	klass = SEAHORSE_SOURCE_CLASS (g_type_class_peek (type));
	g_return_val_if_fail (klass, 0);
	
	g_return_val_if_fail (klass->canonize_id, 0);
	return (klass->canonize_id) (id);
}
