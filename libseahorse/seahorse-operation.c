/*
 * Seahorse
 *
 * Copyright (C) 2004 Nate Nielsen
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
 
#include <gnome.h>

#include "config.h"
#include "seahorse-util.h"
#include "seahorse-marshal.h"
#include "seahorse-operation.h"

enum {
    DONE,
    PROGRESS,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

/* GObject handlers */
static void seahorse_operation_class_init (SeahorseOperationClass *klass);
static void seahorse_operation_init       (SeahorseOperation *operation);
static void seahorse_operation_dispose    (GObject *gobject);
static void seahorse_operation_finalize   (GObject *gobject);

GType
seahorse_operation_get_type (void)
{
    static GType operation_type = 0;
 
    if (!operation_type) {
        
        static const GTypeInfo operation_info = {
            sizeof (SeahorseOperationClass), NULL, NULL,
            (GClassInitFunc) seahorse_operation_class_init, NULL, NULL,
            sizeof (SeahorseOperation), 0, (GInstanceInitFunc) seahorse_operation_init
        };
        
        operation_type = g_type_register_static (G_TYPE_OBJECT, 
                                "SeahorseOperation", &operation_info, 0);
    }
  
    return operation_type;
}

static void
seahorse_operation_class_init (SeahorseOperationClass *klass)
{
    GObjectClass *gobject_class;
   
    parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->dispose = seahorse_operation_dispose;
    gobject_class->finalize = seahorse_operation_finalize;

    signals[DONE] = g_signal_new ("done", SEAHORSE_TYPE_OPERATION, 
                G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SeahorseOperationClass, done),
                NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    signals[PROGRESS] = g_signal_new ("progress", SEAHORSE_TYPE_OPERATION, 
                G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SeahorseOperationClass, progress),
                NULL, NULL, seahorse_marshal_VOID__STRING_DOUBLE, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_DOUBLE);
}

/* Initialize the object */
static void
seahorse_operation_init (SeahorseOperation *operation)
{
    /* This is the state of non-started operation */
    operation->current = -1;
    operation->total = 0;
}

/* dispose of all our internal references */
static void
seahorse_operation_dispose (GObject *gobject)
{
    SeahorseOperation *operation;
    operation = SEAHORSE_OPERATION (gobject);
    
    if (!seahorse_operation_is_done (operation))
        seahorse_operation_cancel (operation);
        
    G_OBJECT_CLASS (parent_class)->dispose (gobject);
}

