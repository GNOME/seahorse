/*
 * Seahorse
 *
 * Copyright (C) 2004-2006 Stefan Walter
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

#include "seahorse-util.h"
#include "seahorse-marshal.h"
#include "seahorse-operation.h"

#define DEBUG_FLAG SEAHORSE_DEBUG_OPERATION
#include "seahorse-debug.h"

/**
 * SECTION:seahorse-operation
 * @short_description: Contains code for operations and multi operations (container for several operations)
 * @include:seahorse-operation.h
 *
 **/

/* -----------------------------------------------------------------------------
 * SEAHORSE OPERATION
 */

enum {
    PROP_0,
    PROP_PROGRESS,
    PROP_MESSAGE,
    PROP_RESULT
};

enum {
    DONE,
    PROGRESS,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (SeahorseOperation, seahorse_operation, G_TYPE_OBJECT);

/* OBJECT ------------------------------------------------------------------- */

/**
* operation:the #SeahorseOperation to initialise
*
* Initialises an operation
*
**/
static void
seahorse_operation_init (SeahorseOperation *operation)
{

    /* This is the state of non-started operation. all other flags are zero */
    operation->progress = -1;
}

/**
* object: an object of the type SEAHORSE_OPERATION
* prop_id: the id of the property to set
* value: The value to set
* pspec: ignored
*
* Sets a property of the SEAHORSE_OPERATION passed
* At the moment only PROP_MESSAGE is supported
*
**/
static void
seahorse_operation_set_property (GObject *object, guint prop_id, 
                                 const GValue *value, GParamSpec *pspec)
{
    SeahorseOperation *op = SEAHORSE_OPERATION (object);
    
    switch (prop_id) {
    case PROP_MESSAGE:
        g_free (op->message);
        op->message = g_value_dup_string (value);
        break;
    }
}

/**
* object: an object of the type SEAHORSE_OPERATION
* prop_id: the id of the property to get
* value: The returned value
* pspec: ignored
*
* Gets a property of the SEAHORSE_OPERATION passed
* Supported properties are: PROP_PROGRESS, PROP_MESSAGE, PROP_RESULT
*
**/
static void
seahorse_operation_get_property (GObject *object, guint prop_id, 
                                 GValue *value, GParamSpec *pspec)
{
    SeahorseOperation *op = SEAHORSE_OPERATION (object);
    
    switch (prop_id) {
    case PROP_PROGRESS:
        g_value_set_double (value, op->progress);
        break;
    case PROP_MESSAGE:
        g_value_set_string (value, op->message);
        break;
    case PROP_RESULT:
        g_value_set_pointer (value, op->result);
        break;
    }
}

/**
* gobject: The operation to dispose
*
* dispose of all our internal references
*
**/
static void
seahorse_operation_dispose (GObject *gobject)
{
    SeahorseOperation *op = SEAHORSE_OPERATION (gobject);
    
    if (op->is_running)
        seahorse_operation_cancel (op);
    g_assert (!op->is_running);

    /* We do this in dispose in case it's a circular reference */
    if (op->result && op->result_destroy)
        (op->result_destroy) (op->result);
    op->result = NULL;
    op->result_destroy = NULL;
    
    G_OBJECT_CLASS (seahorse_operation_parent_class)->dispose (gobject);
}


/**
* gobject: The #SEAHORSE_OPERATION to finalize
*
* free private vars
*
**/
static void
seahorse_operation_finalize (GObject *gobject)
{
    SeahorseOperation *op = SEAHORSE_OPERATION (gobject);
    g_assert (!op->is_running);
    
    if (op->error) {
        g_error_free (op->error);
        op->error = NULL;
    }
        
    g_free (op->message);
    op->message = NULL;
        
    G_OBJECT_CLASS (seahorse_operation_parent_class)->finalize (gobject);
}

/**
* klass: The #SeahorseOperationClass to initialise
*
* Initialises the class, adds properties and signals
*
**/
static void
seahorse_operation_class_init (SeahorseOperationClass *klass)
{
    GObjectClass *gobject_class;
   
    seahorse_operation_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->dispose = seahorse_operation_dispose;
    gobject_class->finalize = seahorse_operation_finalize;
    gobject_class->set_property = seahorse_operation_set_property;
    gobject_class->get_property = seahorse_operation_get_property;

    g_object_class_install_property (gobject_class, PROP_PROGRESS,
        g_param_spec_double ("progress", "Progress position", "Current progress position (fraction between 0 and 1)", 
                             0.0, 1.0, 0.0, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_MESSAGE,
        g_param_spec_string ("message", "Status message", "Current progress message", 
                             NULL, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_RESULT,
        g_param_spec_pointer ("result", "Operation Result", "Exact value depends on derived class.", 
                              G_PARAM_READABLE));

    
    signals[DONE] = g_signal_new ("done", SEAHORSE_TYPE_OPERATION, 
                G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SeahorseOperationClass, done),
                NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    signals[PROGRESS] = g_signal_new ("progress", SEAHORSE_TYPE_OPERATION, 
                G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SeahorseOperationClass, progress),
                NULL, NULL, seahorse_marshal_VOID__STRING_DOUBLE, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_DOUBLE);
}

/* PUBLIC ------------------------------------------------------------------- */

/**
 * seahorse_operation_new_complete:
 * @err: an optional error to set
 *
 * Creates a new operation and sets it's state to done
 *
 * Returns: the operation
 */
SeahorseOperation*  
seahorse_operation_new_complete (GError *err)
{
    SeahorseOperation *operation;
    
    operation = g_object_new (SEAHORSE_TYPE_OPERATION, NULL);
    seahorse_operation_mark_start (operation);
    seahorse_operation_mark_done (operation, FALSE, err);
    return operation;
}

/**
 * seahorse_operation_new_cancelled:
 *
 * Creates a new operation and sets in to cancelled  state
 *
 * Returns: The new operation
 */
SeahorseOperation*
seahorse_operation_new_cancelled ()
{
    SeahorseOperation *operation;
    
    operation = g_object_new (SEAHORSE_TYPE_OPERATION, NULL);
    seahorse_operation_mark_start (operation);
    seahorse_operation_mark_done (operation, TRUE, NULL);
    return operation;
}

/**
 * seahorse_operation_cancel:
 * @op: the #SeahorseOperation to cancel
 *
 * Cancels the operation
 *
 */
void
seahorse_operation_cancel (SeahorseOperation *op)
{
    SeahorseOperationClass *klass;

    g_return_if_fail (SEAHORSE_IS_OPERATION (op));
    g_return_if_fail (op->is_running);

    g_object_ref (op);
 
    klass = SEAHORSE_OPERATION_GET_CLASS (op);
    
    /* A derived operation exists */
    if (klass->cancel)
        (*klass->cancel) (op); 
    
    /* No derived operation exists */
    else
        seahorse_operation_mark_done (op, TRUE, NULL);
    
    g_object_unref (op);
}

/**
 * seahorse_operation_copy_error:
 * @op: the #SeahorseOperation
 * @err: The resulting error
 *
 * Copies an internal error from the #SeahorseOperation to @err
 *
 */
void
seahorse_operation_copy_error  (SeahorseOperation *op, GError **err)
{
	g_return_if_fail (SEAHORSE_IS_OPERATION (op));
	g_return_if_fail (err == NULL || *err == NULL);
    
	if (err) 
		*err = op->error ? g_error_copy (op->error) : NULL;
}

/**
 * seahorse_operation_get_error:
 * @op: A #SeahorseOperation
 *
 * Directly return the error from operation
 *
 * Returns: the #GError error from the operation
 */
const GError*       
seahorse_operation_get_error (SeahorseOperation *op)
{
	g_return_val_if_fail (SEAHORSE_IS_OPERATION (op), NULL);
	return op->error;
}

/**
 * seahorse_operation_display_error:
 * @operation: a #SeahorseOperation
 * @title: The title of the error box
 * @parent: ignored
 *
 * Displays an error box if there is an error in the operation
 *
 */
void
seahorse_operation_display_error (SeahorseOperation *operation, 
                                  const gchar *title, GtkWidget *parent)
{
	g_return_if_fail (SEAHORSE_IS_OPERATION (operation));
	g_return_if_fail (operation->error);
	seahorse_util_handle_error (operation->error, "%s", title);
}

/**
 * seahorse_operation_get_result:
 * @op: a #SeahorseOperation
 *
 *
 *
 * Returns: the results of this operation
 */
gpointer
seahorse_operation_get_result (SeahorseOperation *op)
{
	g_return_val_if_fail (SEAHORSE_IS_OPERATION (op), NULL);
	return op->result;
}

/**
 * seahorse_operation_wait:
 * @op: The operation to wait for
 *
 * Waits till the #SeahorseOperation finishes.
 *
 */
void                
seahorse_operation_wait (SeahorseOperation *op)
{
	g_return_if_fail (SEAHORSE_IS_OPERATION (op));

	g_object_ref (op);
	seahorse_util_wait_until (!seahorse_operation_is_running (op));
	g_object_unref (op);
}

/**
 * seahorse_operation_watch:
 * @operation: The operation to watch
 * @done_callback: callback when done
 * @donedata: data for this callback
 * @progress_callback: Callback when in progress mode
 * @progdata: data for this callback
 *
 * If the operation already finished, calls the done callback. Does progress
 * handling else.
 */
void
seahorse_operation_watch (SeahorseOperation *operation, SeahorseDoneFunc done_callback,
                          gpointer donedata, SeahorseProgressFunc progress_callback, gpointer progdata)
{
    g_return_if_fail (SEAHORSE_IS_OPERATION (operation));

    if (!seahorse_operation_is_running (operation)) {
        if (done_callback)
            (done_callback) (operation, donedata);
        return;
    }
    
    if (done_callback)
        g_signal_connect (operation, "done", G_CALLBACK (done_callback), donedata);
    
    if (progress_callback) {
        (progress_callback) (operation, 
                seahorse_operation_get_message (operation),
                seahorse_operation_get_progress (operation), progdata);
        g_signal_connect (operation, "progress", G_CALLBACK (progress_callback), progdata);
    }
}

/* METHODS FOR DERIVED CLASSES ---------------------------------------------- */

/**
 * seahorse_operation_mark_start:
 * @op: a #SeahorseOperation
 *
 * Sets everything in the seahorse operation to the start state
 *
 */
void
seahorse_operation_mark_start (SeahorseOperation *op)
{
    g_return_if_fail (SEAHORSE_IS_OPERATION (op));
        
    /* A running operation always refs itself */
    g_object_ref (op);
    op->is_running = TRUE;
    op->is_done = FALSE;
    op->is_cancelled = FALSE;
    op->progress = -1;
    
    g_free (op->message);
    op->message = NULL;
}

/**
 * seahorse_operation_mark_progress:
 * @op: A #SeahorseOperation to set
 * @message: The new message of the operation, Can be NULL
 * @progress: the new progress of the operation
 *
 * Sets the new message and the new progress. After a change
 * it emits the progress signal
 */
void 
seahorse_operation_mark_progress (SeahorseOperation *op, const gchar *message, 
                                  gdouble progress)
{
    gboolean emit = FALSE;
    
    g_return_if_fail (SEAHORSE_IS_OPERATION (op));
    g_return_if_fail (op->is_running);

    if (progress != op->progress) {
        op->progress = progress;
        emit = TRUE;
    }

    if (!seahorse_util_string_equals (op->message, message)) {
        g_free (op->message);
        op->message = message ? g_strdup (message) : NULL;
        emit = TRUE;
    }

    if (emit)    
        g_signal_emit (G_OBJECT (op), signals[PROGRESS], 0, op->message, progress);
}

/**
 * seahorse_operation_mark_progress_full:
 * @op: The operation to set the progress for
 * @message: an optional message (can be NULL)
 * @current: the current value of the progress
 * @total: the max. value of the progress
 *
 *
 *
 */
void
seahorse_operation_mark_progress_full (SeahorseOperation *op, const gchar *message,
                                       gdouble current, gdouble total)
{
    if (current > total)
        current = total;
    seahorse_operation_mark_progress (op, message, total <= 0 ? -1 : current / total);
}


/**
 * seahorse_operation_mark_done:
 * @op: a #SeahorseOperation
 * @cancelled: TRUE if this operation was cancelled
 * @error: An error to set
 *
 * Sets everything in the seahorse operation to the end state
 *
 */
void                
seahorse_operation_mark_done (SeahorseOperation *op, gboolean cancelled,
                              GError *error)
{
    g_return_if_fail (SEAHORSE_IS_OPERATION (op));
    g_return_if_fail (op->is_running);

    /* No message on completed operations */
    g_free (op->message);
    op->message = NULL;
    
    op->progress = cancelled ? -1 : 1.0;
    op->is_running = FALSE;
    op->is_done = TRUE;
    op->is_cancelled = cancelled;
    op->error = error;

    g_signal_emit (op, signals[PROGRESS], 0, NULL, op->progress);
    g_signal_emit (op, signals[DONE], 0);

    /* A running operation always refs itself */
    g_object_unref (op);
}

/**
 * seahorse_operation_mark_result:
 * @op: A #SeahorseOperation
 * @result: a result
 * @destroy_func: a destroy function
 *
 * If @op already has a result and a destroy function, the function is called.
 * If there is none, it will be set to @result and @destroy_func
 */
void
seahorse_operation_mark_result (SeahorseOperation *op, gpointer result,
                                GDestroyNotify destroy_func)
{
    if (op->result && op->result_destroy)
        (op->result_destroy) (op->result);
    op->result = result;
    op->result_destroy = destroy_func;
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

G_DEFINE_TYPE (SeahorseMultiOperation, seahorse_multi_operation, SEAHORSE_TYPE_OPERATION);

/* HELPERS ------------------------------------------------------------------ */

/**
* operation: the operation that contains progress and message
* message: the message, will be set by this function
* fract: ignored
* mop: a #SeahorseMultiOperation container
*
* Calculates the combined progress of all the contained operations, sets this
* as progress for @mop
*
**/
static void
multi_operation_progress (SeahorseOperation *operation, const gchar *message, 
                          gdouble fract, SeahorseMultiOperation *mop)
{
    GSList *list;
    
    g_assert (SEAHORSE_IS_MULTI_OPERATION (mop));
    g_assert (SEAHORSE_IS_OPERATION (operation));
    
    if (seahorse_operation_is_cancelled (SEAHORSE_OPERATION (mop)))
        return;
    
    list = mop->operations;
    
    /* One or two operations, simple */
    if (g_slist_length (list) <= 1) {

		seahorse_debug ("[multi-operation %p] single progress: %s %lf", mop,
		                seahorse_operation_get_message (operation), operation->progress);

        seahorse_operation_mark_progress (SEAHORSE_OPERATION (mop), 
                            seahorse_operation_get_message (operation), 
                            operation->progress);


    /* When more than one operation calculate the fraction */
    } else {
    
        SeahorseOperation *op;
        gdouble total = 0;
        gdouble current = 0;
        gdouble progress;
        
        message = seahorse_operation_get_message (operation);

        /* Get the total progress */
        while (list) {
            op = SEAHORSE_OPERATION (list->data);
            list = g_slist_next (list);

            if (message && !message[0])
                message = NULL;
            
            if (!message && seahorse_operation_is_running (op))
                message = seahorse_operation_get_message (op);
            
            if (!seahorse_operation_is_cancelled (op)) {
                total += 1;
                if (op->progress < 0)
                    current += (seahorse_operation_is_running (op) ? 0 : 1);
                else
                    current += op->progress;
                seahorse_debug ("    progres is: %lf", op->progress);
            }
        } 
        
        progress = total ? current / total : -1;

        seahorse_debug ("[multi-operation %p] multi progress: %s %lf", mop,
                          message, progress);
        
        seahorse_operation_mark_progress (SEAHORSE_OPERATION (mop), message, progress);
    }
}

/**
* op: an operation that is done and will be cleaned
* mop: multi operation to process
*
* Sets a multi-operation to done. If one of the sub operations is running it
* just handles the progress.
*
**/
static void
multi_operation_done (SeahorseOperation *op, SeahorseMultiOperation *mop)
{
    GSList *l;
    gboolean done = TRUE;
    
    g_assert (SEAHORSE_IS_MULTI_OPERATION (mop));
    g_assert (SEAHORSE_IS_OPERATION (op));  
  
    g_signal_handlers_disconnect_by_func (op, multi_operation_done, mop);
    g_signal_handlers_disconnect_by_func (op, multi_operation_progress, mop);
    
    if (!seahorse_operation_is_successful (op) && !SEAHORSE_OPERATION (mop)->error)
        seahorse_operation_copy_error (op, &(SEAHORSE_OPERATION (mop)->error));

    seahorse_debug ("[multi-operation %p] part complete (%d): %p/%s", mop,
                    g_slist_length (mop->operations), op, seahorse_operation_get_message (op));
        
    /* Are we done with all of them? */
    for (l = mop->operations; l; l = g_slist_next (l)) {
        if (seahorse_operation_is_running (SEAHORSE_OPERATION (l->data))) 
            done = FALSE;
    }
    
    /* Not done, just update progress. */
    if (!done) {
        multi_operation_progress (SEAHORSE_OPERATION (mop), NULL, -1, mop);
        return;
    }

    seahorse_debug ("[multi-operation %p] complete", mop);

    /* Remove all the listeners */
    for (l = mop->operations; l; l = g_slist_next (l)) {
        g_signal_handlers_disconnect_by_func (l->data, multi_operation_done, mop);
        g_signal_handlers_disconnect_by_func (l->data, multi_operation_progress, mop);
    }

    mop->operations = seahorse_operation_list_purge (mop->operations);
    
    if (!seahorse_operation_is_cancelled (SEAHORSE_OPERATION (mop))) 
        seahorse_operation_mark_done (SEAHORSE_OPERATION (mop), FALSE, 
                                      SEAHORSE_OPERATION (mop)->error);
}

/* OBJECT ------------------------------------------------------------------- */

/**
* mop: The #SeahorseMultiOperation object to initialise
*
* Initializes @mop
*
**/
static void
seahorse_multi_operation_init (SeahorseMultiOperation *mop)
{
    mop->operations = NULL;
}

/**
* gobject: A #SEAHORSE_MULTI_OPERATION
*
* dispose of all our internal references, frees the stored operations
*
**/
static void
seahorse_multi_operation_dispose (GObject *gobject)
{
    SeahorseMultiOperation *mop;
    GSList *l;
    
    mop = SEAHORSE_MULTI_OPERATION (gobject);
    
    /* Remove all the listeners */
    for (l = mop->operations; l; l = g_slist_next (l)) {
        g_signal_handlers_disconnect_by_func (l->data, multi_operation_done, mop);
        g_signal_handlers_disconnect_by_func (l->data, multi_operation_progress, mop);
    }
    
    /* Anything remaining, gets released  */
    mop->operations = seahorse_operation_list_free (mop->operations);
        
    G_OBJECT_CLASS (seahorse_multi_operation_parent_class)->dispose (gobject);
}


/**
* gobject: a #SEAHORSE_MULTI_OPERATION
*
*
* finalizes the multi operation
*
**/
static void
seahorse_multi_operation_finalize (GObject *gobject)

{
    SeahorseMultiOperation *mop;
    mop = SEAHORSE_MULTI_OPERATION (gobject);
    
    g_assert (mop->operations == NULL);
    
    G_OBJECT_CLASS (seahorse_multi_operation_parent_class)->finalize (gobject);
}

/**
* operation: A #SeahorseMultiOperation
*
* Cancels a multi operation and all the operations within
*
**/
static void 
seahorse_multi_operation_cancel (SeahorseOperation *operation)
{
    SeahorseMultiOperation *mop;
    
    g_assert (SEAHORSE_IS_MULTI_OPERATION (operation));
    mop = SEAHORSE_MULTI_OPERATION (operation);

    seahorse_operation_mark_done (operation, TRUE, NULL);

    seahorse_operation_list_cancel (mop->operations);
    mop->operations = seahorse_operation_list_purge (mop->operations);
}

/**
* klass: The Class to init
*
* Initialises the multi-operation class. This is a container for several
* operations
*
**/
static void
seahorse_multi_operation_class_init (SeahorseMultiOperationClass *klass)
{
    SeahorseOperationClass *op_class = SEAHORSE_OPERATION_CLASS (klass);
    GObjectClass *gobject_class;

    seahorse_multi_operation_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    op_class->cancel = seahorse_multi_operation_cancel;        
    gobject_class->dispose = seahorse_multi_operation_dispose;
    gobject_class->finalize = seahorse_multi_operation_finalize;
}

/* PUBLIC ------------------------------------------------------------------- */

/**
 * seahorse_multi_operation_new:
 *
 * Creates a new multi operation
 *
 * Returns: the new multi operation
 */
SeahorseMultiOperation*  
seahorse_multi_operation_new ()
{
    SeahorseMultiOperation *mop = g_object_new (SEAHORSE_TYPE_MULTI_OPERATION, NULL); 
    return mop;
}

/**
 * seahorse_multi_operation_take:
 * @mop: the multi operation container
 * @op: an operation to add
 *
 * Adds @op to @mop
 *
 */
void
seahorse_multi_operation_take (SeahorseMultiOperation* mop, SeahorseOperation *op)
{
    g_return_if_fail (SEAHORSE_IS_MULTI_OPERATION (mop));
    g_return_if_fail (SEAHORSE_IS_OPERATION (op));
    
    /* We should never add an operation to itself */
    g_return_if_fail (SEAHORSE_OPERATION (op) != SEAHORSE_OPERATION (mop));
    
    if(mop->operations == NULL) {
        seahorse_debug ("[multi-operation %p] start", mop);
        seahorse_operation_mark_start (SEAHORSE_OPERATION (mop));
    }
    
    seahorse_debug ("[multi-operation %p] adding part: %p", mop, op);
    
    mop->operations = seahorse_operation_list_add (mop->operations, op);
    seahorse_operation_watch (op, (SeahorseDoneFunc)multi_operation_done, mop,
                              (SeahorseProgressFunc)multi_operation_progress, mop);
}

/* -----------------------------------------------------------------------------
 * OPERATION LIST FUNCTIONS
 */

/**
 * seahorse_operation_list_add:
 * @list: a #GSList
 * @operation: a #SeahorseOperation to add to the lit
 *
 * Prepends the seahorse operation to the list
 *
 * Returns: the list
 */
GSList*
seahorse_operation_list_add (GSList *list, SeahorseOperation *operation)
{
    /* This assumes the main reference */
    return g_slist_prepend (list, operation);
}

/**
 * seahorse_operation_list_remove:
 * @list: A list to remove an operation from
 * @operation: the operation to remove
 *
 * Removes an operation from the list
 *
 * Returns: The new list
 */
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

/**
 * seahorse_operation_list_cancel:
 * @list: a list of Seahorse operations
 *
 * Cancels every operation in the list
 *
 */
void             
seahorse_operation_list_cancel (GSList *list)
{
    SeahorseOperation *operation;
    
    while (list) {
        operation = SEAHORSE_OPERATION (list->data);
        list = g_slist_next (list);
        
        if (seahorse_operation_is_running (operation))
            seahorse_operation_cancel (operation);
    }
}

/**
 * seahorse_operation_list_purge:
 * @list: A list of operations
 *
 * Purges a list of Seahorse operations
 *
 * Returns: the purged list
 */
GSList*             
seahorse_operation_list_purge (GSList *list)
{
    GSList *l, *p;
    
    p = list;
    
    while (p != NULL) {

        l = p;
        p = g_slist_next (p);
        
        if (!seahorse_operation_is_running (SEAHORSE_OPERATION (l->data))) {
            g_object_unref (G_OBJECT (l->data));

            list = g_slist_remove_link (list, l);
            g_slist_free (l);
        }
    }
    
    return list;
}

/**
 * seahorse_operation_list_free:
 * @list: A #GSList of #SEAHORSE_OPERATION s
 *
 * Frees the list of seahorse operations
 *
 * Returns: NULL
 */
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
