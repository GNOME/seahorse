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
    /* New operations are always in the 'done' state.
     * We use 2.0 as our done value  */
    operation->state = 2.0;
}

/* dispose of all our internal references */
static void
seahorse_operation_dispose (GObject *gobject)
{
    SeahorseOperation *operation;
    operation = SEAHORSE_OPERATION (gobject);
    
    if (operation->state < 2.0) 
        seahorse_operation_cancel (operation);
        
    G_OBJECT_CLASS (parent_class)->dispose (gobject);
}

/* free private vars */
static void
seahorse_operation_finalize (GObject *gobject)
{
    SeahorseOperation *operation;
    operation = SEAHORSE_OPERATION (gobject);
    g_assert (operation->state >= 2.0);
    
    if (operation->error) {
        g_error_free (operation->error);
        operation->error = NULL;
    }
        
    G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

SeahorseOperation*  
seahorse_operation_new_complete ()
{
    SeahorseOperation *operation;
    
    operation = g_object_new (SEAHORSE_TYPE_OPERATION, NULL);
    operation->state = 2.0;   
    operation->cancelled = FALSE;
    operation->error = NULL;
    return operation;
}

void
seahorse_operation_cancel (SeahorseOperation *operation)
{
    SeahorseOperationClass *klass;

    g_return_if_fail (SEAHORSE_IS_OPERATION (operation));
    g_return_if_fail (operation->state < 2.0);

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
    seahorse_util_wait_until (operation->state >= 2.0);
}

void                
seahorse_operation_mark_start (SeahorseOperation *operation)
{
    g_return_if_fail (SEAHORSE_IS_OPERATION (operation));
        
    /* A running operation always refs itself */
    g_object_ref (operation);
    operation->state = -1;
}

void 
seahorse_operation_mark_progress (SeahorseOperation *operation, const gchar *status, 
                                  gdouble progress)
{
    g_return_if_fail (SEAHORSE_IS_OPERATION (operation));
    g_return_if_fail (operation->state < 2.0);
    g_return_if_fail (progress < 2.0);
    
    if (operation->state != progress || (!operation->status && status) || 
        (!status && operation->status) || !g_str_equal (operation->status, status)) {
        operation->state = progress;
        
        if (status) {
            g_free (operation->status);
            operation->status = g_strdup (status);
        }
        
        g_signal_emit (G_OBJECT (operation), signals[PROGRESS], 0, operation->status, operation->state);
    }
}

void                
seahorse_operation_mark_done (SeahorseOperation *operation, gboolean cancelled,
                              GError *error)
{
    g_return_if_fail (SEAHORSE_IS_OPERATION (operation));
    g_return_if_fail (operation->state < 2.0);

    /* No status on completed operations */
    g_free (operation->status);
    operation->status = NULL;
    
    operation->state = 1.0;   
    operation->cancelled = cancelled;
    operation->error = error;

    g_signal_emit (operation, signals[PROGRESS], 0, operation->status, operation->state);
    
    operation->state = 2.0;
    g_signal_emit (operation, signals[DONE], 0);

    /* A running operation always refs itself */
    g_object_unref (operation);
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
    
    seahorse_operation_mark_done (operation, TRUE, NULL);
}

SeahorseMultiOperation*  
seahorse_multi_operation_new ()
{
    return g_object_new (SEAHORSE_TYPE_MULTI_OPERATION, NULL); 
}

static void
operation_progress (SeahorseOperation *operation, const gchar *message, 
                    gdouble fract, SeahorseMultiOperation *mop)
{
    g_return_if_fail (SEAHORSE_IS_MULTI_OPERATION (mop));
    g_return_if_fail (SEAHORSE_IS_OPERATION (operation));  
  
    g_assert (mop->operations);
    
    /* Only update for first one in the list */
    if (SEAHORSE_OPERATION (mop->operations->data) == operation)
        seahorse_operation_mark_progress (SEAHORSE_OPERATION (mop), message, fract);
}

static void
operation_done (SeahorseOperation *op, SeahorseMultiOperation *mop)
{
    g_return_if_fail (SEAHORSE_IS_MULTI_OPERATION (mop));
    g_return_if_fail (SEAHORSE_IS_OPERATION (op));  
  
    g_assert (mop->operations);
    
    g_signal_handlers_disconnect_by_func (op, operation_done, mop);
    g_signal_handlers_disconnect_by_func (op, operation_progress, mop);
    
    mop->operations = seahorse_operation_list_remove (mop->operations, op);

    /* Are we done with all of them? */
    if (mop->operations == NULL)
        seahorse_operation_mark_done (SEAHORSE_OPERATION (mop), FALSE, NULL);

    /* Otherwise update status to first one in list */
    else {
        op = SEAHORSE_OPERATION (mop->operations->data);
        operation_progress (op, seahorse_operation_get_status (op),
                            seahorse_operation_get_progress (op), mop); 
    }
}

void                     
seahorse_multi_operation_add (SeahorseMultiOperation* mop, SeahorseOperation *op)
{
    gboolean first;
    
    g_return_if_fail (SEAHORSE_IS_MULTI_OPERATION (mop));
    g_return_if_fail (SEAHORSE_IS_OPERATION (op));
    
    /* Don't bother with completed stuff */
    if (seahorse_operation_is_done (op)) {
        g_object_unref (op);
        return;
    }

    first = (mop->operations == NULL);
    
    mop->operations = seahorse_operation_list_add (mop->operations, op);
    
    g_signal_connect (op, "done", G_CALLBACK (operation_done), mop);
    g_signal_connect (op, "progress", G_CALLBACK (operation_progress), mop);

    if(first) {
        seahorse_operation_mark_start (SEAHORSE_OPERATION (mop));
        operation_progress (op, seahorse_operation_get_status (op),
                            seahorse_operation_get_progress (op), mop);     
    }    
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