/* free private vars */
static void
seahorse_operation_finalize (GObject *gobject)
{
    SeahorseOperation *operation;
    operation = SEAHORSE_OPERATION (gobject);
    g_assert (seahorse_operation_is_done (operation));
    
    if (operation->error) {
        g_error_free (operation->error);
        operation->error = NULL;
    }
        
    g_free (operation->details);
    operation->details = NULL;
        
    G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

SeahorseOperation*  
seahorse_operation_new_complete (GError *err)
{
    SeahorseOperation *operation;
    
    operation = g_object_new (SEAHORSE_TYPE_OPERATION, NULL);
    seahorse_operation_mark_start (operation);
    seahorse_operation_mark_done (operation, FALSE, err);
    return operation;
}

void
seahorse_operation_cancel (SeahorseOperation *operation)
{
    SeahorseOperationClass *klass;

    g_return_if_fail (SEAHORSE_IS_OPERATION (operation));
    g_return_if_fail (!seahorse_operation_is_done (operation));

	g_object_ref (operation);
 
    klass = SEAHORSE_OPERATION_GET_CLASS (operation);
    g_return_if_fail (klass->cancel != NULL);
    
    (*klass->cancel) (operation); 
    
	g_object_unref (operation);
}

void                
seahorse_operation_steal_error (SeahorseOperation *operation, GError **err)
{
    g_return_if_fail (err == NULL || *err == NULL);
    if (err) {
        *err = operation->error;
        operation->error = NULL;
    }
}

void                
seahorse_operation_copy_error  (SeahorseOperation *operation, GError **err)
{
    g_return_if_fail (err == NULL || *err == NULL);
    if (err) 
        *err = operation->error ? g_error_copy (operation->error) : NULL;
}

gpointer
seahorse_operation_get_result (SeahorseOperation *operation)
{
    return g_object_get_data (G_OBJECT (operation), "result");
}

void                
seahorse_operation_wait (SeahorseOperation *operation)
{
    seahorse_util_wait_until (seahorse_operation_is_done (operation));
}

gdouble
seahorse_operation_get_progress (SeahorseOperation *operation)
{
    if (operation->total == 0)
        return -1;
    return (gdouble)(operation->current) / (gdouble)(operation->total);
}

void                
seahorse_operation_mark_start (SeahorseOperation *operation)
{
    g_return_if_fail (SEAHORSE_IS_OPERATION (operation));
        
    /* A running operation always refs itself */
    g_object_ref (operation);
    operation->cancelled = FALSE;
    operation->details = NULL;
    operation->current = 0;
    operation->total = 0;
}

void 
seahorse_operation_mark_progress (SeahorseOperation *operation, const gchar *details, 
                                  gint current, gint total)
{
    gboolean emit = FALSE;
    
    g_return_if_fail (SEAHORSE_IS_OPERATION (operation));
    g_return_if_fail (operation->total != -1);
    g_return_if_fail (total >= 0);
    g_return_if_fail (current >= 0 && current <= total);
    
    /* We reserve 'current == total' for complete operations */
    if (current == total && total != 0)
        current = total - 1;
    
    if (operation->current != current) {
        operation->current = current;
        emit = TRUE;
    }
    
    if (operation->total != total) {
        operation->total = total;
        emit = TRUE;
    }
    
    if (!seahorse_util_string_equals (operation->details, details)) {
        g_free (operation->details);
        operation->details = details ? g_strdup (details) : NULL;
        emit = TRUE;
    }

    if (emit)    
        g_signal_emit (G_OBJECT (operation), signals[PROGRESS], 0, operation->details, 
                       seahorse_operation_get_progress (operation));
    
    g_return_if_fail (!seahorse_operation_is_done (operation));
}

static gboolean
delayed_mark_done (SeahorseOperation *operation)
{
    g_signal_emit (operation, signals[DONE], 0);

    /* A running operation always refs itself */
    g_object_unref (operation);   
    return FALSE;
}

void                
seahorse_operation_mark_done (SeahorseOperation *operation, gboolean cancelled,
                              GError *error)
{
    g_return_if_fail (SEAHORSE_IS_OPERATION (operation));
    g_return_if_fail (!seahorse_operation_is_done (operation));

    /* No details on completed operations */
    g_free (operation->details);
    operation->details = NULL;
    
    operation->current = operation->total;
    operation->cancelled = cancelled;
    operation->error = error;

    g_signal_emit (operation, signals[PROGRESS], 0, NULL, 1.0);

    if (operation->total <= 0)
        operation->total = 1;
    operation->current = operation->total;

    /* Since we're asynchronous we guarantee this by going to back out
       to the loop before marking an operation as done */
    g_timeout_add (0, (GSourceFunc)delayed_mark_done, operation);
}

/* -----------------------------------------------------------------------------
 * MULTI OPERATION
 */

/* GObject handlers */
static void seahorse_multi_operation_class_init (SeahorseMultiOperationClass *klass);
static void seahorse_multi_operation_init       (SeahorseMultiOperation *mop);
static void seahorse_multi_operation_dispose    (GObject *gobject);
static void seahorse_multi_operation_finalize   (GObject *gobject);
static void seahorse_multi_operation_cancel     (SeahorseOperation *operation);

static GObjectClass *multi_parent_class = NULL;

GType
seahorse_multi_operation_get_type (void)
{
    static GType gtype = 0;
 
    if (!gtype) {
        
        static const GTypeInfo ginfo = {
            sizeof (SeahorseMultiOperationClass), NULL, NULL,
            (GClassInitFunc) seahorse_multi_operation_class_init, NULL, NULL,
            sizeof (SeahorseMultiOperation), 0, (GInstanceInitFunc) seahorse_multi_operation_init
        };
        
        gtype = g_type_register_static (SEAHORSE_TYPE_OPERATION, "SeahorseMultiOperation", 
                                        &ginfo, 0);
    }
  
    return gtype;
}

static void
seahorse_multi_operation_class_init (SeahorseMultiOperationClass *klass)
{
    SeahorseOperationClass *op_class = SEAHORSE_OPERATION_CLASS (klass);
    GObjectClass *gobject_class;

    multi_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    op_class->cancel = seahorse_multi_operation_cancel;        
    gobject_class->dispose = seahorse_multi_operation_dispose;
    gobject_class->finalize = seahorse_multi_operation_finalize;
}

/* Initialize the object */
static void
seahorse_multi_operation_init (SeahorseMultiOperation *mop)
{
    mop->operations = NULL;
}

/* dispose of all our internal references */
static void
seahorse_multi_operation_dispose (GObject *gobject)
{
    SeahorseMultiOperation *mop;
    mop = SEAHORSE_MULTI_OPERATION (gobject);
    
    /* Anything remaining, gets released  */
    mop->operations = seahorse_operation_list_free (mop->operations);
        
    G_OBJECT_CLASS (parent_class)->dispose (gobject);
}

/* free private vars */
static void
seahorse_multi_operation_finalize (GObject *gobject)
{
    SeahorseMultiOperation *mop;
    mop = SEAHORSE_MULTI_OPERATION (gobject);
    
    g_assert (mop->operations == NULL);
    
    G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void 
seahorse_multi_operation_cancel (SeahorseOperation *operation)
{
    SeahorseMultiOperation *mop;
    
    g_return_if_fail (SEAHORSE_IS_MULTI_OPERATION (operation));
    mop = SEAHORSE_MULTI_OPERATION (operation);

    seahorse_operation_list_cancel (mop->operations);
    seahorse_operation_list_purge (mop->operations);    
    
    seahorse_operation_mark_done (operation, TRUE, 
                                  SEAHORSE_OPERATION (mop)->error);
}

SeahorseMultiOperation*  
seahorse_multi_operation_new ()
{
    SeahorseMultiOperation *mop = g_object_new (SEAHORSE_TYPE_MULTI_OPERATION, NULL); 
    return mop;
}

static void
multi_operation_progress (SeahorseOperation *operation, const gchar *message, 
                          gdouble fract, SeahorseMultiOperation *mop)
{
	GSList *list;
    
    g_return_if_fail (SEAHORSE_IS_MULTI_OPERATION (mop));
    g_return_if_fail (SEAHORSE_IS_OPERATION (operation));  
	
    g_assert (mop->operations);
	list = mop->operations;
    
    /* One or two operations, simple */
    if (g_slist_length (list) <= 1) {

#ifdef _DEBUG 
        g_printerr ("[multi-operation 0x%08X] single progress: %s %d/%d\n", (guint)mop, 
            seahorse_operation_get_details (operation), operation->current, operation->total);
#endif
        
        seahorse_operation_mark_progress (SEAHORSE_OPERATION (mop), 
                            seahorse_operation_get_details (operation), 
                            operation->current, operation->total);


    /* When more than one operation calculate the fraction */
    } else {
    
        SeahorseOperation *op;
    	gdouble total = 0;
	    gdouble current = 0;
        
        message = seahorse_operation_get_details (operation);

    	/* Get the total progress */
        while (list) {
		    op = SEAHORSE_OPERATION (list->data);
    		list = g_slist_next (list);

            if (message && !message[0])
                message = NULL;
            
            if (!message && !seahorse_operation_is_done (op))
                message = seahorse_operation_get_details (op);
		
	    	if (!seahorse_operation_is_cancelled (op)) {
		    	if (op->total == 0) {
    				total += 1;
	    			current += (seahorse_operation_is_done (op) ? 1 : 0);
			    } else {
		    		total += (op->total >= 0 ? op->total : 0);
    				current += (op->current >= 0 ? op->current : 0);
	    		}
		    }
        }		

#ifdef _DEBUG 
        g_printerr ("[multi-operation 0x%08X] multi progress: %s %d/%d\n", (guint)mop, 
                    message, (gint)current, (gint)total);
#endif
        
        seahorse_operation_mark_progress (SEAHORSE_OPERATION (mop), message,
	    								  current, total);
    }
}

static void
multi_operation_done (SeahorseOperation *op, SeahorseMultiOperation *mop)
{
    GSList *l;
    gboolean done = TRUE;
    
    g_return_if_fail (SEAHORSE_IS_MULTI_OPERATION (mop));
    g_return_if_fail (SEAHORSE_IS_OPERATION (op));  
  
    g_signal_handlers_disconnect_by_func (op, multi_operation_done, mop);
    g_signal_handlers_disconnect_by_func (op, multi_operation_progress, mop);
    
    if (!seahorse_operation_is_successful (op) && !SEAHORSE_OPERATION (mop)->error)
        seahorse_operation_copy_error (op, &(SEAHORSE_OPERATION (mop)->error));

#ifdef _DEBUG 
    g_printerr ("[multi-operation 0x%08X] part complete (%d): 0x%08X/%s\n", (guint)mop, 
                g_slist_length (mop->operations), (guint)op, seahorse_operation_get_details (op));
#endif
        
    /* Are we done with all of them? */
    for (l = mop->operations; l; l = g_slist_next (l)) {
        if (!seahorse_operation_is_done (SEAHORSE_OPERATION (l->data))) 
            done = FALSE;
    }
    
    /* Not done, just update progress. */
    if (!done) {
   		multi_operation_progress (SEAHORSE_OPERATION (mop), NULL, -1, mop);
        return;
    }

#ifdef _DEBUG 
    g_printerr ("[multi-operation 0x%08X] complete\n", (guint)mop);
#endif

    /* Remove all the listeners */
    for (l = mop->operations; l; l = g_slist_next (l)) {
        g_signal_handlers_disconnect_by_func (l->data, multi_operation_done, mop);
        g_signal_handlers_disconnect_by_func (l->data, multi_operation_done, mop);
    }

    mop->operations = seahorse_operation_list_purge (mop->operations);
    
    seahorse_operation_mark_done (SEAHORSE_OPERATION (mop), FALSE, 
                                  SEAHORSE_OPERATION (mop)->error);        
}

void                     
seahorse_multi_operation_add (SeahorseMultiOperation* mop, SeahorseOperation *op)
{
    g_return_if_fail (SEAHORSE_IS_MULTI_OPERATION (mop));
    g_return_if_fail (SEAHORSE_IS_OPERATION (op));
    
    if(mop->operations == NULL) {
#ifdef _DEBUG 
        g_printerr ("[multi-operation 0x%08X] start\n", (guint)mop);
#endif        
        seahorse_operation_mark_start (SEAHORSE_OPERATION (mop));
    }
    
#ifdef _DEBUG
    g_printerr ("[multi-operation 0x%08X] adding part: 0x%08X\n", (guint)mop, (guint)op);
#endif
    
    mop->operations = seahorse_operation_list_add (mop->operations, op);
    
    g_signal_connect (op, "done", G_CALLBACK (multi_operation_done), mop);
    g_signal_connect (op, "progress", G_CALLBACK (multi_operation_progress), mop);
    
    multi_operation_progress (op, NULL, -1, mop);
}

/* -----------------------------------------------------------------------------
 * OPERATION LIST
 */

GSList*             
seahorse_operation_list_add (GSList *list, SeahorseOperation *operation)
{
    /* This assumes the main reference */
    return g_slist_prepend (list, operation);
}

GSList*             
seahorse_operation_list_remove (GSList *list, SeahorseOperation *operation)
{
   GSList *element;
   
   element = g_slist_find (list, operation);
   if (element) {
        g_object_unref (operation);
        list = g_slist_remove_link (list, element);
        g_slist_free (element);
   } 
   
   return list;
}

void             
seahorse_operation_list_cancel (GSList *list)
{
    SeahorseOperation *operation;
    
    while (list) {
        operation = SEAHORSE_OPERATION (list->data);
        list = g_slist_next (list);
        
        if (!seahorse_operation_is_done (operation))
            seahorse_operation_cancel (operation);
    }
}

GSList*             
seahorse_operation_list_purge (GSList *list)
{
    GSList *l, *p;
    
    p = list;
    
    while (p != NULL) {

        l = p;
        p = g_slist_next (p);
        
        if (seahorse_operation_is_done (SEAHORSE_OPERATION (l->data))) {
            g_object_unref (G_OBJECT (l->data));

            list = g_slist_remove_link (list, l);
            g_slist_free (l);
        }         
    }
    
    return list;
}
                                                    

                                                    
GSList*             
seahorse_operation_list_free (GSList *list)
{
    GSList *l;
    
    for (l = list; l; l = g_slist_next (l)) {
        g_assert (SEAHORSE_IS_OPERATION (l->data));
        g_object_unref (G_OBJECT (l->data));
    }
    
    g_slist_free (list);
    return NULL;
}
